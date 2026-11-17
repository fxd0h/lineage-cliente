#!/usr/bin/env python3 -I
"""Replace the Chinese item names (and empty NPC name ids) in L1JGO-Whale's data/yaml by client-side
`$N` description ids (resolved by desc-e.tbl), so an English client shows English names.

Source: the l1j-jp Taiwan CSV tables (servidor_local/l1j-jp/db/csv/tw), which carry identified_name_id
for the same item ids. A Whale entry is matched by id when the Chinese name or inv_gfx agrees, else by
an unambiguous Chinese name. Plain (non-$) ids fall back to the English name in name_fixes.tsv.
Unmatched entries are left untouched and counted. Run on pristine yaml (instalar.sh restores it first).

Usage: translate_names.py <whale data/yaml dir> <csv/tw dir> <name_fixes.tsv>
"""
import csv
import re
import sys

yamldir, csvdir, fixes = sys.argv[1:4]

def rows(tbl):
    with open(f"{csvdir}/{tbl}.csv", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))

english = {}  # (table, id) -> English plain name
with open(fixes, encoding="utf-8") as f:
    for r in csv.DictReader(f, delimiter="\t"):
        if r["column"] == "name":
            english[(r["table"], int(r["id"]))] = r["new"]

def display(r, table):
    ident = r["identified_name_id"]
    if ident.startswith("$"):
        return ident
    return english.get((table, int(r["id"]))) or (ident or None)

def translate_items(fname, itemkey, tables):
    byid, byname = {}, {}
    for t in tables:
        for r in rows(t):
            byid[int(r["id"])] = (t, r)
            byname.setdefault(r["name"], []).append((t, r))
    path = f"{yamldir}/{fname}.yaml"
    lines = open(path, encoding="utf-8").read().split("\n")
    out, i, done, miss = [], 0, 0, 0
    cur_id = cur_gfx = None
    blocks = []  # (name line index, id, name)
    for n, ln in enumerate(lines):
        m = re.match(r"\s+- item_id: (\d+)", ln)
        if m:
            cur_id = int(m.group(1)); blocks.append([None, cur_id, None, None])
        elif blocks and blocks[-1][0] is None and (m := re.match(r"(\s+name: )(.*)$", ln)):
            blocks[-1][0], blocks[-1][2] = n, m.group(2).strip()
        elif blocks and (m := re.match(r"\s+inv_gfx: (\d+)", ln)):
            blocks[-1][3] = int(m.group(1))
    for n, iid, name, gfx in blocks:
        hit = byid.get(iid)
        if hit and hit[1]["name"] != name and int(hit[1]["inv_gfx_id"]) != gfx:
            hit = None
        if not hit and len(byname.get(name, [])) >= 1:
            c = byname[name]
            if len({x[1]["identified_name_id"] for x in c}) == 1:
                hit = c[0]
        new = display(hit[1], hit[0]) if hit else None
        if new:
            lines[n] = re.sub(r"name: .*$", "name: " + ("'%s'" % new.replace("'", "''")), lines[n]); done += 1
        else:
            miss += 1
    open(path, "w", encoding="utf-8").write("\n".join(lines))
    print(f"{fname}: {done} translated, {miss} left")

translate_items("etcitem_list", "items", ["etc_items"])
translate_items("weapon_list", "weapons", ["weapons"])
translate_items("armor_list", "armors", ["armors"])

# NPCs with an empty nameid: take it from the tw table (same id and gfx)
npcs = {int(r["id"]): r for r in rows("npcs")}
path = f"{yamldir}/npc_list.yaml"
lines = open(path, encoding="utf-8").read().split("\n")
cur = gfx = None; done = empty = 0
for n, ln in enumerate(lines):
    if (m := re.match(r"\s+- npc_id: (\d+)", ln)):
        cur = int(m.group(1)); gfx = None
    elif (m := re.match(r"\s+gfx_id: (\d+)", ln)):
        gfx = int(m.group(1))
    elif (m := re.match(r"(\s+nameid: )''\s*$", ln)):
        empty += 1
        r = npcs.get(cur)
        if r and r["name_id"].startswith("$"):
            lines[n] = m.group(1) + "'" + r["name_id"] + "'"; done += 1
open(path, "w", encoding="utf-8").write("\n".join(lines))
print(f"npc_list: {empty} empty nameid, {done} filled")

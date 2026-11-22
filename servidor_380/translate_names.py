#!/usr/bin/env python3 -I
"""Replace the Chinese item names (and empty NPC name ids) in L1JGO-Whale's data/yaml by client-side
`$N` description ids (resolved by desc-e.tbl), so an English client shows English names.

Source: the l1j-jp Taiwan CSV tables (servidor_local/l1j-jp/db/csv/tw), which carry identified_name_id
for the same item ids. A Whale entry is matched by id when the Chinese name or inv_gfx agrees, else by
an unambiguous Chinese name. Plain (non-$) ids fall back to the English name in name_fixes.tsv.
Unmatched entries are left untouched and counted. Run on pristine yaml (instalar.sh restores it first).

names_en.tsv (kind, id, chinese, english, source) is the reviewable result: source `mapped` rows are
regenerated, `translated` rows are hand/agent translations (US Lineage naming) and are kept. Rows with an
empty english column still need a translation.

Usage:
  translate_names.py generate <whale data/yaml dir> <csv/tw dir> <name_fixes.tsv> <names_en.tsv>
  translate_names.py apply    <whale data/yaml dir> <names_en.tsv>
"""
import csv
import re
import sys


mode = sys.argv[1]

def rows(tbl):
    with open(f"{csvdir}/{tbl}.csv", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))

def display(r, table, english):
    ident = r["identified_name_id"]
    if ident.startswith("$"):
        return ident
    return english.get((table, int(r["id"]))) or ident or None

def blocks_of(lines, idre):
    """[(name line, id, name, gfx)] for each yaml entry."""
    out = []
    for n, ln in enumerate(lines):
        m = re.match(idre, ln)
        if m:
            out.append([None, int(m.group(1)), None, None])
        elif out and out[-1][0] is None and (m := re.match(r"(\s+name: )(.*)$", ln)):
            out[-1][0], out[-1][2] = n, m.group(2).strip().strip("'\"")
        elif out and (m := re.match(r"\s+(?:inv_)?gfx(?:_id)?: (\d+)", ln)):
            out[-1][3] = int(m.group(1))
    return out

CJK = re.compile("[\u3400-\u9fff]")
FILES = [("etcitem_list", "item_id", ["etc_items"]), ("weapon_list", "item_id", ["weapons"]),
         ("armor_list", "item_id", ["armors"]), ("npc_list", "npc_id", [])]

if mode == "generate":
    yamldir, csvdir, fixes, outtsv = sys.argv[2:6]
    english = {}
    with open(fixes, encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["column"] == "name":
                english[(r["table"], int(r["id"]))] = r["new"]
    kept = {}
    try:
        with open(outtsv, encoding="utf-8", newline="") as f:
            for r in csv.DictReader(f, delimiter="\t"):
                if r["source"] == "translated" and r["english"]:
                    kept[(r["kind"], r["chinese"])] = r["english"]
    except FileNotFoundError:
        pass
    result = []
    for fname, idre, tables in FILES:
        lines = open(f"{yamldir}/{fname}.yaml", encoding="utf-8").read().split("\n")
        byid, byname = {}, {}
        for t in tables:
            for r in rows(t):
                byid[int(r["id"])] = (t, r)
                byname.setdefault(r["name"], []).append((t, r))
        kind = "npc" if fname == "npc_list" else "item"
        for n, iid, name, gfx in blocks_of(lines, r"\s+- " + idre + r": (\d+)"):
            if name is None or not CJK.search(name):
                continue
            if kind == "npc":
                # only entries whose client name is empty need a name
                nl = next((l for l in lines[n:n + 6] if "nameid:" in l), "")
                if not re.search(r"nameid: ''", nl) or not CJK.search(name):
                    continue
                result.append((kind, iid, name, kept.get((kind, name), ""), "translated"))
                continue
            hit = byid.get(iid)
            if hit and hit[1]["name"] != name and int(hit[1]["inv_gfx_id"]) != gfx:
                hit = None
            if not hit and name in byname and len({x[1]["identified_name_id"] for x in byname[name]}) == 1:
                hit = byname[name][0]
            new = display(hit[1], hit[0], english) if hit else None
            if new and not CJK.search(new):
                result.append((kind, iid, name, new, "mapped"))
            else:
                result.append((kind, iid, name, kept.get((kind, name), ""), "translated"))
    with open(outtsv, "w", encoding="utf-8", newline="") as f:
        f.write("kind\tid\tchinese\tenglish\tsource\n")
        for r in result:
            f.write("\t".join(map(str, r)) + "\n")
    print(f"{len(result)} rows: mapped {sum(r[4]=='mapped' for r in result)}, "
          f"translated {sum(r[4]=='translated' and bool(r[3]) for r in result)}, "
          f"pending {sum(not r[3] for r in result)}")

elif mode == "apply":
    yamldir, tsv = sys.argv[2:4]
    table = {}
    with open(tsv, encoding="utf-8", newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["english"]:
                table[(r["kind"], int(r["id"]))] = r["english"]
    for fname, idre, _ in FILES:
        path = f"{yamldir}/{fname}.yaml"
        lines = open(path, encoding="utf-8").read().split("\n")
        kind = "npc" if fname == "npc_list" else "item"
        done = 0
        for n, iid, name, gfx in blocks_of(lines, r"\s+- " + idre + r": (\d+)"):
            eng = table.get((kind, iid))
            if eng is None or name is None:
                continue
            q = "'" + eng.replace("'", "''") + "'"
            if kind == "npc":
                for k in range(n, min(n + 6, len(lines))):
                    if re.match(r"\s+nameid: ''", lines[k]):
                        lines[k] = re.sub(r"''", q, lines[k], count=1); done += 1
                        break
            else:
                lines[n] = re.sub(r"name: .*$", "name: " + q, lines[n]); done += 1
        open(path, "w", encoding="utf-8").write("\n".join(lines))
        print(f"{fname}: {done} applied")

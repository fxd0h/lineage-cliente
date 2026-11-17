#!/usr/bin/env python3 -I
"""Remove from L1JGO-Whale's data/yaml everything the game data cannot show or the official 3.81C does not have.

prune_ids.tsv (kind<TAB>id<TAB>reason) is the reviewable, committed list; `apply` (run by instalar.sh on the
pristine, translated yaml) removes it reproducibly without needing the game files:
  npc   spawned monsters/NPCs whose gfx is in no data source: their spawn_list / light_spawn_list entries and drop_list tables
  poly  polymorph entries whose gfx is in no source
  doll  magic dolls whose gfx is in no source (their item is removed too)
  item  items with no inventory icon / ground gfx in any source (reason `gfx`), and private-server-only items
        (reason `private`: not mapped to the official TW tables and absent from the official 3.81C desc-c.tbl);
        their entries are also removed from every list that names them by `item_id` (drops, shops, recipes, item_power...)
Usage:
  prune_data.py generate <yaml dir> <names_en.tsv> <desc-c.tbl (3.81C, decrypted)> <out.tsv> <game folder>...
  prune_data.py apply    <yaml dir> <prune_ids.tsv>
"""
import os, re, sys

def generate(y, names_tsv, desc_c, out, folders):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import gfx_sources
    import io, contextlib
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        gfx_sources.missing(y, folders)
    rows = [l.split('\t') for l in buf.getvalue().splitlines()]
    official = {l.strip() for l in open(desc_c, encoding='big5', errors='replace')}
    have = {(r[0], r[1]) for r in rows}
    for l in list(open(names_tsv, encoding='utf-8'))[1:]:
        k, i, zh, en, src = l.rstrip('\n').split('\t')[:5]
        if k == 'item' and src == 'translated' and zh not in official and ('item', i) not in have:
            rows.append(['item', i, 'private: not in the official TW tables or 3.81C desc-c.tbl'])
    # a doll without gfx takes its item along
    for r in list(rows):
        if r[0] == 'doll' and ('item', r[1]) not in have:
            rows.append(['item', r[1], 'gfx: doll ' + r[2]])
    seen, outrows = set(), []
    for r in rows:
        if (r[0], r[1]) in seen: continue
        seen.add((r[0], r[1]))
        outrows.append(r)
    outrows.sort(key=lambda r: (r[0], int(r[1])))
    with open(out, 'w', encoding='utf-8') as f:
        f.write('kind\tid\treason\n')
        for r in outrows:
            f.write('\t'.join(r) + '\n')
    print({k: sum(1 for r in outrows if r[0] == k) for k in ('npc', 'poly', 'doll', 'item')})

def remove_blocks(path, key, ids):
    lines = open(path, encoding='utf-8-sig', newline='').read().split('\n')
    bom = open(path, 'rb').read(3) == b'\xef\xbb\xbf'
    pat = re.compile(r'^(\s*)- (?:\{ )?' + key + r': (\d+)\b')
    out, i, n = [], 0, 0
    while i < len(lines):
        m = pat.match(lines[i])
        if m and int(m[2]) in ids:
            ind = len(m[1])
            i += 1
            while i < len(lines) and (not lines[i].strip() or len(lines[i]) - len(lines[i].lstrip()) > ind):
                if not lines[i].strip():  # blank line: block ends unless more indented content follows
                    j = i
                    while j < len(lines) and not lines[j].strip(): j += 1
                    if j >= len(lines) or len(lines[j]) - len(lines[j].lstrip()) <= ind: break
                i += 1
            n += 1
            continue
        out.append(lines[i]); i += 1
    if n:
        with open(path, 'w', encoding='utf-8', newline='') as f:
            f.write(('﻿' if bom else '') + '\n'.join(out))
    return n

def apply(y, tsv):
    ids = {'npc': set(), 'poly': set(), 'item': set()}
    for l in list(open(tsv, encoding='utf-8'))[1:]:
        k, i = l.split('\t')[:2]
        ids['item' if k == 'doll' else k].add(int(i))
    tot = {'npc': 0, 'poly': 0, 'item': 0}
    for fn in sorted(os.listdir(y)):
        if not fn.endswith('.yaml'): continue
        p = os.path.join(y, fn)
        if fn in ('spawn_list.yaml', 'light_spawn_list.yaml'): tot['npc'] += remove_blocks(p, 'npc_id', ids['npc'])
        if fn == 'drop_list.yaml': tot['npc'] += remove_blocks(p, 'mob_id', ids['npc'])
        if fn == 'polymorph_list.yaml': tot['poly'] += remove_blocks(p, 'poly_id', ids['poly'])
        if fn != 'npc_list.yaml': tot['item'] += remove_blocks(p, 'item_id', ids['item'])
    print('pruned entries:', tot)

if __name__ == '__main__':
    if sys.argv[1] == 'generate': generate(*sys.argv[2:6], sys.argv[6:])
    else: apply(*sys.argv[2:4])

#!/usr/bin/env python3 -I
"""Which gfx / icons / maps of the server data exist in the game data sources (juego= + juego_extra= folders).

Read-only on the game folders (untrusted data: only the plain .idx indexes and directory listings are read, nothing
is executed). Usage:
  gfx_sources.py report  <yaml_dir> <folder>...      coverage numbers (primary first, extras after)
  gfx_sources.py missing <yaml_dir> <folder>...      write prune_ids.tsv rows (kind<TAB>id<TAB>reason) to stdout
"""
import os, re, struct, sys
import yaml

def load(path):
    with open(path, encoding='utf-8-sig') as f:
        return yaml.load(f, Loader=yaml.CSafeLoader)

def idx_names(base):
    """Names of a plain .idx (u32 count + count x {u32 offset, char[20] name, u32 size})."""
    try:
        d = open(base + '.idx', 'rb').read()
    except OSError:
        return []
    n = struct.unpack_from('<I', d, 0)[0]
    out = []
    for i in range(min(n, (len(d) - 4) // 28)):
        out.append(d[8 + i * 28: 28 + i * 28].split(b'\0')[0].decode('latin1').lower())
    return out

def ci_dir(folder, name):
    try:
        for e in os.listdir(folder):
            if e.lower() == name.lower():
                return os.path.join(folder, e)
    except OSError:
        pass
    return None

class Sources:
    def __init__(self, folders):
        self.gfx, self.icons, self.maps, self.listed = set(), set(), set(), set()
        for f in folders:
            names = []
            for fn in os.listdir(f) if os.path.isdir(f) else []:
                m = re.fullmatch(r'(sprite(\d\d)?)\.idx', fn, re.I)
                if m:
                    names += idx_names(os.path.join(f, fn[:-4]))
            sd = ci_dir(f, 'sprite')
            if sd:
                names += [e.lower() for e in os.listdir(sd)]
            for n in names:
                m = re.fullmatch(r'(\d+)-(\d+)\.spr', n)
                if m: self.gfx.add(int(m[1]))
                m = re.fullmatch(r'(\d+)\.tbt', n)
                if m: self.icons.add(int(m[1]))
            md = ci_dir(f, 'map')
            if md:
                self.maps |= {int(e) for e in os.listdir(md) if e.isdigit()}
            # plaintext sprite lists (list.spz, spritelist/list.spz, top-level *.txt with a list header)
            cands = [os.path.join(f, 'list.spz'), os.path.join(f, 'spritelist', 'list.spz')]
            cands += [os.path.join(f, e) for e in os.listdir(f) if e.lower().endswith('.txt')] if os.path.isdir(f) else []
            for c in cands:
                try:
                    head = open(c, 'rb').readline()
                except OSError:
                    continue
                if not re.match(rb'S?\d+ \d+ \d+\s*$', head):
                    continue
                for line in open(c, 'rb'):
                    m = re.match(rb'#\d+\s+\d+=(\d+)', line)
                    if m: self.listed.add(int(m[1]))

    def has_gfx(self, g):
        return g in self.gfx

def server_needs(y):
    npcs = {n['npc_id']: n for n in load(f'{y}/npc_list.yaml')['npcs']}
    spawned = set()
    for s in load(f'{y}/spawn_list.yaml')['spawns']: spawned.add(s['npc_id'])
    for s in load(f'{y}/light_spawn_list.yaml'): spawned.add(s['npc_id'])
    items = {}
    for fn, key in (('armor_list', 'armors'), ('weapon_list', 'weapons'), ('etcitem_list', 'items')):
        d = load(f'{y}/{fn}.yaml')
        lst = d[key] if isinstance(d, dict) and key in d else next(iter(d.values()))
        for it in lst: items[it['item_id']] = it
    polys = [p['poly_id'] for p in load(f'{y}/polymorph_list.yaml')['polymorphs']]
    dolls = {d['item_id']: d['gfx_id'] for d in load(f'{y}/dolls.yaml')['dolls']}
    doors = [d['gfxid'] for d in load(f'{y}/door_gfx.yaml')['door_gfxs']]
    return npcs, spawned, items, polys, dolls, doors

def report(y, folders):
    npcs, spawned, items, polys, dolls, doors = server_needs(y)
    per = [Sources([f]) for f in folders]
    allsrc = Sources(folders)
    def row(name, need, fn):
        need = set(need)
        cols = [sum(1 for g in need if fn(s, g)) for s in per]
        tot = sum(1 for g in need if fn(allsrc, g))
        print(f'| {name} | {len(need)} | ' + ' | '.join(map(str, cols)) + f' | {tot} | {len(need) - tot} |')
    hdr = ' | '.join(f'in {chr(65 + i)}' for i in range(len(folders)))
    print(f'| Category | needed | {hdr} | any | in none |\n|---|---|' + '---|' * (len(folders) + 2))
    g = lambda s, x: s.has_gfx(x)
    row('NPC gfx (npc_list)', [n['gfx_id'] for n in npcs.values()], g)
    row('NPC gfx (spawned)', [npcs[i]['gfx_id'] for i in spawned if i in npcs], g)
    row('Polymorph gfx', polys, g)
    row('Doll gfx', dolls.values(), g)
    row('Door gfx', doors, g)
    row('Item ground gfx', [i['grd_gfx'] for i in items.values() if 'grd_gfx' in i], g)
    row('Inventory icons', [i['inv_gfx'] for i in items.values() if 'inv_gfx' in i], lambda s, x: x in s.icons)
    maps = {s['map_id'] for s in load(f'{y}/spawn_list.yaml')['spawns']}
    row('Maps with spawns', maps, lambda s, x: x in s.maps)

def missing(y, folders):
    npcs, spawned, items, polys, dolls, doors = server_needs(y)
    s = Sources(folders)
    for i in sorted(spawned):
        if i in npcs and not s.has_gfx(npcs[i]['gfx_id']):
            print(f"npc\t{i}\tgfx {npcs[i]['gfx_id']} in no source")
    for p in sorted(set(polys)):
        if not s.has_gfx(p): print(f'poly\t{p}\tgfx {p} in no source')
    for iid, it in sorted(items.items()):
        if 'inv_gfx' in it and it['inv_gfx'] not in s.icons:
            print(f"item\t{iid}\ticon {it['inv_gfx']} in no source")
        elif 'grd_gfx' in it and not s.has_gfx(it['grd_gfx']):
            print(f"item\t{iid}\tground gfx {it['grd_gfx']} in no source")
    for iid, g in sorted(dolls.items()):
        if not s.has_gfx(g): print(f'doll\t{iid}\tgfx {g} in no source')

if __name__ == '__main__':
    cmd, y, folders = sys.argv[1], sys.argv[2], sys.argv[3:]
    {'report': report, 'missing': missing}[cmd](y, folders)

#!/usr/bin/env python3 -I
"""Copy the user's character `fx` (knight, level 92) from the old jp363 MariaDB into the Whale PostgreSQL.

The old database is only read (SELECT through its socket); nothing is logged in. Creates account fx / password fx
(access 200) and character fx with the same stats, level, exp, skills, adena and every inventory item with its
enchant, equipped state and identify flag. Idempotent: an existing fx character is replaced.
Usage: migrar_fx.py [old char id (default 268479040)]
"""
import csv, io, json, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OLD_SOCK = os.path.join(HERE, '..', 'servidor_local', 'datos', 'mysql.sock')
PG = ['/usr/lib/postgresql/16/bin/psql', '-h', '127.0.0.1', '-p', '5433', '-U', 'postgres', 'l1jgo', '-v', 'ON_ERROR_STOP=1']
OLD_ID = int(sys.argv[1]) if len(sys.argv) > 1 else 268479040
# jp363 ids that differ in the TW 3.81C data (matched by English name); None = no equivalent in Whale's data
ID_MAP = {41477: 49302, 41480: 49305, 41481: 49306,  # Greater Haste / Bravery / Mana potion
          47041: 40186,                               # Create Zombie spellbook
          21248: None, 21249: None}                   # Royal Valor Necklace / Royal Earring (jp only)
FX_HASH = '$2a$10$bWXY8H4gsG51NeqJK58jZ.aPtGRDctvnkW1UwPeAc4IVFM/wWxVV.'  # bcrypt of the password "fx" (local test server only)
SLOT = {'helm': 1, 'armor': 2, 'glove': 3, 'boots': 4, 'shield': 5, 'cloak': 6, 'amulet': 9, 'belt': 10, 'weapon': 11,
        'earring': 12, 'guarder': 13, 'T': 14}

def old(sql):
    out = subprocess.run(['mariadb', '-S', OLD_SOCK, '-u', 'root', 'l1jdb_363', '-B', '-e', sql],
                         check=True, capture_output=True, text=True).stdout
    return list(csv.DictReader(io.StringIO(out), delimiter='\t'))

def q(v):
    return "'" + str(v).replace("'", "''") + "'"

c = old(f"SELECT * FROM characters WHERE id={OLD_ID}")[0]
items = old(f"SELECT * FROM inventory_items WHERE owner_id={OLD_ID} ORDER BY id")
skills = [int(r['skill_id']) for r in old(f"SELECT skill_id FROM character_skills WHERE char_id={OLD_ID} ORDER BY skill_id")]

# item slots/types from Whale's yaml (equip slot of the equipped ones)
sys.path.insert(0, HERE)
import yaml
Y = os.path.join(HERE, 'l1jgo', 'data', 'yaml')
def load(fn, key):
    d = yaml.load(open(os.path.join(Y, fn), encoding='utf-8-sig'), Loader=yaml.CSafeLoader)
    return d[key] if key in d else next(iter(d.values()))
kind = {}
for it in load('armor_list.yaml', 'armors'): kind[it['item_id']] = ('armor', it.get('type') or it.get('armor_type'))
for it in load('weapon_list.yaml', 'weapons'): kind[it['item_id']] = ('weapon', 'weapon')
for it in load('etcitem_list.yaml', 'items'): kind[it['item_id']] = ('etc', None)

sql = ["BEGIN;",
       "DELETE FROM character_items WHERE char_id IN (SELECT id FROM characters WHERE name='fx');",
       "DELETE FROM characters WHERE name='fx';",
       "DELETE FROM accounts WHERE name='fx';",
       "INSERT INTO accounts (name, password_hash, access_level) VALUES ('fx', %s, 200);" % q(FX_HASH),
       "INSERT INTO characters (account_name, name, class_type, sex, class_id, str, dex, con, wis, cha, intel, level, exp, hp, mp, max_hp, max_mp, ac, x, y, map_id, heading, lawful, title, bonus_stats, elixir_stats, food, high_level, access_level, known_spells, birthday, original_str, original_con) VALUES "
       "('fx','fx',%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,10,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,200,%s,%s,%s,%s);" % tuple(
           map(str, [c['type'], c['sex'], c['class'], c['str'], c['dex'], c['con'], c['wis'], c['cha'], c['int'], c['level'], c['exp'],
                     c['max_hp'], c['max_mp'], c['max_hp'], c['max_mp'], c['loc_x'], c['loc_y'], c['map_id'], c['heading'], c['lawful'],
                     q(c['title']), c['bonus_status'], c['elixir_status'], c['food'], c['high_level'], q(json.dumps(skills)),
                     c['birthday'][:10].replace('-', ''), c['original_str'], c['original_con']]))]
used = {}
skipped = []
for it in items:
    iid = int(it['item_id'])
    iid = ID_MAP.get(iid, iid)
    if iid is None or iid not in kind:
        skipped.append(it['item_id']); continue
    slot = 0
    if it['is_equipped'] == '1':
        k, t = kind[iid]
        if k == 'weapon': slot = 11
        elif t in ('ring',): slot = 7 if used.get('ring', 0) == 0 else 8; used['ring'] = used.get('ring', 0) + 1
        else: slot = SLOT.get(t, 0)
        if slot == 0: print('warning: no slot for equipped', iid, t, file=sys.stderr)
    sql.append("INSERT INTO character_items (char_id, item_id, count, enchant_lvl, bless, equipped, identified, equip_slot, obj_id, attr_enchant_kind, attr_enchant_level) "
               "SELECT id, %d, %s, %s, 1, %s, %s, %d, 0, %s, %s FROM characters WHERE name='fx';" % (
                   iid, it['item_count'], it['enchant_level'], 'true' if slot else 'false', 'true' if it['is_identified'] == '1' else 'false',
                   slot, it['attr_enchant_kind'], it['attr_enchant_level']))
sql.append("COMMIT;")
subprocess.run(PG, input='\n'.join(sql), text=True, check=True, capture_output=True)
print('migrated fx: level', c['level'], '%d items, %d skills; skipped (no equivalent): %s' % (len(items) - len(skipped), len(skills), skipped))

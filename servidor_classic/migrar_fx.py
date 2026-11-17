#!/usr/bin/env python3 -I
"""Copy the user's character `fx` (knight, level 92) from the old jp363 MariaDB into the l1j-en classic database.

The old database is only read (SELECT through its socket); nothing is logged in. Creates account fx / password fx
(access 200, the old SHA1+Base64 hash, which l1j-en upgrades on first login) and character fx with the same stats,
level, exp, skills and every inventory item with enchant, equipped state and identify flag. Item ids are kept when
the same id has the same name in l1j-en, else matched by unique name; the rest is reported. Idempotent.
Usage: migrar_fx.py [old char id (default 268479040)]
"""
import csv, io, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OLD = ['mariadb', '-S', os.path.join(HERE, '..', 'servidor_local', 'datos', 'mysql.sock'), '-u', 'root', 'l1jdb_363', '-B']
NEW = ['mariadb', '-S', os.path.join(HERE, 'datos', 'my.sock'), '-u', 'root', 'l1jdb', '-B']
OLD_ID = int(sys.argv[1]) if len(sys.argv) > 1 else 268479040

def run(cmd, sql):
    out = subprocess.run(cmd + ['-e', sql], check=True, capture_output=True, text=True).stdout
    return list(csv.DictReader(io.StringIO(out), delimiter='\t', quoting=csv.QUOTE_NONE))

def q(v):
    return "NULL" if v in (None, 'NULL') else "'" + str(v).replace("\\", "\\\\").replace("'", "''") + "'"

c = run(OLD, f"SELECT * FROM characters WHERE id={OLD_ID}")[0]
acc = run(OLD, f"SELECT * FROM accounts WHERE id={c['account_id']}")[0]
items = run(OLD, f"SELECT i.*, COALESCE(w.name,a.name,e.name,'') AS nm FROM inventory_items i "
                 f"LEFT JOIN weapons w ON w.id=i.item_id LEFT JOIN armors a ON a.id=i.item_id "
                 f"LEFT JOIN etc_items e ON e.id=i.item_id WHERE owner_id={OLD_ID} ORDER BY i.id")
skills = run(OLD, f"SELECT skill_id FROM character_skills WHERE char_id={OLD_ID} ORDER BY skill_id")

names = {}
for t in ('weapon', 'armor', 'etcitem'):
    for r in run(NEW, f"SELECT item_id, name FROM {t}"):
        names[int(r['item_id'])] = r['name']
by_name = {}
for i, n in names.items():
    by_name.setdefault(n.lower(), []).append(i)

sql = ["SET FOREIGN_KEY_CHECKS=0;", "START TRANSACTION;",
       f"DELETE FROM character_items WHERE char_id={OLD_ID};", f"DELETE FROM character_skills WHERE char_obj_id={OLD_ID};",
       f"DELETE FROM characters WHERE objid={OLD_ID} OR char_name='fx';", "DELETE FROM accounts WHERE login='fx';",
       "INSERT INTO accounts (login,password,lastactive,access_level,ip,host,banned,character_slot) VALUES ('fx',%s,NOW(),1,'127.0.0.1','127.0.0.1',0,0);" % q(acc['password']),
       "INSERT INTO characters (account_name,objid,char_name,level,Exp,MaxHp,CurHp,MaxMp,CurMp,Ac,Str,Con,Dex,Cha,Intel,Wis,Status,Class,Sex,Type,Heading,LocX,LocY,MapID,Food,Lawful,Title,BonusStatus,ElixirStatus,accesslevel,HighLevel,OriginalStr,OriginalCon,OriginalDex,OriginalCha,OriginalInt,OriginalWis) VALUES "
       "('fx',%s,'fx',%s);" % (OLD_ID, ','.join(map(q, [c['level'], c['exp'], c['max_hp'], c['max_hp'], c['max_mp'], c['max_mp'], c['ac'], c['str'], c['con'], c['dex'], c['cha'], c['int'], c['wis'],
                                                  c['status'], c['class'], c['sex'], c['type'], c['heading'], c['loc_x'], c['loc_y'], c['map_id'], c['food'], c['lawful'], c['title'],
                                                  c['bonus_status'], c['elixir_status'], 1, c['high_level'], c['original_str'], c['original_con'], c['original_dex'], c['original_cha'], c['original_int'], c['original_wis']])))]
skipped, remapped = [], []
MANUAL = {41480: 40014}  # Greater Potion of Bravery ($7736, not in the US 2009 client) -> Potion of Bravery
for it in items:
    iid = int(it['item_id'])
    if iid in MANUAL:
        remapped.append((iid, MANUAL[iid], it['nm'])); iid = MANUAL[iid]
        names.setdefault(iid, 'Potion of Bravery')
    if iid not in names or (it['nm'] and names[iid].lower() != it['nm'].lower() and it['nm'].lower() in by_name):
        cand = by_name.get(it['nm'].lower(), [])
        if len(cand) >= 1:
            remapped.append((iid, cand[0], it['nm'])); iid = cand[0]
        elif iid not in names:
            skipped.append((it['item_id'], it['nm'])); continue
    sql.append("INSERT INTO character_items (id,item_id,char_id,item_name,count,is_equipped,enchantlvl,is_id,durability,bless,attr_enchant_level,attr_enchant_kind,charge_count,remaining_time) "
               "VALUES (%s,%d,%d,%s,%s,%s,%s,%s,0,1,%s,%s,%s,0);" % (it['id'], iid, OLD_ID, q(names[iid]), it['item_count'], it['is_equipped'], it['enchant_level'],
                                                                it['is_identified'], it['attr_enchant_level'], it['attr_enchant_kind'], it['charge_count']))
for s in skills:
    sql.append(f"INSERT INTO character_skills (char_obj_id,skill_id,skill_name,is_active,activetimeleft) SELECT {OLD_ID},skill_id,name,0,0 FROM skills WHERE skill_id={s['skill_id']};")
sql.append("COMMIT;")
subprocess.run(NEW + ['-e', '\n'.join(sql)], check=True, capture_output=True, text=True)
print('migrated fx: level', c['level'], '%d items, %d skills; remapped by name: %s; skipped: %s' % (len(items) - len(skipped), len(skills), remapped, skipped))

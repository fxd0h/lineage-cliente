-- Idempotent database fixes for l1j-en/classic, applied by instalar.sh after the import.

-- GM characters log in invisible and invulnerable (run_on_login). NPCs and mobs ignore invisible GMs
-- (getRecognizePlayer(.., ignoreInvisGm)), so a GM test character sees a frozen world. Make both opt-in (.invis / .invul).
UPDATE commands SET run_on_login = 0 WHERE name IN ('invis', 'invul');

-- #43 Lastabad / Resistance Village teleport scrolls sent everyone to the same spot (map 400).
-- "Teleport - Lastabad" -> Lastavard 1F landing used by the 1F scroll; Resistance Village scroll -> same
-- spot as the Return scroll (40120).
UPDATE etcitem SET locx = 32737, locy = 32900, mapid = 310 WHERE item_id = 40083;
UPDATE etcitem SET locx = 32571, locy = 32673, mapid = 400 WHERE item_id = 40853;

-- #35 Skill reuse timers: Fire Storm (and Lightning Storm) were 400/500 ms; the original is ~2.5 s.
-- The other 380 skills with reuseDelay 0 still need an audit against the original values.
UPDATE skills SET reuseDelay = 2500 WHERE skill_id IN (65, 70) AND reuseDelay < 2500;

-- Clan alliances (patch 0100; the server also creates it on first use).
CREATE TABLE IF NOT EXISTS clan_alliance (clan_id INT NOT NULL PRIMARY KEY, alliance_id INT NOT NULL, slot INT NOT NULL);

-- #9: the GM shops of potions / scrolls / teleport scrolls cost 1 adena like all the other GM shops.
UPDATE shop SET selling_price = 1, purchasing_price = 1 WHERE npc_id IN (81027, 81028, 81029);

-- Maeno is the Bugbear Race Manager: the US/TW client data has page "maeno1" (the DB pointed at "perkin", which no client has).
UPDATE npcaction SET normal_action = 'maeno1', caotic_action = 'maeno1' WHERE npcid = 70041 AND normal_action = 'perkin';
-- Biyan: the page is "biyan" (there is no "biyan1").
UPDATE npcaction SET normal_action = 'biyan', caotic_action = 'biyan' WHERE npcid = 70752 AND normal_action = 'biyan1';

-- Bugbear Race (patch 0101): ticket table, the 20 runners (npc 91350+) and the buy/sell lists of the ticket sellers
-- (Polly 70035, Maeno 70041, Secil 70042). The selling list is filled by the race itself; the purchasing price is the
-- ticket price (winners are paid price * allotment).
CREATE TABLE IF NOT EXISTS race_tickets (
  item_obj_id INT NOT NULL DEFAULT 0,
  round INT NOT NULL DEFAULT 0,
  allotment_percentage DOUBLE NOT NULL DEFAULT 0,
  victory INT NOT NULL DEFAULT 0,
  runner_num INT NOT NULL DEFAULT 0,
  KEY idx_item_obj_id (item_obj_id),
  KEY idx_round (round, runner_num)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
INSERT INTO npc (npcid, name, nameid, note, impl, gfxid, size, passispeed, atkspeed, alt_atk_speed) VALUES
 (91350, 'Vekadoo',   '$1213', 'Giran> Bugbear Race', 'L1Npc', 3497, 'small', 320, 1080, 3360),
 (91351, 'Tutu',      '$1214', 'Giran> Bugbear Race', 'L1Npc', 3498, 'small', 320, 1080, 3360),
 (91352, 'Buggy',     '$1215', 'Giran> Bugbear Race', 'L1Npc', 3499, 'small', 320, 1080, 3360),
 (91353, 'Urloo',     '$1216', 'Giran> Bugbear Race', 'L1Npc', 3500, 'small', 320, 1080, 3360),
 (91354, 'Jeffrey',   '$1217', 'Giran> Bugbear Race', 'L1Npc', 3501, 'small', 320, 1080, 3360),
 (91355, 'Kai',       '$1218', 'Giran> Bugbear Race', 'L1Npc', 3502, 'small', 320, 1080, 3360),
 (91356, 'Adolph',    '$1219', 'Giran> Bugbear Race', 'L1Npc', 3503, 'small', 320, 1080, 3360),
 (91357, 'Overpot',   '$1220', 'Giran> Bugbear Race', 'L1Npc', 3504, 'small', 320, 1080, 3360),
 (91358, 'Lumps',     '$1221', 'Giran> Bugbear Race', 'L1Npc', 3505, 'small', 320, 1080, 3360),
 (91359, 'Buka',      '$1222', 'Giran> Bugbear Race', 'L1Npc', 3506, 'small', 320, 1080, 3360),
 (91360, 'Glook',     '$1223', 'Giran> Bugbear Race', 'L1Npc', 3507, 'small', 320, 1080, 3360),
 (91361, 'Kundra',    '$1224', 'Giran> Bugbear Race', 'L1Npc', 3508, 'small', 320, 1080, 3360),
 (91362, 'Kumato',    '$1225', 'Giran> Bugbear Race', 'L1Npc', 3509, 'small', 320, 1080, 3360),
 (91363, 'Durek',     '$1226', 'Giran> Bugbear Race', 'L1Npc', 3510, 'small', 320, 1080, 3360),
 (91364, 'Grodon',    '$1227', 'Giran> Bugbear Race', 'L1Npc', 3511, 'small', 320, 1080, 3360),
 (91365, 'Queniver',  '$1228', 'Giran> Bugbear Race', 'L1Npc', 3512, 'small', 320, 1080, 3360),
 (91366, 'Pluto',     '$1229', 'Giran> Bugbear Race', 'L1Npc', 3497, 'small', 320, 1080, 3360),
 (91367, 'Duriba',    '$1230', 'Giran> Bugbear Race', 'L1Npc', 3498, 'small', 320, 1080, 3360),
 (91368, 'Buruel',    '$1231', 'Giran> Bugbear Race', 'L1Npc', 3499, 'small', 320, 1080, 3360),
 (91369, 'Elijah',    '$1232', 'Giran> Bugbear Race', 'L1Npc', 3500, 'small', 320, 1080, 3360)
 ON DUPLICATE KEY UPDATE name = VALUES(name), note = VALUES(note);
INSERT IGNORE INTO shop (npc_id, NPC_Name, item_id, Item_Name, order_id, selling_price, pack_count, purchasing_price) VALUES
 (70035, 'Polly', 40309, 'Race Ticket', 0, -1, 1, 500),
 (70041, 'Maeno', 40309, 'Race Ticket', 0, -1, 1, 500),
 (70042, 'Secil', 40309, 'Race Ticket', 0, -1, 1, 500);

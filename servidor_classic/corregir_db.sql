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

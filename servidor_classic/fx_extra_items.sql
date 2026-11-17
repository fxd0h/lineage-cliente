-- Run ONLY while fx is offline (the server rewrites the inventory on logout): the 28 Greater Potions of Bravery
-- of the old character, as Potions of Bravery (40014; the greater one does not exist in the US 2009 client).
INSERT IGNORE INTO character_items (id,item_id,char_id,item_name,count,is_equipped,enchantlvl,is_id,durability,bless,attr_enchant_level,attr_enchant_kind,charge_count,remaining_time)
VALUES (268567493,40014,268479040,'Potion of Bravery',28,0,0,1,0,1,0,0,0,0);

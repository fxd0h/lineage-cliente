# 3.81C data coverage (L1JGO-Whale vs the two game installs)

A = L1Justice (`juego/Lineage Justice/`), B = TW 3.81C 2013 (`ref/tw2013/休閒天堂381/`, untrusted, only read, never executed).
Needs come from `servidor_380/l1jgo/data/yaml/*` (+ literals in `internal/*.go`). Installs were read with the fork's
`.idx/.pak` format (Text.idx/Text.pak DES-decrypted; script was a throwaway Python + a small C++ dumper using `recursos/des.cpp`).
"Present" = in `Sprite*.pak`/`Sprite.pak`/`Tile.pak`/`Text.pak` or loose in the install folders (B has loose `sprite/` 1290 files).
Method limits: the packed `list.spz` of A/B (signatures S1354/S1375) and B's `List.spr.bin` are not DES (unreadable), so "in table"
uses the plaintext `juego/spritelist/list.spz` (21970 gfx, newer than both) and `datos/sprite_table.txt` (65 gfx).
Hash samples compare raw bytes of ids present in both.

## Summary

| Category | needed | in A | in B | in neither | only B (A lacks) |
|---|---|---|---|---|---|
| Maps (ids: map_list, spawns, portals, teleports, doors) | 736 | 619 | 692 | 41 | 76 (only A: 3) |
| Map segments (.s32 at spawn/portal/door points) | 2094 | 1842 | 1864 | 230 | 22 (A is a subset of B) |
| Tile sets referenced by those segments (`N.til`) | 3643 | 3642 | 3643 | 0 | 1 |
| Sprites, all categories (gfx with any `<gfx>-<n>.spr`) | 3186 | 2521 | 2598 | 587 | 77 |
| Inventory icons (`inv_gfx` -> `N.tbt`) | 1918 | 1802 | 1917 | 1 | 115 |
| `$N` name ids in desc-e.tbl | 4310 | 4310 | 3208 | 0 | 0 |
| NPC HTML pages (`<id>-e.html`; any lang) | 77 | 68 (68) | 41 (71) | 6 | 3 (any lang) |
| Sound effects (ids from list.spz frames of needed gfx) | 3464 | 3180 | 3430 | 34 | 250 |

## 1. Maps
- Server has 1039 `map/N.txt`; 735 ids in map_list, 313 with spawns, 370 in portals, 75 in teleports. Union used: 736.
- Neither install has 41 maps: 1006-1023 (dungeon instances, 16 segments each wanted), 7000, 7783, 7951-7957...
  Maps only in B (76, e.g. 560-566, 633-641, 1700-1705, 3301-3303...). Only in A: 260, 1935, 1936.
- Segment files for all needed maps: A 5945 `.s32`, B 7528. At needed points A covers 1842, B 1864; every A segment exists in B.
- Common segments differ in bytes: 113 of 1842 (6%, e.g. map 0 `7ffd8002.s32`, `7ffc8004.s32`): B is the later map revision.
- Tiles: all 3643 tile sets the segments use exist in B (A lacks 1). Byte compare of 60 common `.til`: 0 differ. B has 4633 .til vs A 4114.

## 2. Sprites (`<gfx>-<n>.spr`, hashed Sprite00-15 + Sprite.pak)
| Category | needed | A | B | neither | only B |
|---|---|---|---|---|---|
| NPC/monster gfx (all npc_list) | 1312 | 988 | 996 | 316 | 8 |
| NPC/monster gfx (actually spawned) | 762 | 605 | 605 | 157 | 0 |
| Polymorph gfx | 303 | 44 | 44 | 259 | 0 |
| Doll gfx | 15 | 11 | 11 | 4 | 0 |
| Door gfx | 136 | 135 | 135 | 1 | 0 |
| Item ground gfx (grd_gfx) | 1076 | 1035 | 1062 | 13 | 28 |
| Spell/effect gfx (cast_gfx, traps, services) | 406 | 345 | 385 | 21 | 40 |
| Effect gfx literals in Go | 22 | 19 | 21 | 1 | 2 |

- Missing in both: all of them are listed in the plaintext (newer) list.spz, i.e. Whale uses gfx from clients later than 3.81C.
  Examples: NPC 144 (ghoul), 146 (Ungoliant), 234, 266, 695-703, 3860/3864 (orc archer/fighter), 4003 (black knight),
  5683-5691 (pollution/"污濁" monsters); polymorph 2284, 2323, 2374-2388...; dolls 7659-7662; door 2510; effects 829, 832, 871, 1085, 5906-5910, 6975.
- Only in B: effects 830, 10568, 10657, 10672, 11671...; ground gfx 10551, 10552, 11283...; NPC 8542, 8545, 8653, 10370, 12338.
- Common gfx: 2520. B has more frames in 12 of them (+193 frame files, A has none that B lacks).
- Byte compare of 500 common `.spr`: 26 differ (5%; e.g. 4091-8, 4649-24, 7684-17, 2332-13, 2289-17). Same ids, different art/frame data.
- Sound ids are not sent by Whale; they come from `[n`/`<n` markers in the sprite list (see 5).

## 3. Item icons and UI images
- Inventory icons: armor 710 needed (A 601, B 709), weapon 238 (236/238), etc 993 (988/993). Missing in both: 8647. Only B: 115 (3213, 3215, 3216, 3364, 3512...). 400 common ids hashed: 0 differ (identical).
- UI `.img`: A 5899, B 5383, both 4787 (A only 1112, e.g. 6510-6523; B only 596, e.g. 3332-3341). 34 of 300 common differ (e.g. 3387, 2748, 2394, 1101).
- UI `.png`: A 2453, B 2531, both 1953 (only A 500, e.g. 2144-2153; only B 578, e.g. 1561-1563, 1861, 2445-2448). 2 of 300 differ (1570, 10142).
- `skin/`: A Button.bmp, Serverlist.bmp; B adds Bulletin.bmp (login-only).

## 4. Text
- `$N` ids (yaml + Go literals) = 4310. A desc-e.tbl (22332 lines) covers 100%; B (14170 lines) covers 3208 (74%), 1102 missing.
- 522 ids differ between A and B. The bulk are NPC names: A has English "Role^Name" ("Storage^Dorin", "Merchant^Pandora"), B has the bare name ("Dorin") - A's form is the 2009 one the fork parses.
- HTML pages Whale opens (SendHypertext ids + yaml html ids, 77): A 68, B 71 in any language but only 41 with `-e.html`; neither: doromond1-5,7. B has 3 not in A. 32 of the `-e` pages present in both differ (duvall3, inn3, kirius2, sirius2, telecaden2, telecgiran2...).
  Whale also has its own plaintext dialogs (`data/dialogs/*.htm`), sent dynamically, not from the client.
- B loose `text/` (42 `-c.html`) is Chinese only.

## 5. Sounds / music
- Effect ids from the list.spz frame markers of the needed gfx: 3464. A has 3180, B 3430, neither 34 (952, 4521-4594...), only B 250.
- `.wav` files: A 5207, B 11386. 14 of 200 common wavs differ. Music: A 165 mp3 (+mid, 22 ogg in B), B 475 mp3; A's 165 are all in B.

## Recommendation
1. **A primary** for text (English `desc-e.tbl`, Role^Name, English html), the already validated UI/sprite set and everything byte-identical (icons, tiles).
2. **B as fallback** for what A lacks, via an ordered multi-folder loader (`juego_extra=<B>/` after the main folder, lookup order: A pak, then B pak/loose):
   - maps: 76 maps + 22 segments + 1 tile set (this alone moves needed-map coverage from 84% to 94%);
   - sprites: +77 gfx (effects/ground items/8 NPCs), +115 inventory icons, +250 sounds, 596 `.img`/578 `.png` UI ids A lacks;
   - html: 3 pages.
3. Never take B text for names: its `desc-e.tbl` lacks 26% of the ids and drops the English roles.
4. Collisions (same id, different bytes): sprites 5%, segments 6%, UI .img 11%, wav 7%. Keep A for the 2009 look (A is what the fork was matched against); the loader should let a per-category override prefer B only for map segments if A's segment is older and a spawn/portal falls on tiles missing in A. A sample of the 113 differing segments should be diffed before deciding.
5. Real gaps (nothing in A or B): 41 maps (1006-1023, 7000, 7783, 7951-7957), 157 spawned NPC gfx + 259 polymorph gfx + 4 dolls + 21 effects, 1 inventory icon, 6 html pages. They need a newer client or the existing placeholder (magenta block); polymorph and 3 of 5 spawn gfx groups (pollution, orc, ghoul) are the priority.

# Round 6 RE (S3EP1U.bin) -- tags: [C] confirmed in code, [I] inferred

Skipped by request: the game-time orb sparkle overlays (item 3; the HUD code is under review).

## 1. Cooking window (ctor 0x4d43f0, global [0x8f2fd4]; recipe grid ctor 0x4d3840, global [0x8f2fd0]) -- implemented (`cocina_ui.cpp`)
- Opens from S_PacketBox 52 (handler 0x49135f): `d junk, c sub, c stage-1`; sub 1 opens with stage = value + 1 (the same stage again closes it),
  sub 0 makes the exe answer with C_ItemUse (l1j-en never sends it). The cook book object id [0x8f2fb8] is written where an item with gfx
  0x9c2..0x9c6 (2498..2502, 0x449a70) is used (0x44989d / 0x44c558); with none the click prints string-e 1167 (0x4d3900, white, "In order to cook,
  you need a cookbook."). [C]
- Window: default x 640 - 190 (the base object starts at 0 and the ctor adds 640 - width of 1480), y 0; background 1481 at (6,8), frame 1480
  (190 x 292), ornament 998 at (0,11) drawn over the grid, stage picture 1482 + stage at (112,265), progress slot at (32,260) 50 x 8, close 969
  at (165,18), drag handle (0,5,191,25), scroll bar (0x47cd80) up arrow (161,48) / track (161,56) / down arrow (162,235), thumb 976. [C]
- Grid (class 0x44dc90 base, vtable 0x4f5ca0, draw 0x4d3d10, hit rect (21,42)-(155,244)): 34 x 34 cells, 4 columns, the first at the rect
  corner, at most 24 cells from the scroll offset (offset in recipes, step 4 = one row, `offset + 4 < count` over all 24 rows). Hit test 0x44eec0:
  index = offset + row * 4 + column. A cell is the 32 x 32 dish icon (`DAT_701450[gfx]`, the inventory icon table, normal state) of the static row i
  of 0x6e52e8 (gfx, string-e effect line, tier); stage n shows the first 8 * n rows (0x4d43c0). [C]
- Cell marker 536 at (x-1,y-1) when the row is selected (+8). One click selects (0x44f240 / 0x44f140: clears the others, sets +8); the double click
  (vtable slot 0x58 = 0x4d39c0, needs a selected visible row) calls 0x4d3900 -> `cdcc` 0x77 `d book, c 1, c recipe` (recipe = list index 0..23). [C]
- Hover (same routine as the inventory, 0x4d3d10 = 0x44e45b): selected cell with an effect text -> 142 px bevel box (light 0x50050c / dark 0x500514,
  darkened fill) at grid x, y = cell y + 32 (above: y - 12 n - 52, clamp grid y + 10 / + 330), dish name wrapped at 23 columns then the string-e effect
  line wrapped at 22 (0x446490 / 0x446520 via 0x41a040), 12 px lines at +5, item colour table 0x8f0f34[1] (white); any other cell: the name only under
  the cell. The name comes from recipe.tbl (first quoted field, a `$N` desc-e id). [C]
- recipe.tbl is read by 0x4d3130 in file order, only cookNo < 0x30 goes to the grid (the cookNo 80/81/82 records go to a second vector, unused by
  this window). Drag of a recipe to the shortcut bar: 0x4d3ac0 (slot 0x54), not modelled. [C]
- Progress bar (0x4d4c40, 50 px * (remaining / 2) / total, timer 0x4d38d0 every 500 ms): started from a server message handler (0x485256 /
  0x4853f5: [0x8f2fac] = recipe +0x104 * 2) that l1j-en never triggers (makeCooking sends only S_SkillSound 6392 / 6390 / 6394 and messages
  1160 / 1103 / 1102 / 1101), and string 1100 is the "in progress" refusal; not modelled. [C]
- l1j-en has no cook book in a GM's reach (`.create` needs access 200, Mergechk has 1), so the live test used `debug-cocina <stage> [book id]`.
- Index note: the fork's `TablaTexto::linea(n)` is the exe's index n (string-e entry n = file line n + 1 counting the header as line 0), not
  "line n + 2" as RE_ROUND5 / RE_ROUND4 wrote; every string id in this round is the exe index.

## 2. Mail reading window (class 0x4541b0, global [0x7026f8], vtable 0x4f1698) -- implemented (`correo_ui.cpp`)
- The reading window is the compose paper 1545 with four read-only edits; the S_Mail read handler (0x492bb4: letter id -> view +0x28) fills
  From (100,55) = the list row's name column (+0x6c), To (100,70) = the own name ([0x8f0e4c] +0x5c), Subject (100,90) 116 px (+0x7a) and the body
  (50,112) 170 x 192 (+0xa4), closes the list (0x4d1200) and opens the view. Paper 671 (0x417320) is not this flow. [C]
- Buttons (0x47c6d0): Reply 1574 at (56,308) -> 0x4555b0: closes the view, opens the compose window in mode 2 (private) with To = the row's name;
  Delete 1558 at (96,308) -> 0x455610: C_Mail `c 0x30 | box (0 private, 1 clan, 2 storage), d id`; Keep 1577 at (138,308) -> 0x455660: C_Mail 0x40
  `d id`; Back 1568 at (174,308) -> 0x455680: closes the view and reopens the list at its scroll; X 1583 at (230,5) -> 0x455270: closes only the view. Tabs
  (55/118/174, 31) and mode buttons (24/53, 340) like the compose window: 0x455450 inbox, 0x455480 compose, 0x4554b0 storage, 0x4554e0 private
  list, 0x455510 clan list (0x480f00). Drag handle (7,6,198,26). [C]
- Fork: `c.leyendo` paper 1545 view as above; Reply opens the compose with To = sender, subject / body empty (the exe's focus after Reply is not
  known; the fork focuses Subject). Delete does not close the view (the exe leaves it open with id -1). Verified live with `debug-correo`.
- **From edit of the compose window (0x453120, field +0x18)**: every reference to [0x7026fc] was checked (0x453a82, 0x453c62, 0x454062, 0x454092,
  0x4540c1, 0x454140, 0x45548d, 0x4555bd, 0x4577a9..); nothing writes the From edit, it stays blank. In the view the same slot is the sender. [C]

## 3. Orb sparkle overlays -- skipped (see the top).

## 4. Cursor states
- **State 8, case (a)** (0x469f1f): pending use [0x81654c] (a callback; setter 0x402490 `(ecx = callback, edx = range, [arg] = string-e id printed)`,
  14 callers, message 179 "Select a target." or 282 "Select an item.", range 15 in every caller except the out-of-range spell retry at 0x4ac54a where it is
  the spell's range). Cursor 8 when the target (object under the pointer, else the cell under it) is within |dx| <= range and |dy| <= range (the exe
  compares the doubled x with 2 * range), 15 with Ctrl ([0x6f1dac]) or Alt ([0x702dd6]) held, else 11 (0x46a344). [C]
- **State 8, case (b)** (0x46a23a, attack mode [0x703105]): the value is `[0x8f07b0] + 0x70`, and [0x8f07b0] is the spellbook grid (ctor 0x4aaef0,
  `+0x70` = -1 at creation): the selected spell index, 0x28 = 40 "Create Zombie", 0x3c = 60 "Resurrection" (spelldesc-e entries); then the attack cursor
  15 / 10 is replaced by 8. [C for the object, I for the "selected spell" meaning]. Not implemented: the fork keeps no selected spell.
- **State 14** (kinds 7 / 13, door kind 8 with tile flag bit 2) and **16** (own object +0x12c non-empty = the action overlay of the fishing bobber): no data /
  no fishing in the fork, so they stay unselected. The chooser's other kind-based states (9, 10) are also still not used.
- Fork: pending target modes (`hechizo_apuntando`, `item_apuntando`, `mascota_eligiendo`) now pick 8 in range / 11 out of range (15 with Ctrl / Alt);
  `gm_apuntando` (fork only) keeps 11. The target range uses 15 cells for all of them (the spell retry range is not modelled).
- **Loader bug fixed**: `ORIGINAL_IDS` was sized by the cursor enum (21) with 16 zero entries, so loading RT_CURSOR id 0 failed and the loader dropped every
  original cursor; the 16 state cursors (RT_GROUP_CURSOR 105, 127, 130..133, 136, 138..145, 150) were never loaded and the chooser's states
  fell back to the hand-drawn hand. They load now. `estado` in the live control prints `cursor=<state>`.
- Also seen: while a grid is being pressed ([0x6fa7cc] == the cooking grid [0x8f2fd0] or the spell grid) the chooser returns 5 (0x469ebe).

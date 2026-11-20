# Windows in S3EP1U.bin (reverse-engineered)

Tags: [C] confirmed from decompile, [I] inferred. Coordinates are relative to the window origin.

## Window management

- **Drag [C]:** inventory (0x44ccc0), status (0x4b46d0), spellbook, trade, the 1480 window and the 1197-frame window
  have a drag handle (ctor 0x47bcd0; handler 0x47bd90) over their title strip only, clamped to 640x480.
  Handle rects: inventory/status/trade (0,5,191,25) (trade 199 wide), spellbook (6,9,197,29), 1197 window (7,6,198,26).
- **Open list [C]** (0x4d0f30, thunk 0x4d1200): 4 slots (0x8f2f54..0x8f2f60). Opening a window already in the list
  closes it (toggle); otherwise it goes to the front and a 5th window closes the oldest. Null closes all four.
- **Esc [C]** (key handler 0x456ba0, case 0x1b): closes all four windows, drops the popup `[0x7010d0]`, cancels a pending
  trade / private shop. Every open window gets the key first (vtable+0x28).
- **Ctrl keys [C]:** code 3 community, 8 help, 0x16 options, 0x12 floating HP/MP; 1, 9, 0x11, 0x13 toggle windows (targets U).
- **Right double click [C]** inside a window closes it (inventory 0x44d1b0); a press inside brings it to front; any message
  inside the rect is swallowed.
- **Close button [C]:** art 969 (ctor 0x47c2d0) at inventory/status/pet inventory (168,18), spellbook (181,18), trade (174,18),
  1480 window (165,18), 1197 window (182,19).
- **Default positions:** inventory (640-190=450, 0) [C]; spellbook (640-207=433, 0) [C]; 1480 window 640-width [C];
  status (0,0) [I]; others (0,0). No saved layout.

## Status page (draw 0x4b4a00, body 0x4b3e10)

- Gauge box 1263 at (69,46); fills EXP 1264 (69,46) width = w*floor(exp%)/100, HP 1265 (69,62), MP 1266 (69,77), blend
  0x8003, drawn only if max > 0. [C]
- Text: level (100,50), EXP "%02d.%02d%%" (115,50), HP "%d/%d" y 66 (x 100), MP y 80, weight / food centred in x 107..150 at
  y 94 / 107 (0x4b38b0). [C]
- Stats (0x4b3740): STR/DEX/CON x 92, INT/WIS/CHA x 155, y 126/139/152, centred at 6 px per char; class 3 draws them 1 px
  higher. Pair (115,171)/(115,184); elements x 80/147 at y 205/218 (colours 0x50051c, 0x50050c, 0x500570, 0x500518);
  value (72,231); AC x 149; alignment number (158,184). [C]
- Alignment image (0x4b8520): 987 if < 0, 988 if 0..499, 989 if >= 500, at (121,167); (125,164) when `[0x702b7c]` is 3 or 5
  (`[0x702b7c]` is most likely the class byte [I]). [C]
- Bottom gauge 990-995 is a day/night game clock [C]: 0x4b2cc0 light level, 0x4b2e30 picks day 990/991/992 or night
  993/994/995 and the marker offset `v = (t-start)*83/(end-start)`; cap 990/993 at (76,233), bar 991/994 at (83,232), marker
  992/995 at (83+v,233). Time ranges [I].
- Tabs: hit rects (22,245) and (57,245), 34x17; string 882 "Equipment" is tab 0, 883 "Character" tab 1. [C]

## Inventory (draw 0x44e010, mouse 0x44f240)

- Grid origin (21,42), 4 columns, 24 per page, pitch 34. Equipped marker 536 at (x-1,y-1) behind the icon. [C]
- Overlays from the icon table: 2700 at the cell / 2701 at (x+1,y+1) when the item name contains one of two global strings
  (table `[0x8f0afc]` entries 9 and 117, meaning U), skipped for item gfx 0x1d1/0x1d2; 2702 at (x+24,y+24) when item+0x80. [C/U]
- Hover popup: x = cell x clamped to view+0x27c; y = cell y+32, or y-12 near the bottom. Equipped items with stats: 142 px
  bevel box (0x50050c light / 0x500514 dark border, darkened fill). Name colour `0x8f0f34[idx]` (idx 3 when item+0x74 == 0,
  else item+0x6c); plain names with a black outline (0x5004f0); two-part names on two 12 px lines. [C]
- Mouse: left down selects (and captures for drag unless Ctrl); drag starts after 25 px; drop via vtable+0x54 (outside the
  grid = trash / world); left double click = use; no right-click handling in the grid. [C]
- Tabs at (19+35i, 249), 35x17, strings 878-881 All / Equipment / Quest / Misc. Pet inventory 0x44f5c0: same grid, frame 997,
  one tab (string 926). [C]
- 1480 window (0x4d43f0): frame 1480, grid class 0x4d3840 (21,42, pitch 34), no tabs, opened by a server packet from
  0x48e780 near 0x49138a; contents U.

## Trade (ctor 0x42bf20, draw 0x42d280)

- Background 1199 (7,10), frame 1201 (0,0), ornament 1200 (2,12), close 969 (174,18), OK 1202 (53,282), Cancel 1204 (100,282)
  (pressed = id+1, no hover). Names white centred at (43,31) and (43,159). [C]
- Grids: own at y 48, partner y 176; cell x 27 (text 23), pitch 34, 4x3 visible; mark 696 at (x-2,y-1) behind flagged items.
  Scroll bars x 168: up 1206 (168,46), track 1212 (168,54), down 1208 (168,139), thumb 1210; second at y 175/183/268. [C]

## Yes/No bar (0x41fe30)

- Image 701 at x = 137 + view origin, caller y; two question lines inside (12 px pitch from bar y+4, white outline); YES/NO at
  bar x+224 / x+264 (361,447 / 401,447). [C]

## Mail letter (0x417320)

- Buttons 686 (159,302), 678 (213,297) close/back, 674 (90,299); title string 541 right-aligned at x 180, y 18; body control
  (180,228), up to 0x200 chars. Mail windows are pre-created at world entry (0x458610). Roles of 0x413500 / 0x413a80 /
  0x416b50, compose 0x455800 and the memo editor: U. [C/U]

## Spellbook (ctor 0x4aebe0)

- Tab strip (class 0x47e860) at (16,38): by class byte `[0x81b534]`: elf 936 (11 frames), prince 947 (7), dark elf 954 (5),
  others 959 (10); image = base + tab. Grid (37,43) 174x248. Scroll bar: up 974 (175,45), track 971 (175,53), down 972
  (176,232), thumb 976; max rows prince 13, elf 21, dark elf 9, others 19. Frame 934 (207 wide), close (181,18). [C]

## Pet / summon

- Frame 1197 also belongs to window 0x4a6c90 (global `[0x8ed650]`), separate from the volume window 0x4a9cd0; content U.

## Round 2 findings

- **Spellbook tabs (C):** strip handler 0x47e9d0 (down / double click); tab i is an 18x19 rect at strip_y + 19*i; a
  click selects it, sets the first scroll row to 2*i; the drawn tab follows the scroll (row / 2). Each tab is 8 slots
  (2 rows of 4). Max scroll rows: prince 13, elf 21, dark elf 9, others 19.
  Spell placement (S_SkillAdd 0x48ce40, idx = row*8 + bit, skill id = idx+1) by class slot tables (0x4424xx):
  knight/wizard slots 0-79 = idx 0-79; prince 0-47 = idx 0-47, 48-55 = idx 112-119; elf 0-47 = idx 0-47,
  48-98 = idx 128-178; dark elf 0-15 = idx 0-15, 16-19 = 96-99, 20 = 108, 24-27 = 100-103, 28 = 109, 32-35 = 104-107,
  36 = 110; dragon knight (5) 0-14 = idx 180-194; illusionist (6) 0-19 = idx 200-219. Knight overrides tab 2:
  slot 8 = idx 86, 9 = 88, 10 = 87, 13 = 89, 14 = 90.
- **Buff table 0x6e1020 (C):** 154 rows: icon, seconds (-1 infinite), exclusion group (+8: activating a slot removes the
  other active slots of the same non-zero group), tooltip string-e index (+0xc), end-of-effect chat message string-e
  index (+0x10, printed when it expires, e.g. 718 "The weapon has returned to normal."). string-e entry n is line n+1.
  A dump (with text) is kept outside the repo at re/scratch/buff_table_0x6e1020.tsv.
- **Close button (C):** art 969 at frame origin + (168,18) for inventory/status; there is no title strip above the frame
  image (the title is part of the frame art; the drag handle (0,5,191,25) lies inside it).
- **Quantity entry (C):** there is no quantity window. Drop / pick up / give use the chat input line as a prompt
  (string-e 119 drop "How many do you want to drop? (1-%u, Enter=all, Esc=cancel)", 121 pick up, 140 give); empty =
  whole stack. 0x4cfa90 is the warehouse password keypad (image 895, digits 896+2d at (12+22*(i%5), 38+17*(i/5)), field
  (17,14) with `*`, max 6 digits, clear 916 (12,72), OK 918 (61,72), cancel 920 (90,72), prompts string-e 838-841,
  wrong 842).
- **Mail (C/I):** 0x417a90 mailbox list (buttons 658 close (209,305), 660 (34,332), 662 (139,304), 664 (171,304),
  666 (64,311), 668 (99,311)); 0x416b50 letter writing (edits (16,50) w180, (20,67) w228; buttons 672 (56,312),
  676 (110,312), 678 close (213,297), 674 (90,299)); 0x417320 read letter; 0x453120 compose (edits (100,55), (100,70),
  (100,90) w116, body (50,112) 170x192, buttons 1580/1582/1581 at y 308, sends opcode 127 `cchsSS`); 0x4541b0 view
  variant (I); 0x47fd10 memo editor (frame 1545, button 1583 at x 230) (I).
- **Info card 0x4a6c90 (C):** frame 1197, background 1198 at (8,11), icon (icon table) at (25,45), wrapped desc-e text
  below, close (182,19); opened by clicking a type-4 world object (sign) whose name is a `$` desc id (0x408300).
- **Hold-to-walk (C):** no timer: on arriving at each tile with the mouse held, the click decision 0x468c20 reruns with
  the stored pointer position; mouse move with the button held also reruns it.
- **Alt+1/2/3 (C):** emotes via opcode 0x26 for standard human sprites: Alt+1 -> 0x44, Alt+2 -> 0x45, Alt+3 -> 0x43
  (other sprites gated, U).
- **Panel buttons (C):** pressed = hover = base+1; no third image.
- **Inventory overlays (C):** desc-e entries 9 "Armed" and 117 "Worn": items whose name contains them get overlay 2700
  then 2701 at (+1,+1) (not item types 0x1d1/0x1d2). 2702 at (+24,+24) marks a sealed item (item+0x80, set when the
  status byte gains 0x80, message string-e 1004 "%s was sealed.", cleared with 1015).

## Round 2 implementation notes

- **Spellbook:** `spellbook_skill_id(class, slot)` / `spellbook_max_scroll` in recursos/hechizos.h; hechizos_ui.cpp draws 6 grid
  rows from the scroll row; the scroll bar range is 0..max (19 / 21 / 13 / 9). Verified in a test instance (tab 2 click
  scrolls to row 2 and highlights tab 2).
- **Buffs:** mundo/tabla_buffs.h embeds only the numeric columns (tooltip idx, end idx, group) and the effect -> slot map
  (derived from the packet handlers; Shield / Shadow Armor slots 4 / 5 inferred). Exclusion groups act in
  `poner_efecto_activo` for known slots. The end message is queued on expiry or removal and printed one second later
  unless the server sent the same text meanwhile (l1j-en sends `sysmsgID_stop` of the skill row for some skills, e.g. 308
  for Enchant Weapon, and nothing for others). Original UI only. Effects the fork does not map to a slot (spell-registered
  EA_HECHIZO, most class buffs) have no tooltip / end message: TODO.
- **Close button:** (168,18) for inventory and status.
- **Quantity prompts:** drop 119 / give 140 / pick up 121 in the chat line; the %u is the stack size.
- **Emotes:** `Cli::EXTCOMMAND` (s3ep1 wire 38); l1j-en C_ExtraCommand rebroadcasts S_DoActionGFX to the others, so the
  own sprite plays the action locally. Non-human sprite action table (0x456b30) not done.
- **Overlays:** 2700 / 2701 at (+1,+1) for names containing " ($9)" / " ($117)" (the existing `item_equipado`), skipped for
  icons 0x1d1 / 0x1d2; 2702 at (+24,+24) when the bless byte has 0x80. The exact blend of 2700 is not decoded: additive
  is used (a normal blit covered the item with an opaque disc).
- **Hold-to-walk:** already equivalent (no timer, per-frame decision after each step).
- **Panel buttons:** already pressed = hover = base+1.


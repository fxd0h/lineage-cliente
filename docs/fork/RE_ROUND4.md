# Round 4 RE (S3EP1U.bin) -- tags: [C] confirmed in code, [I] inferred

Scratch tools added (run via scratch/r.sh, which sets PYTHONPATH for capstone): d.py (disasm: addr n), xr.py (code xrefs
to a dword), ctorargs.py / ctx.py (call-site arg and guard recovery), plus the older callers.py.

## 1. Character-list entry (handler 0x489690, format "sscchhhccccccccc", entry stride 0x30 at 0x6de6e0)
Server packet = S_CharPacks (opcode 0x2f? list handler; 0x38 variant: single-entry "new char" path).  [C]
| entry off | wire field | l1j-en S_CharPacks arg |
|---|---|---|
| +0x00 (13 B max, 0xd) | s name | name |
| +0x0d (17 B max, 0x11) | s clan | clanName |
| +0x1e | c type/class | type |
| +0x1f | c sex | sex |
| +0x20 | h alignment | lawful |
| +0x22 | h HP | hp |
| +0x24 | h MP | mp |
| +0x26 | c AC | ac |
| +0x27 | c level | lv |
| +0x28..0x2d | c x6: str dex con wis int cha as the client names them | l1j-en writes str,dex,con,wis,**cha,intel** (order swapped vs the client's 4th..6th? [I] -- check char-select numbers) |
| +0x2e | c LAST byte | `writeC(0)`: l1j-en ignores its `accessLevel` argument and always sends 0 |
| +0x2f | not on wire: =1 if bit 0x20 of +0x1e was set (that bit is then cleared) | (never sent by l1j-en) |
- World entry (0x470b73): `mov al,[ebx+0x2e]; test; je` -> `0x4a5620(0x8ed548, 1)`: any non-zero byte sets the GM flag
  [0x8ed548] to 1. Before that the flag is zeroed (0x470b5b). So the GM flag is never set with l1j-en (always 0). [C]
- Entry count: S_CharAmount handler 0x489600: if [0x6f1da8] >= 0x13cd2 (81106) format "cc" = (count, max slots -> [0x81b5dc]),
  else "c" = count only. l1j-en sends `c value, c maxAmount` (value = characters on the account, maxAmount = default slots +
  bought slots). count==0 -> 0x474c30 (go to create-character). The list then arrives as `count` S_CharPacks packets
  (counter [0x81c63c] of [0x81c640]); after the last, 0x474c30 shows the list. [C]
- Entries are pre-set to 0xff in class/sex bytes (0x48965f) = "empty slot" marker. [C]

## 2. S_Ability (opcode 0x56 -> 0x488da0, fmt "cc" idx,value; idx<6; slot = 0x8ed558 + idx*12, protected ints)
l1j-en: S_Ability(type, equipped) writes C opcode, C type, C 0/1, C 2, H 0. Only caller: L1EquipmentSlot 142/205
`S_Ability(1, true/false)` for item 20288 (Ring of Teleportation Control, "ROTC"); the class comment says 5 = ROSC (ring of
summon control) but NO code sends 5. (RE_ROUND3's "l1j-en sends 1 and 5" is wrong: only idx 1.) Types 2, 3, 4 are never sent. [C]
- idx 2 (0x8ed570): read only at 0x408fc5, in the item-use path. If the item's use type [item+0x68]==0x27 (polymorph scroll
  class) and its id == [0x816748], and slot2 == 1 exactly -> [0x6f1f80]=1 and 0x409cf0 directly (opens HTML page `monlist`,
  0x4ffce4) without the 0x77 round trip (already in U-D9). Otherwise page named in [0x7011b4] or the typed-name prompt.
  Meaning: "polymorph list granted / free polymorph choice" ability. [C code, I name]
- idx 3 (0x8ed57c): 1..3 = see invisible (0x46689f, skips the whole dark overlay block; 0x465f60, 0x46526a) -- RE_ROUND3.
- idx 4 (0x8ed588): in the world draw 0x465c30 at 0x466935. Block (after drawing objects, before the HUD): runs when blind
  state [0x8ed528]==0 (see 4), the local player exists, and the light level (ebp-0x58) < 15 (or the 0x70311d toggle off).
  Then: if the elf infravision toggle [0x6f1e4b] (`/infracon`) is set and class [0x81b534] is 2 (elf) or 4 (dark elf) ->
  draw; else if toggle is off and slot4 == 1 exactly -> draw (class 4 uses function pointer [0x8246fc], others 0x49af00).
  0x49af00 post-processes the frame buffer in place (RGB565 pairs: `(px>>4)&0x08030803 | (px>>1)&0x03e003e0`, 0x198 rows x
  0x140 dwords, i.e. keeps mostly green): the night-vision/infravision green tint. So idx4 = **infravision** (dark-vision)
  ability; class 2/4 get it by their own toggle, other classes only through S_Ability 4 = 1. If neither -> falls to
  0x466993 (other overlay via [0x824700], flag set for maps 0x12f / 0x25e). [C draw, I "infravision" name]
- idx 5 (0x8ed594): read at 0x4adce6 = case 23 of the shortcut/item-use switch (jump table 0x4ae2e4; selector value 50 = 0x32,
  byte map 0x4ae3d4). Value 1..3 -> 0x409eb0: closes the monster page and opens HTML page **`summonlist`** (0x4ffcf4)
  on window [0x7012bc], sets [0x6f1e64]=1. So idx5 = Ring of Summon Control (summon-selection list); value outside 1..3 falls to the
  normal path (draws the shortcut using [0x8f07f2]). [C]
- idx 1: teleport control ring (RE_ROUND3).
- l1j-en items: only 20288 -> idx 1. Nothing for 2..5.

## 3. 1480 window (ctor 0x4d43f0, global [0x8f2fd4], vtable 0x4f5cfc) = COOKING window
- Feeder: opcode 0x3e (62, S_PacketBox) sub-type **52 = COOK_WINDOW** (handler 0x49135f; PacketBox table 0x4920f8 indexed by sub
  type, case 52). l1j-en: C_ItemUSe, item ids 41255..41259 (Cooking pans) with cookStatus==0:
  `S_PacketBox(COOK_WINDOW, itemId-41255)` = type 52, d 0x02df31db (constant junk), c 1, c value (0..4). [C]
- Client: dword at +0 (kept), sub-byte c at +4, mode byte at +5, mode := byte+1. sub 0 -> reply `cdd` 0x77 (C_ITEMUSE) with the dword
  and 1 (never sent by l1j-en); sub 1 -> if window open with same mode -> toggle closed; else open window with [0x8f2fa8]=mode. [C]
- Contents: frame image 1480 (+1481 overlay), picture 1482+mode (so 1483..1487 for the five pans) at window +(0x109,0x70), a
  scrolling recipe list (scroll callbacks 0x4d4ca0 up / 0x4d4d00.., 0x4d5110 sets offset*4, close 969), and a cooking progress bar: [0x8f2fac] = remaining
  half-seconds (timer 0x4d38d0 every 500 ms, set at 0x485400 from the spell effect object +0x104 * 2, total in [0x8f2fb0]);
  bar drawn by 0x4d4c40 (width = 10*fac/total...). Clicking a recipe (0x4d3900): no pan chosen ([0x8f2fb8]==-1) -> message
  0x48f/..; cooking in progress -> message; else sends `cdcc` = 0x77, pan item object id [0x8f2fb8], 1 (cookStatus), recipe index
  -> l1j-en C_ItemUSe reads `cookStatus c, cookNo c` for 41255..41259 -> makeCooking(pc, cookNo). [C]
- So it lists the recipes (dishes) of the pan type; no other opcode feeds it. Not the pet window. (U-D5 resolved)

## 4. Light flash
Two hold-full-brightness mechanisms; both set the light level slot [0x8ed538] to 15 and keep it via counter [0x7026d0] (the
day/night updater 0x4b2f00 skips while counter > 0); saved value in [0x7026cc], restored when counter returns to 0. [C]
- A: 0x450650 (class vtable 0x4f15f0, slot1 0x450790 / timer 0x450770), frame index [obj+0x13] == 2, NO flag test.
  Created only by opcode **0x23 (S_UseAttackSkill, handler 0x487780, fmt "cddccd"+"hchhhhcs")** when the sprite-list type
  `[0x70130c][gfx]` (list.spr token `109.effect(T x)`) is **2** (jump table 0x487ed4, case 0x48787e, ctor 0x450540). T=2 sprites in
  juego/spritelist/list.spz: gfx **10 lightning**, 1115 succubus v magic, 1119 succubus g magic, 1161 imp magic effect, 1214 orc mage
  effect, 4539 helvine deadfall, 11736/11737 renewal call lightning, 13209 new elite vampire att, 13850 inter thebe anubis lightning. [C]
  (list.spr per-frame markers `[n` sound, `<n` sound/effect, `!` = frame record byte+0x12 bit0 = hit/trigger: NOT the flash.)
- B: 0x450970 (vtable 0x4f15f8 slot 2, objects built by ctor 0x4507d0), frame [obj+0x13] == 1 AND [obj+0x12] != 0.
  [obj+0x12] = ctor stack arg 4 (ebp+0x14); the 0x4cb355 store is a different record (0x44-byte spawn record from the NPC pack:
  its +0x12 is the "light size" byte, arg 7 of fmt hhdhccccdhsscdsscccc) -- not this flag. Call sites with arg4 != 0 [C]:
  - 0x4ca0bc/106/150/19d/1ec: parent gfx 0x7d1 (2001 valakas) action 0x13 -> 5x child gfx 0x2fa (762 "meteor strike eff sc"), flag 5.
  - 0x4c8ab0: child gfx 0xd6 (214 "give fire"), flag 0xf (in the object tick 0x4c8970).
  - 0x48be7f: opcode 0x45 (S_EffectLocation, 0x48bdf0), gfx 0x7ec..0x808 (2029..2055 "cracker type .." / "fire stick type ..", fireworks), flag 1.
  - 0x491121: S_PacketBox sub-type 48 (0x30), random of 3 gfx starting 0x7ed (cracker), at each tracked object's tile, flag 1.
  - 0x4cd786: parent gfx 0x1a38 / 0x1a3e / 0x1a44 (6712/6718/6724 CubeQuake/CubeShock/CubeBalance eff), flag 1.
  - 0x450e81/ee0/f3c and the 0x4510xx block: children of gfx 0x71b (1819 firestorm) copy the parent's flag.
  All other spawns (about 80 sites) pass 0 -> no flash from B.
- Net: lightning-type spell effects flash at their 3rd frame; meteor strike (Valakas), give fire, fireworks/crackers, Cube* effects
  flash at their 2nd frame; flash lasts until the effect object ends (counter dec at end), overriding night darkness.

## 5. Audit items
- U-A12 Ctrl+T (0x421330, state [0x7030dc] 0..3, cycles every press; presses > 2 s apart fast-forward through 0x41fad0): chat
  area top y [0x500480] = 402 / 366 / 330 / 294 and visible lines [0x500484] = 5 / 8 / 11 / 14, line pitch 12 px (draw 0x41f215..0x41f540, ring buffer 150 lines).
  Frame image 0x417+mode (1047..1050); scroll arrows (972/974/976) move up 36 px per step (y 0x189/0x191 - 36*mode, x 0x1d6/0x1c1); the
  resize button image 0x415 -> after 2 s (timer 0x421300) 0x420; at mode 3 immediately 0x420 / 0x502. No extra backdrop. [C]
- U-C10 / death dialog. Esc menu = window [0x8ed3f0] (ctor 0x4a1180, toggled by Esc at 0x457352), 3 buttons at x 31: y 34 (imgs
  0x427 / hover 0x4fe, cb 0x4a0ba0), y 52 (0x429 / 0x4ff, cb 0x4a0fc0), y 70 (0x42b / 0x500, cb 0x4d1210 = close). [C]
  * Button 1 (0x4a0ba0) forks on the DEAD flag [0x702dcc]: dead -> sends `c` 0x52 = C_RESTART (82; l1j-en C_Restart = respawn at the
    town/getback location, HP := level, food 40) and clears the flag; alive -> refused with string idx 130 (0x82) if the player is
    alive and fought within 15 s, else sends `c` 0x10 = C_CHANGECHAR (16) = return to character select (after clearing windows, 0x459ed0). [C]
  * Button 2 (0x4a0fc0) = Quit game: returns at once if 0x456860(0x14) is true; refused with string idx 131 (0x83) when alive, not on a
    tile with flag 4, and [0x8f0df8]+15000 ms > now (combat); otherwise `c` 0x72 = C_QUITGAME (114). Dead players and flag-4 tiles may quit. [C]
  * Button 3 = close window. 
  * Death: the own-character death handler (0x457620) sets [0x702dcc]=1, auto-opens this same Esc menu if not open (0x4d1200), plays a
    resource sound (0x29a via [0x4f05bc]) and clears pending-teleport/ctrl state. S_Resurrection (opcode 0x2c, 0x488e10) clears the flag when the id
    equals the own id ([0x816748]). So the "death/restart dialog" is the Esc menu popped open; button 1 = Restart. [C code, I that no other dialog exists]
- U-A13 Blindness: S_CurseBlind (opcode 0x6e -> 0x488de0, reads only the first byte; l1j-en writes H) -> 0x409f00 stores the state in
  [0x8ed528] (0 none, 1, 2) and prints string idx 147 / 148 (colour [0x50051c], state 2 [0x50054c]). NO overlay image: the world draw
  0x4626c0 forces the light level to 0 while the state != 0 (`ecx = state ? 0 : light`, then 0x49c110) and the infravision block
  (idx 4) is skipped. [C]
- Ctrl+P message for GMs: not examined (flag [0x8ed548] has 40 readers, e.g. 0x4a4ec0 getter).
- Game Tip window ([0x7026f0], ctor 0x4badd0, img 808/811 234x180 area 0xe7 x 0x5e at (0xb0,0x63), scroll 0x199..0x19b buttons, close 0x329):
  created at startup (0x45914f), opened/toggled with 0x4d1200 by the HTML page action **`sendsmp`** (0x40c514 -> 0x40c53b; the sibling
  action `sendsm` sends `cd`, `checkpet` precedes). Scroll callbacks 0x455e30. Not opened by a hotkey or login. [C]
- Not done (budget): U-A8 zone label colours, U-A9 game-time orb, Ctrl+P GM message text.

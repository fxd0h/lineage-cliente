# Round 5 RE (S3EP1U.bin) -- tags: [C] confirmed in code, [I] inferred

## 1. U-A9 game-time orb (HUD draw 0x416160) and U-A8 zone label (0x4d2410)
- Orb: 12 groups of 2 game hours. Table 0x500228 (stride 16) = {first image, frame count, x, y} for 1237(2) 1239(2) 1241(3)
  1244(2) 1316(2) 1318(2) 1213(2) 1215(2) 1217(5) 1222(3) 1225(2) 1227(5) (ids = 0x4d5 0x4d7 0x4d9 0x4dc 0x524 0x526 0x4bd 0x4bf
  0x4c1 0x4c6 0x4c9 0x4cb). Group = `((tm_hour + 23) % 24) / 2` with `tm_hour` from 0x4bad30 (gmtime of the game-time dword
  [0x8f07d4]); the resting image is the group's LAST frame (first + count - 1), drawn at (hx+0x127, y 0x1e1 - (h1042 - 100) -
  34 * ctrlT) = (295, 359) at Ctrl+T 0. [C]
- Transition: S_GameTime (0x4876a0 tail) sets [0x8f0809]=1 and the tick [0x8f080c] when the new hour is odd and minute 0. Then:
  t < 900 ms the previous group's image (first - 1; special cases 0x4bd -> 0x527, 0x524 -> 0x4dd), after that one frame per
  150 ms up to the last, flag cleared once t >= 1350 and t >= (count * 3 + 18) * 50. Extra sparkle images 0x4e3 + t/100 (at the
  table x, y, drawn through the blit pointer [0x8ed3d4]) and 0x4dc + t/150 over the orb for 450 < t < 1050 ms: NOT implemented
  (blend mode of [0x8ed3d4] not decoded). [C code, sparkle not ported]
- Chat backdrop 1042 (0x73,0x1e0 - (h-100)), wings 1044 (y+1) and flourish 1043 (0x79, 0x173) ALSO rise 34 px per Ctrl+T
  step (they share local_18 with the orb). The fork drew them fixed; now fixed. [C]
- The old fork mapping (34 linear frames 1213..1246) was a template-match guess; the 34 images are the 12 groups above.
- Optional 0x8f080a flag prints `"%02d:%02d"` (tm_hour, tm_min) of the game time at (hx+295, y+0x1ea): a debug clock; flag writer
  not examined, not drawn.
- Zone label object [0x8f2f8c] (ctor 0x4d1f40, built at 0x457f00; font CreateFont height 12, weight 500, "Times New Roman",
  zone2*.tbl colour/zone loader 0x4d20a0 chosen by the locale enum [0x702b7c]) is drawn by 0x4d2410 from the world draw 0x467c83 (only when
  neither [0x8f30b1] nor [0x8f307c] overlays are active). [C]
  * Zone TYPE is an icon, not text: cell left attribute word bit 4 -> image 1226, bit 8 -> 1224, else 1225 (30x31, US
    branch; the 936/950 locales use other ids 0x22e..0x230 / 0x26a..0x26c), blitted by 0x498b10 at (hx + 0x212 + off, y + 0x159) =
    (530, 345); `off` = return value of 0x4ae5b0, which is 0 when idle (other value not traced). The fork's "Safety zone" text was invented.
  * Zone NAME (record [obj] + idx*64, class byte +0x28 picks colour [obj + 0x14 + 4*class]) is drawn by 0x4d2bb0 with GDI
    ExtTextOut: 1 px outline (x-1, x+1, y-1, y+1) in the CLASS colour (SetTextColor [ebp+0x24]) and the fill in 0xffffff, at
    (hx+16, y+0x15e = 350). The class colour is the zone table's colour line (COLORREF r g b order). [C]

## 2. Ctrl+P (0x402680, callers 0x456d77 / 0x456dad hotkey, 0x459e83 / 0x459eaa window message)
- Non-GM (flag [0x8ed548] != 1, always the case with l1j-en): creates `"%sCapture"` (CreateDirectory, 0x4ff3b4), builds
  `"%sCapture\%s%04d.bmp"` (0x4ff39c) with prefix "LinC" (0x4ff20c) and counter [0x6f1e54] (starts 0, post-incremented, so
  the first file is LinC0000.bmp and each run overwrites the previous one), `%s` = the exe's base folder [0x700c30]. Writes a 640x480 24-bit
  BMP (0x96036 bytes) from the primary surface (565 -> 555 conversion when [0x6fa678]) and prints `"Saved current screen to %s"`
  (0x4ff36c, the full path) through the chat adder 0x41f0a0 in white (RGB555 0x7fff, [0x500528]). [C]
- GM (flag == 1): same file but silent, then opens a text prompt ([0x702dd8] vtable +0x64, empty strings, callback 0x457600). OK with text
  -> 0x402590 renames the file to `"%sCapture\%s.bmp"` (0x4ff388) with that name, prints the message with the new name and
  decrements the counter; cancel -> closes the prompt (0x41f1d0). Not modelled (flag never set, see RE_ROUND4 section 1). [C]
- Fork: original UI now writes `<settings folder>/Capture/LinC%04u.bmp` (counter per run) and prints the exe's line in white.
  The simple UI keeps `captura.bmp`. Shift+F12 uses the same routine (the exe's 0x459e83 window-message path calls the same function). Verified live.

## 3. Mail compose window (class 0x453120, global [0x7026fc], vtable 0x4f1660, draw 0x453750, send 0x453c60)
Created at world entry (0x458c62, id 0x16); the same paper 1545 (252x370) is shared with the view variant 0x4541b0 ([0x7026f8]).
So the real composer is 0x453120 (paper 1545), NOT 0x455800 (paper 577, `ccss`, RE_WINDOWS round 3). [C]
- Children (list [obj+8], ids): 0 Send, 1 Cancel, 2 edit [+0x18] (100,55) 116x16 ("From" printed on the paper), 3 edit [+0x1c] To
  (100,70) 116x16 (limit 0x82, names <= 16), 4 edit [+0x20] Subject (100,90) 116x16 (limit 0x14 = 20), 5 edit [+0x24] body (50,112)
  170x192 multi-line (limit 0x100 = 256), 10 close, 11/12 mode buttons. Ink colour [0x500510]. [C]
- Buttons (`0x47c6d0` image buttons, `0x47c950` text/tab buttons, `0x47c2d0` plain): Send 1580/1582 at (127,308) cb 0x453c60; Cancel
  1546/1548 at (163,308) cb 0x453a80 (closes the window, resets the list: the list does NOT reopen); close 1583 at (230,5) cb 0x453a80;
  tabs at y 31: x 55 imgs 1564/1566 cb 0x454060 (close, open the inbox list via 0x480ea0: private list, or the clan list when mode 5),
  x 118 imgs 1571/1573 cb 0x4536b0 (`ret`, current tab; the selected frame 1573 is drawn over it, 0x453986), x 174 imgs 1561/1563 cb 0x454090
  (0x480e40: opens the storage list, C_Mail type 2); mode buttons at y 340: x 24 imgs 1549/1551 cb 0x4540c0 (mode 2 private),
  x 53 imgs 1552/1554 cb 0x454100 (mode 5 clan, only with a clan name [player+0x64]; otherwise string 1262 "You are not a member of a
  clan..."; the clan name goes into the To edit, locked). The mode byte is [0x81c5ec] (2 private, 5 clan; 0 / 3 = inbox / clan list, 8 = storage). Selected
  mode frame 1551 / 1554 drawn on top; fee text `"%d %s"` (0x6da514) = 1000 (clan) or 50 with desc-e entry 4 (Adena) at (50,310) in black (0x453971). [C]
- Send 0x453c60: C_Mail (opcode 0x7f, `c type h 0 s receiver s subject s body` via 0x4a7df0), type 0x20 private / 0x21 clan. Checks, with
  string-e entries (entry n = file line n + 2): no name 434 "There are no recipients.", more than [obj+0x14] (= 1) names 433, name over 16 435 "Name is too long.",
  empty subject or body 1251 (line 1253) "You did not enter the letter's header or content.", own name 1252 "You cannot send a letter to yourself."
  (unless own name == the second player string [+0x64]), clan mail with 50 or more letters in the clan list 400 + 1261 (not modelled), a name check
  0x401b00 (not modelled; the server validates). Messages go to the chat in white. The list's Write opens it in mode 5 when the clan list was showing
  (0x480e0b) else mode 2 (0x480e14). A letter's Reply (0x4555b0) opens it with To = the sender (not wired: the reading window of the fork has no Reply). [C]
- Not determined: nothing writes the From edit (+0x18); left blank. Window position: assumed (0,0) like the list (the default position table was not traced).
- Fork: original UI compose rebuilt on paper 1545 with the layout above (To locked to the clan name in clan mode), subject 20 / body 256 chars, Cancel closes
  the whole mail window. Simple UI keeps its own compose. Verified live: layout, validation messages (no clan, self), Cancel.

## 4. Buff slot flags (S_SkillSound 0x48a5c0, own-character wrapper 0x48bbd5..0x48bdb4)
- The exe keeps one byte per buff slot, `[0x81680c + slot]` (1 while it runs; set by 0x4647a0 `(duration, slot)`, cleared by 0x464760 `(slot)`,
  which also hides the icon and prints NO end-of-effect message). Every `[0x8168xx]` flag read in the handlers is such a slot byte:
  0x816830 = slot 0x24, 0x816844 = 0x38, 0x81684e = 0x42, 0x816850 = 0x44, 0x816837 = 0x2b, 0x816859/5a = 0x4d/0x4e, 0x81685b/5c/5d = 0x4f/0x50/0x51. [C]
- gfx 0x1130: slot 0x42 if slot 0x42 already runs (0x81684e), else slot 0x38; gfx 0x16c1: slot 0x38 if slot 0x38 runs (0x816844), else slot 0x42 and
  the chat line string-e 1066 (0x42a). Slots 0x38 / 0x42 share icon 2339; 960 s / 100 s, tooltip 1012 / 1083 ("MP and HP still recover even when you are
  over your weight limit."). gfx 0xf47 first clears slot 0x44 when it runs (0x48ac73) and starts 0x24; gfx 0x16c5 (slot 0x44, line 1068+) does nothing while
  slot 0x24 runs (0x48ad93). The fork mapped 0x16c1 to 0x38 unconditionally: fixed (`ranura_de_skillsound`, mundo.cpp). The start chat lines that
  the exe prints for these slots (0x48b679: 1066 etc.) are NOT printed (l1j-en never sends these graphics; the end/start text plumbing differs). [C]
- Own S_SkillSound gfx 870 (0x366, Cancellation) runs a client-side purge after the normal handler (0x48bc15): slot 0x4d, else 0x4e if running;
  when slot 0x2b is NOT running: slots 0x0c..0x3a except 0x2c, and 0x42..0x48 except 0x46 / 0x47; then slot 3; 0x53..0x5a; 0x5d..0x64; 0x88..0x8f;
  0x65; the first running of 0x51 / 0x4f / 0x50; 0x6b 0x6c 0x6f 0x71 0x72 0x74 0x75 0x78 0x7b 0x7c 0x7d 0x80 0x81; 0x84; 0x99. All silent. Implemented
  (`cancelacion_propia`, unit test in test_hechizos.cpp). [C]
- Slot flags used in S_SkillSound gfx 0xbe (0x816832 = 0x26, 0x81685b/5c/5d) were already modelled.

## 5. Shout direction icon, grey overlapped bubbles, cursor states 8 / 14 / 16
- **Shout direction (S_Say 0x483500, type 2).** The packet ends with `h x, h y` (the speaker's tile). The client computes dx = x - ownX, dy = y - ownY
  (the exe reads own x from obj+0x30 as a doubled coordinate: (+0x30 - 0x8000) / 2) and, unless both are 0, picks a digit '0'..'7' from
  deg = atan(dy / dx) * 180 / pi (+-90 when dx = 0), thresholds from the doubles at 0x4f2610..0x4f2648: dx >= 0: deg > 70 -> 7, > 15 -> 0, > -20 -> 1,
  > -65 -> 2, else 3; dx < 0: deg > 75 -> 3, > 20 -> 4, > -17 -> 5, > -73 -> 6, else 7. The chat line (not the bubble, which is created before)
  becomes `"\d" + digit + text` (0x4836e5..0x48374d). The chat text blitter 0x432aa0 (at 0x432cc9) treats `\d<0-7>` as an icon: image **605 + n** (12x12, ids
  605..612) blitted at the pen position (0x499080), pen += 12. [C]
  Fork: `direccion_grito`, `LineaChat::dir`; the decoder now reads the shout's `h x, h y` (`CH_X/CH_Y/CH_CON_POS`); the original-UI chat draws the icon before the
  first row (12 px of its width). Unit tested (the live server only echoes own shouts, which carry dx = dy = 0 and so no icon).
- **Grey overlapped bubbles (0x41a8e3, in the bubble constructor/queue 0x41a600).** The bubble list is [0x6f5d28] (count [0x6f5d20]); each bubble keeps its
  rectangle at +0x384 left, +0x388 top, +0x38c right, +0x390 bottom and its colour word at +0x39c. When a new bubble is made, every OTHER bubble whose rectangle overlaps
  (strict comparisons) gets +0x39c = [0x50054c] = 0x4a52 (RGB555, 148,148,148) for the rest of its life. [C] Fork: `GloboChat` stores its last drawn rectangle
  (text block, +5 px frame padding when boxed: the exe's exact rectangle formula was not extracted, [I]); on a bubble's first drawn frame older overlapping
  ones are flagged `gris` and draw in (148,148,148). Not seen live (needs two speakers).
- **Cursor states (chooser 0x469e40, setter 0x477c90), still not selectable:**
  * 16 (resource 150): the own object's +0x12c pointer non-empty (0x469e53). That field belongs to the object created by the action 0x47 handler at 0x487064
    (it frees the object, then the handler sets state 3 again at 0x4870d6): an in-progress action overlay (fishing bobber [I]). The fork has no fishing.
  * 8 (resource 136): (a) a pending use ([0x81654c]) with no Ctrl / Alt-held flag ([0x6f1dac], [0x702dd6]) and the target within |dx| <= 2 * [0x816548],
    |dy| <= [0x816548] of the own object = "target in range" circle (out of range: 11, 0x46a344); (b) attack mode ([0x703105]) while the selected shortcut
    ([0x8f07b0] + 0x70) is 0x28 or 0x3c, on kinds 2/3/5/10/11, instead of 15. The fork lacks both the use range ([0x816548] writer not traced) and the meaning of those
    two shortcut values.
  * 14 (resource 143): sprite kinds 7 / 13, and kind 8 (door) when the tile flag bit 2 is set. The fork has no tile-flag or kind 7 / 13 data for the cursor.
  Decision: left unimplemented (the existing states 1/3/5/6/7/11/13/15 stay).

## 6. Cooking window (ctor 0x4d43f0, global [0x8f2fd4]): data source found, layout NOT finished (not implemented)
- Recipes come from **`recipe.tbl`** in Text.pak (string at 0x6e5404, loaded by 0x4d3130 through 0x430b00): line 1 = count (27), then per recipe
  `"name" "title" cookNo type nIngredients ("ingredient" qty)*`, all names being `$N` desc-e ids resolved with the desc table [0x8f0af8]
  (name = desc 4928 + the dish word in the "title"). Records (0x124 bytes) with cookNo < 0x30 go to the window's list (0..23), the cookNo 80 / 81 / 82 ones (>= 0x30) to a second
  vector (use not traced). The 24 rows are created by 0x4d47b0 from the static table at **0x6e52e8** (24 x 12 bytes): `{u16 dish icon gfx (0xa34, 0xa12, 0xa31, 0xa10, ...),
  u16 0, u32 string-e index of the effect line (1144.., 1211.., 1394..: "Ingredient: 1 monster eye meat (Attribute resistance power increases by 10.)"),
  u32 tier 1 / 2 / 3 (8 rows each)}` through the row factory 0x4d3a20 (-> 0x4458f0, the same row class as the other item lists). 0x4d43c0 shows the first `8 * mode` rows
  (mode = pan 1..5, so pans 1 / 2 / 3+ show tier 1 / 1-2 / all 24 rows). Row index = cookNo sent in `cdcc` (l1j-en `recipes[cookNo]`, 19 entries
  server side, so cookNo >= 19 would index out of bounds there). [C]
- Window: frame 1480 at the window origin, close 969 at (165,18), drag handle callback 0x4d5130, scroll bar 0x47cd80 (images 971..976, callbacks 0x4d5110 / 0x4d4ca0 / 0x4d4d00),
  picture 1482 + mode at (+0x109,+0x70), progress bar 0x4d4c40 at (+0x104,+0x20) with image 1495 scaled by remaining / total half-seconds. [C]
- Not determined: the visible row count and geometry of the scrolling list (the row class 0x4458f0 arguments, text positions inside a row, how the ingredient list is
  drawn), where the dish name is shown, and the exact scroll step. Without those the window cannot be reproduced, so it stays unimplemented (the server sends type 52,
  the fork ignores it). Next step when picked up: decompile 0x4d4840 (draw), 0x4458f0 / 0x451dd0 (row add) and 0x47cd80 (scroll bar).

> **Reverted (U-A8/U-A9 implementation of d6d399d):** in the live client the new orb groups drew a yellow sun orb and the
> zone-type "icons" 1224-1226 drew a second orb at (530,345) instead of the "Normal zone"/"Safety zone" text, which
> contradicts the original's screenshots that the previous implementation was matched against. The findings above are
> kept as unverified notes; the screenshot-matched 34-frame orb and text zone label are back.

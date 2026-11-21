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

# Round 7 RE (S3EP1U.bin) -- tags: [C] confirmed in code, [I] inferred

## 1. Chat bubble rectangle and grey overlap test (constructor 0x41a600, rect recompute 0x419e70, wrap 0x41a040)
Bubble object (0x3a0 bytes). Fields: +0x36c relL, +0x370 relT, +0x374 relR, +0x378 relB (= 0), +0x37c anchor X, +0x380 anchor Y, +0x384 / +0x388 /
+0x38c / +0x390 = screen rect left / top / right / bottom, +0x369 line count, +0x368 boxed flag, +0x39c colour word. [C]
- Text block: 0x41a040 wraps the text with a first limit `w0 = 20` cells (16 when the option byte [0x6f5d2c] != 0; that byte is a config value set at 0x421e41),
  returns the line count `L` (+0x369) and rewrites `w` = the widest wrapped line in cells. One cell = 6 px (a byte >= 0x80 counts as 2 cells). [C]
- Unboxed (`[0x6f5d2c] == 0` and kind != 5): relL = (w * -6) / 2 = -3w, relT = L * -12 - 6, relR = (w * 6) / 2 = 3w, relB = 0. [C]
- Boxed (`[0x6f5d2c] != 0` or kind == 5): relL = -5 - 3w, relT = L * -12 - 14, relR = 3w + 5, relB = 0. [C]
- Anchor: `ax = X + 0x18` where X is the speaker's screen x from the world->screen helper 0x497fe0 (dx = x - [0x816750], dy = y - [0x816754];
  X = 12 * (((dx + 1) & ~1) + 2*dy - 2)), `ay = frameTopY + Y - 2` where frameTopY = `[[obj+0x88] + 0x14 + [obj+0x19] * 0x18]` (the current sprite
  frame's top offset, negative [I]) and Y the helper's second output. [C code, frame field meaning I]
- Rect (0x419e70, run on creation and when drawn): `left = ax + relL`, `top = ay + relT`, `right = ax + relR`, `bottom = ay`. So unboxed
  size = 6w x (12L + 6); boxed 6w + 10 x (12L + 14); bottom edge sits on the anchor (head top). [C]
- Screen clamp (same function, 640x480 view): if left < 0x30 shift right by `2 - left`; else if right > 0x2b0 shift left by `right - 0x2b0`; if top < 0x30
  push the rect down under the sprite (top = sprite foot y + 4... exact: top += (frame bottom [+4 of 0x497fe0 result] + [obj+0x88][0x1c + ...]) - top, flag +0x36a = 4);
  the bubble is dropped (not shown) unless top >= 0x30 and bottom < 0x1c9 (457). [C code, the downward-flip detail I]
- Grey test (0x41a8e3, new bubble N): for every other bubble B in [0x6f5d28] (count [0x6f5d20]):
  `B.left < N.right && B.top < N.bottom && N.left < B.right && N.top < B.bottom` (all strict) and B.colour != [0x50054c] -> `B.colour = [0x50054c]`
  (0x4a52 = RGB555 (148,148,148)); permanent for B's life. N is never greyed by this pass. [C]
- Not extracted: the text blit position inside the rect (draw callback queued by 0x4ba8e0). Use rect.left (unboxed) / rect.left + 5 (boxed), top + 3 / top + 7 [I].

How to implement: on bubble creation compute w (cells, max 20 or 16), L; rect = [ax - 3w, ay - 12L - 6, ax + 3w, ay] (boxed: [ax - 3w - 5, ay - 12L - 14, ax + 3w + 5, ay]);
ax = speaker tile centre x + 24, ay = sprite-frame top - 2. Then loop older bubbles with the strict 4-compare test and set their colour to (148,148,148) once.

## 2. Mail compose window (ctor 0x453120, global [0x7026fc]) position and From edit
- Position: the window origin is the child list's `+0x14 / +0x18` (list ctor 0x422570 sets both to 0 and nothing else writes them: the open path
  0x4d0f30 -> vtable+0x20 = 0x453a00 only resets/shows children, the ctor has no drag handle, no table). Draw adds the view origin
  `[0x816698] / [0x81669c]` (0 for a 640x480 view; written at 0x45f7b2, 0x490ee1, 0x4c8268). So the window sits at **(0, 0)** like the mail list. [C for "no writer"; I that nothing outside the traced paths moves it]
- From edit (+0x18): written by the open handler 0x453a00 (vtable+0x20, runs on every open via 0x4d1200): `edit[+0x18].setText([[0x8f0e4c] + 0x5c])`
  (0x453a19..0x453a22). `[0x8f0e4c]` is the own player record; +0x5c is the own character name [I] (+0x64 is the clan name, used right after). [C code, name meaning I]
  Same handler: mode byte [0x81c5ec] == 5 (clan) -> To edit [+0x1c] gets `[player + 0x64]` (clan name) and its flag +0x70 = 2 (locked); else +0x70 = 0 (editable, text untouched).

How to implement: place paper 1545 at (0,0); on every open fill From with the own character name (read-only); clan mode fills To with the clan name and locks it.

## 3. Game-time orb and zone label: both earlier readings were mixing two image tables
- **Two different image tables.** The orb draw 0x416160 indexes `[0x8f0b1c] + id*4` ("Surf Resource Error[%d]", the main UI/sprite image set). The zone-type
  blit in 0x4d2410 indexes `[0x701450] + id*4`, count [0x701454], error string 0x5013bc = **"Icon Resource Error[%d]"**: that is the icon table built at
  0x442640..0x4427f0 from `list.ico` + `%d.tbt` (i.e. the N.tbt inventory-icon files in SpriteNN.pak). [C]
- So zone images 1224 / 1225 / 1226 are **N.tbt icons 1224/1225/1226, NOT UI image ids**. In the UI image table the same ids are orb frames
  (group 9 = 1222..1224, group 10 = 1225..1226), which is exactly the "second orb at (530,345)" seen live: the fork loaded them from the wrong table. [C]
- 0x4d2410 branches on `[0x6e4fc8]` (an ANSI codepage: set from a config value at 0x421df7, else `GetACP` at 0x4b73b5): 950 -> icons 0x22f/0x22e/0x230
  at y 0x163; 936 -> 0x26c/0x26a/0x26b at y 0x163; any other (1252 = US) -> **1226 / 1224 / 1225 at (hx + 0x212 + off, y + 0x159) = (530 + off, 345)**, `off` = return of
  0x4ae5b0 (0 when idle). Selection: tile attribute word bit 4 (0x4 of `bl`) -> 1226, bit 8 -> 1224, else 1225. So 0x4d2410 does run for US: it is the default branch, not another locale. [C]
- Image content not inspected [I]: the original screenshots show right-aligned text "Safety zone"/"Normal zone"/"Combat zone" ending x ~ 626 = 530 + 96, so
  N.tbt 1224..1226 are most likely ~96 px wide label pictures with the text baked in (round 5's "30x31" came from the Surf table). Verify by decoding 1224.tbt (expect ~96 wide).
- Zone NAME (round 5, 0x4d2bb0 at (16,350)) is separate and unchanged.
- **Orb 0x416160 is real for this exe** [C]: table 0x500228 (stride 16, {first, count, x, y}) 12 groups: (1237,2,305,381) (1239,2,298,379) (1241,3,293,374) (1244,2,292,367)
  (1316,2,293,360) (1318,2,298,354) (1213,2,305,352) (1215,2,313,354) (1217,5,318,360) (1222,3,319,367) (1225,2,317,374) (1227,5,312,379). Group g = `((tm_hour + 23) % 24) / 2`;
  resting image = first + count - 1 = g0..g11: 1238 1240 1243 1245 1317 1319 1214 1216 1221 1224 1226 1231, drawn at (hx + 0x127 = 295, 0x1e1 - (h1042 - 100) - 34*ctrlT) (= 359 at default). `tm_hour` = hour of
  the game time dword [0x8f07d4] through 0x4bad30. Group 0 = hours 1-2, group 5 = 11-12, group 6 = 13-14, group 11 = 23-0. The y values in the table are the sparkle positions (same -34*ctrlT shift).
  The 1213..1246 "34-frame linear" model is the union of these ranges (1232..1236 and 1246 are unused here). The code cannot show "frame 0 = noon": noon (hour 12) rests on 1319. If the original
  screenshots really show 1213 at noon, the client's `tm_hour` differs from the server clock (S_GameTime is relative to the 0x879e0800 epoch shift in 0x4bad30) -> compare with the server's game time before changing the table. [C code, mismatch cause I]
- Transition and sparkle (S_GameTime 0x4876c0 tail: when tm_hour is odd and tm_min == 0: [0x8f0809] = 1, [0x8f080c] = now ms), t = now - [0x8f080c], first/count of the NEW group:
  * t <= 1349 (0x545): base image = previous group's last frame (first - 1; special cases first 1213 -> 1319 (0x527), first 1316 -> 1245 (0x4dd)); for t >= 900: `min(first + (t - 900)/150, first + count - 1)`.
  * sparkle A, every frame while t <= 1349: image `1251 + t/150` (0x4e3..) at (tbl.x, tbl.y - 34*ctrlT) through blit pointer [0x8ed3d4].
  * sparkle B, only for 449 < t < 1050: image `1244 + t/150` (0x4dc.., so 1247..1250) at (295, 359-equivalent) through [0x8ed3d4], over the orb.
  * t > 1349: frame `first + (t - 900)/150` clamped to last, same position; flag cleared when t >= (count*3 + 18) * 50 ms.
  * [0x8ed3d4] is an indirect blit (blend mode not decoded; additive glow [I]).
  * Without the transition flag (flag 0) only the resting image is drawn, no sparkle.

How to implement: orb = group table above, rest on last frame; zone type = load icon (N.tbt) 1226 / 1224 / 1225 (attr bit 4 / bit 8 / else) from the icon table (same path as inventory icons)
and blit at (530, 345) with no extra text; do not read 1224..1226 from the UI image table.

## 4. Buff start chat lines
- Slot table 0x6e1020, 20 bytes per slot: +0 icon, +4 seconds, +8 exclusion group, +0xc tooltip string index (read at 0x47e238), +0x10 end-message string index
  (read ONLY at 0x464911, in the blink/expiry timer 0x4648c0, printed through 0x41f0a0 with colour [0x500528] when the slot flag [0x81680c + slot] is 0 on expiry). [C]
  There is **no start-message column**; `0x4647a0 (seconds, slot)` itself prints nothing. [C]
- Start lines are hard-coded per S_SkillSound gfx handler: handlers that tail-jump to 0x48b679 call 0x4647a0 and then print string `[0x8f0b0c][esi]` (string-e entry esi, colour [0x500528]);
  handlers that `call 0x4647a0` directly are silent. [C]
- gfx 0x1130 (slot 0x38 / 0x42): **silent** in both branches (0x48acd4..0x48ad63). gfx 0x16c1: if slot 0x38 runs (0x816844) restart 0x38 silently; else start slot 0x42 and print **string-e 1066 (0x42a)** (0x48ad6d..0x48ad7d). [C]
- Other (slot, start-line) pairs from the same tail (esi = string index): 0x35 -> 0x3f1; 0x40 -> 0x413; 0x41 -> 0x414; 0x44 -> 0x42c; 0x43 -> 0x42b; 0x45 -> 0x42d; 0x48 -> 0x430; 0x47 -> 0x42f; 0x46 -> 0x42e;
  0x2f -> 0x2ae; 0x2d -> 0x2b1; 0x31 -> 0x2b9; 0x33 -> 0x2b4; 0x32 -> 0x2b7; 0x30 -> 0x2ba; 0x2b -> 0x2bc; 0x0e -> 0x2bd; 0x0f -> 0x2c0; 0x11 -> 0x2bf; 0x37 -> 0x2c8; 0x10 -> 0x2c0; 0x1c -> 0x2e1; 0x1e -> 0x332;
  0x21 -> 0x334; 0x1f -> 0x335; 0x20 -> 0x336; 0x99 -> 0x5a5; slots 0x6b..0x83 -> 0x549..0x561 (0x6b 0x54b, 0x6c 0x54c, 0x6d 0x54d, 0x6f 0x54f, 0x70 0x550, 0x71 0x551, 0x72 0x552, 0x73 0x553, 0x74 0x554, 0x76 0x556,
  0x77 0x557, 0x78 0x558, 0x79 0x559, 0x7a 0x55a, 0x7b 0x55b, 0x7c 0x55c, 0x7d 0x55e->slot 0x7e: 0x55d, 0x7f 0x55e, 0x80 0x55f, 0x81 0x560, 0x82 0x561, 0x83 0x549). The scan was automatic: re-check a pair against
  the disassembly (0x48a5c0..0x48b6a8) before relying on it. [I for the automatic pairing]

How to implement: for 0x16c1 on a fresh slot 0x42 add chat line string-e[1066] in the normal chat colour; print nothing when 0x38 is restarted or for 0x1130.

## Implementation status
- Section 1 DONE (`dibujar_globo`, pantalla_juego.cpp): rect, horizontal clamps, drop rule, strict overlap grey test. Overlap runs on the bubble's first drawn frame (not at creation); text rows start at rect top (+5 boxed); the top-clip flip uses the sprite anchor y + 4 [I].
- Section 2 DONE (correo_ui.cpp): From edit (100,55) shows the own name on every frame of the compose window; clan mode keeps To = clan name, locked.
- Section 3 DONE for the zone label: icon-table pictures 1224 / 1225 / 1226 drawn live and confirmed to be the "Combat zone" / "Normal zone" / "Safety zone" label art (text baked in); the text label was replaced (screenshot /tmp/r7_bubble_zone.png). The `off` shift from 0x4ae5b0 is not modelled. Orb untouched.
- Section 4 DONE (`mensaje_inicio_ranura`, tabla_buffs.h; queued with the end-message mechanism, due immediately): all 50 slot -> string pairs re-extracted from 0x48a5c0..0x48b6a8 (`jmp 0x48b679` tails) and match the table above; the only silent direct calls on printing slots are 0x1130 (0x42/0x38) and 0x16c1 restarting 0x38. String 1066 = "MP and HP still recover even when you are over your weight limit." Not live-tested (needs the server to send the S_SkillSound); unit test covers the table.

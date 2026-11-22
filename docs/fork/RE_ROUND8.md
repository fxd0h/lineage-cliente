# Round 8 RE (S3EP1U.bin) -- tags: [C] confirmed in code, [V] verified against original captures / video, [I] inferred

## 1. Game-time orb (0x416160), re-read and implemented with its transition and sparkles
- Which table: every id in 0x416160 indexes `[0x8f0b1c] + id*4` (UI images, g.img(), error string "Surf Resource Error", count 0x70c). The ids are NOT icon-table ids. [C]
- Draw order: 1042 at (hx + 0x73, vy + 0x1e0 - (h + 34T - 100)) through [0x8ed3cc], 1044 one pixel lower (0x4991e0), 1043 at (hx + 0x79, vy + 0x173 - 34T) through
  [0x8ed3d0], then the orb. T = `[0x7030dc]` = the Ctrl+T chat size (0..3); h = height of image 1042. So backdrop, wings, flourish and orb all rise 34 px per size step. [C]
  (The fork now does this too: `dibujar_panel` shifts 1042 / 1043 / 1044 and the orb by 34 * chat_tam.)
- Hour: `[0x8f07d4]` (the S_GameTime dword) -> 0x4bad30: if the dword >= 0x80000000 it adds 0x879e0800 (mod 2^32) and calls the time import `[0x4f031c]` (gmtime per RE_ROUND5;
  the import table of this packed dump is empty, so gmtime vs localtime is [I]) and adds 64 to tm_year. The server sends `(int)seconds` of its 6x clock, which is > 2^31 today,
  so the shift matters: effective seconds = `v + 0x879e0800` mod 2^32; hour = (eff / 3600) % 24, minute = (eff / 60) % 60. The fork's light code uses the signed value (`v mod 86400`),
  which differs by 512 s; only the orb uses the exact 0x4bad30 form for now. [C]
- Group g = ((hour + 23) % 24) / 2 over the table 0x500228 (stride 16: first, count, x, y): hours 1-2 -> 1237(2), 3-4 -> 1239(2), 5-6 -> 1241(3), 7-8 -> 1244(2), 9-10 -> 1316(2), 11-12 -> 1318(2),
  13-14 -> 1213(2), 15-16 -> 1215(2), 17-18 -> 1217(5), 19-20 -> 1222(3), 21-22 -> 1225(2), 23-0 -> 1227(5). Rest image = first + count - 1, at (hx + 0x127, vy + 0x1e1 - (h + 34T - 100)) = (295, 359 - 34T). [C]
- Transition: the S_GameTime handler 0x4876c0 sets `[0x8f0809] = 1` and `[0x8f080c] = tick` EVERY time a packet arrives while the new hour is odd and the minute is 0 (so the animation re-arms for
  as long as the server keeps sending the time during that game minute: ~10 s real). t = tick - [0x8f080c]. [C]
  * t < 900: image first - 1 (1213 -> 1319, 1316 -> 1245 special cases); 900 <= t < 1350 and t >= 1350: first + (t - 900) / 150 clamped to the last frame; the flag clears at t >= 1350 and t >= (count*3 + 18) * 50. [C]
  * Sparkle A while t < 1350: image 1251 + t/150 at (hx + tbl.x, vy + tbl.y - 34T). Sparkle B for 450 <= t < 1050: image 1244 + t/150 at the orb position. [C]
  * Both sparkles go through `[0x8ed3d4]`: set at 0x4a09fc (565 build, [0x6fa678] != 0) -> 0x49c8a0 -> 0x49c380, else (555) 0x4a0b0e -> 0x49c870 -> 0x49c170. The routine skips colour-key pixels and
    replaces each destination channel by a table lookup `T[src5 * 32 + dst5]`, tables 0x8cc714 / 0x8ccf14 / 0x8cd714 (blue / green / red, pre-shifted), built by 0x4a03b0 as
    `max + min * (31 - max) / 31` (constants 31.0 and 1/31 at 0x4f27a8 / 0x4f27a0): a **screen blend**, colour-keyed. The fork's `Mezcla::PANTALLA`. [C]
  * `[0x8ed3d0]` (chat flourish 1043) uses the same three tables through 0x49da40, i.e. also a screen blend. [C, not touched]
- Debug flag `[0x8f080a]` prints "%02d:%02d" at (hx + 295, y + 0x1ea): not drawn.
- Day/night sound switch in the same function (image index 1223..1240 -> `[0x700b0c]` = 2 else 1): not examined.
- [V] The original video (original.mp4): rest image before t = 1018 s is group 0 (1238), and from 1018 s on, the transition to group 1 (1239 -> 1240) plays with the sparkle at the bottom-left of
  the orb (table position (298,379)) and the screen-blended glow streak over the orb; it restarts every ~2 s because the packet repeats. A fork capture after `forzar-hora 10800` reproduces the same frame
  sequence (/tmp/r8_orb.png: video row on top, fork row below). The 12 rest frames were rendered one by one (hours 1..23): hour 1 matches the video's first half and hour 3 its second half.
  Earlier "1213 = noon" was wrong: 1213/1214 are the 13-14 h frames (blue sun).

## 2. Bottom panel buttons: rest / hover / pressed (ctor 0x47c6d0, draw 0x47c760)
- Ctor `0x47c6d0(id1, id2, x, y, callback, cursor, flags, ...)` (called from 0x459595..0x45980a, y = 0x1cc = 460 for all eight): stores id1 at +0x24, id2 at +0x4c, +0x50 = -1, kind +0x48 = 0x1f. [C]
- Draw 0x47c760 (vtable idx 9 of 0x4f1df8): id = +0x2c (pressed) ? (+0x50 == -1 ? id1 + 1 : +0x50) : +0x2d (hover) ? id2 : id1. Image from the UI table `[0x8f0b1c]` (g.img()), drawn by 0x4b7fb0
  at (hx + x, vy + y) with flags 0x8003: first an OPAQUE copy of the overlay `[0x8f0b14]` rect (0x499330, restores the background), then the colour-keyed draw of the image (0x499480). No tint, no blend, no per-state offset. [C]
- State: mouse handler 0x47b2a0: move inside -> vtable +0x0c (0x47b470: +0x3c = +0x2d = 1 = hover), left down -> +0x30 (0x47b4e0: +0x2c = +0x2d = 1 = pressed, capture owner [0x6fa7cc]), up -> +0x34 (0x47b510 clears +0x2c, then the click callback). Leaving the rect clears them. [C]
  So rest = id1 (odd), hover = pressed = id2 = id1 + 1 for all eight buttons (1017/1018, 1585/1586, 1007/1008, 1005/1006, 1011/1012, 1015/1016, 1009/1010, 1013/1014).
- Why the fork had a "sepia" fit: the image loader 0x4b7da0 (US: "%de.img", then "%d.img", hashed SpriteNN bucket first = sum of the name bytes & 15, then Sprite.pak) would pick the SpriteNN patch copy of the
  odd images (1017: Sprite04, 328 bytes, a dark tile art; 1005: Sprite01) but the captured client shows the Sprite.pak art (brown monochrome icon on the parchment, 548 bytes for 1017): the installation here is newer
  than the captured client. Drawing the base-archive image `Recursos::sprite_base` at rest reproduces the capture without any tint: per-button best offset vs hud_original_1.png is (0,0), mean abs error 4-5 on the icons
  (the video blur accounts for the rest). The old `img_sepia` luminance fit and its 17 row clip are removed. [V]
- [V] Hover in original.mp4 (t = 1202.3, cursor on the magic button): the star turns reddish = image 1006 in colour; the video is too compressed to compare saturation. Pressed was not seen (no click in the footage).

## 3. Strip button hints (hint manager [0x8f0b6c], class draw 0x4bb8f0, items 0x4bc1ef..)
- NOT 0x4bbd20 / 0x4bbd30: those only set / clear the manager's enabled flag `[mgr + 8]` from the option byte `[0x6e50b8]` (option "tooltip", default 1 in the exe's data; applied at 0x459abf). [C]
- Items are built in 0x4bbdb0.. by ctor 0x4bb850(rect{l,t,r,b}, text, id, anchor x, anchor y, callback 0x4bb830); ids 0..7 are the HP/MP/AC/... boxes (not ported), ids 8..15 the strip. The text is
  string-e entry n (`[0x8f0b0c] + 4n`), wrapped by 0x41a040 at 22 cells (6 px): help 613 "Help (Ctrl-H)", mail 1250 "Letter", status 614, magic 615 "Magic (Ctrl-S)", inventory 616, social 617, options 618,
  quit 619 "System (Ctrl-Q)". Rects (r, b exclusive): help 491-506, mail 506-525, status 526-543, magic 544-562, inventory 563-579, social 580-600, options 601-617, quit 618-639, all y 460-478;
  anchors x 495, 495, 516, 537, 558, 579, 600, 621, y 478. [C]
- Delay: the item's mouse handler 0x4bbb50 schedules the show with `0x4ba8e0(0x2bc = 700 ms, 0x4bb830, item)`; 0x4bb830 sets +0x48 = show. Leaving the rect, or a left button down / double click, cancels (0x4bbb30) and the next
  mouse move re-arms the 700 ms. [C]
- Draw 0x4bb8f0: w = 142 (22 * 6 + 10), or for ONE line `6 * len + 21` if narrower (0x433f70 = 6 * len for codepage 1252); w &= ~3; h = 12 * lines + 10. x = anchor x (+ view origin), pushed left so x + w <= 640;
  y = anchor y = 478, but when y + h > 480 it goes ABOVE the rect: y = rect.top - h = 460 - h (always, for the strip). Then: 0x49a440 halves every channel of the w x h rect (50% darkening, not a black box);
  8 bevel lines through 0x498720 (fill rect): light RGB555 0x5ef7 (23/31 grey = 189) top rows y, y+1 and left columns x, x+1; dark 0x3def (15/31 = 123) bottom rows y+h-1, y+h-2 and right columns x+w-1, x+w-2;
  text via the chat-line blitter 0x432dc0 (flag 1) in white 0x7fff at (x + 5, y + 5), 12 px per line. [C]
- Not verified visually: the footage has the option off (no hint ever appears while the pointer rests on the strip for > 1 s, e.g. t = 1196-1206 s). The fork implements the code as read; face = the chat face with shadow. [I for the face]

## 4. Chat bubble draw (draw loop 0x4199eb..0x419e48, rect 0x419e70, ctor 0x41a600)
- Text: per line `0x4334f0(surface, line text, len, x, y, fill = bubble colour [+0x39c], outline = [0x5004f0] = 0 (black), flag)` (flag 1 for lines 2.. = "keep the colour state of the previous line").
  All lines use the SAME x: unboxed x = rect.left, boxed x = rect.left + 5 (flush left, NOT centred; the rect is as wide as the widest line); y = rect.top (+ 5 boxed), +12 per line. 1 px black outline. [C]
- Pointer (unboxed bubbles only; the boxed variant draws its own tail masks 0x500328 / 0x50032c): `0x498620` bit-mask blit (1 bpp, MSB first). Normal: black 9 x 5 triangle (mask 0x500348) at (left + off - 3, bottom - 5), then the
  bubble-coloured 7 x 4 triangle (mask 0x500340) at the same x, y = bottom - 4. Off = +0x394 = distance from rect.left to the anchor (3 * w), kept on the anchor by the clamps. [C]
- Clamps (0x419e70), complete: left < 0x30 -> shift right by 2 - left and off -= shift, dropped when off < 8; right > 0x2b0 -> off += overshoot, dropped when off > width - 9, then shift left. [C]
- Pushed-down flip (top < 0x30, 0x419f5e..): `top = Y + frame[+0x1c]` = the current sprite frame's BOTTOM (frame +0x14 is its top, used for the anchor), bottom moves by the same delta, flag +0x36a = 4. No "+ 4". [C code, frame fields as
  top / bottom of the frame rect: I]. Then the pointer flips: up-triangle masks 0x500352 (outline, at y = top) and 0x500344 (fill, at y = top + 1), and the text starts 6 px lower (boxed: 4 + 5). The bubble is dropped when top < 0x30 or bottom > 0x1c8 afterwards. [C]
- Grey marking: done in the constructor right after the first rect computation (0x41a867..0x41a8f4), and ONLY when that computation did not drop the new bubble. Fork: still run on the bubble's first drawn frame (same
  result unless the speaker is off screen at creation: the fork has no screen position then). [C; difference kept]
- Verified: fork capture only (/tmp/r8_bubble.png): no speech bubble could be found in original.mp4 or the JP footage, so none of the above could be compared with an original frame. Not tested live: the flipped case (needs a speaker near the top edge).

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

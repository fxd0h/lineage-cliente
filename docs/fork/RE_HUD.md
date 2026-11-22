# HUD, chat, names and input in S3EP1U.bin (reverse-engineered)

Tags: [C] confirmed from decompile, [I] inferred. In REA pass addresses as `ram:0x...`.

## Chat colours

- Server chat handler 0x484800 (from 0x4932c0): type `t` via jump table 0x484a14 (index t-1), RGB555 table at 0x5004f0. [C]
  - default (255,246,180); t1 (255,205,205); t3 global (172,238,238); t4 (197,238,180), or (255,131,255) if text starts
    with `{`; t11 (205,213,255); t12 trade (189,164,106) (dropped if 0x421440 / filter 0x40e530 fail).
  - `[0x703107]` set: type 3/12 lines with bytes > 0x7f are dropped (ASCII-only filter).
- S_Say (0x483500): normal white (255,255,255); a line starting with `(` (189,238,230); shout (type 2) (255,230,205) with a
  `\d<0-7>` direction digit (atan2 to the player) before the text; type 0xf (255,131,255). Whisper (0x483970) (255,205,205),
  format `"(%s) %s"`. [C]
- Line adder 0x41f0a0 splits on `\n`, strips `\r`. [C]

## World labels

- `[0x702b7c]` is a locale enum from the ANSI codepage (0x45ca90): 950 -> 3, 936 -> 5, 1252 -> 1, 932 -> 4, else 0.
  The US build is 1. [C]
- Names (0x465c30, 0x4676bb..0x467a2c): only the hovered object `[0x816684]`. Title (obj+0x60) only if `[0x816560]`, at
  boxTop-3, name 13 px lower (or at boxTop-3 without title). Centred `boxL + (boxR-boxL - 6*len)/2`; own character
  `screenX - 3*len - 0x18`; +10 if obj+0x4c. Glyph 6x12 with a 1 px black outline (0x4334f0). A name starting with `+`/`-`
  skips its first token. Non-NPC with obj+0x23 == 0 and gfx 0xa9b: red fill. [C]
- Level tiers, monsters (kind 0xa) with locale 1 or 5: `d = monsterLevel - myLevel`; d >= 10 (255,0,0); d >= 3 (131,131,0);
  d >= -1 (0,0,255); d >= -4-(myLevel>>1) (0,189,0); else plain. The tier colour is the 1 px OUTLINE (after an 8x14 black
  halo pre-pass 0x433a90 at (x-2,y-2)); the fill stays the normal name colour. [C from pseudocode, not seen rendered]
- Own HP/MP bars `[0x8169ac]`: off by default, toggled by the `showhpmp` option (0x40b5b8). [C]
- No damage numbers. Banner texts `[0x81647c]` / `[0x8163f8]` drawn in blue 0x5004f4 near y+0xb8 / y+0xaa (meaning U). [C]

## Speech bubbles (S_Say -> 0x41a600)

- Lifetime mode x 1000 ms: 8 s for others, 5 s for the own character (0x41a530). Plain: 20 chars per line, 12 px pitch,
  centred, top offset -lines*12-6. Boxed (option `[0x6f5d2c]` or mode 5): 16 chars, 5 px padding, -lines*12-14. Anchor
  (objScreenX+0x18, sprite top-2+offset). An older overlapped bubble turns grey (148,148,148). Skipped when `[0x703108]` and
  the text has bytes > 0x7f. [C]

## Buff icons

- Layout (0x47df20): n-th icon (n from 1) at x = 638-35n, y = 5; if x < 0, x += 630, y = 40 (row 2 from n = 19). [C]
- Table 0x6e1020 (stride 0x14, 154 slots): +0 icon id, +4 seconds, +8 small 0..7, +0xc tooltip message index, +0x10 second id.
  Tooltip = that string only, word-wrapped in a 150-wide box (0x47e1a0), colour (189,222,255). No blink in the draw. [C]

## Panel buttons (flag 0x8003, 0x4b7fb0)

- Bit 0x8000 = colour-keyed blit; bit 1 with overlay `[0x8f0b14]` copies the overlay first; bit 2 clips. No blend or tint:
  rest / hover / pressed are the plain 1st / 2nd / 3rd images. [C]

## Mouse and keys

- Click decision 0x468c20: aborts if `[0x702dcc]`, 0x456860(0x18) or dragging `[0x816558]`; mode `[0x702dd0]` 0 walk,
  1 Ctrl, 2 Ctrl+Alt, 3 walk toward an out-of-reach target (> 2 tiles x or > 1 y). [C]
- Emote 0x40e0d0: if `!0x4cb8a0() && [0x8f0e21] == 0`, send `cc` opcode 0x26, emote index; then the animation 0x4c96c0. [C]
  Alt+digit mapping U.

## Cursors

- Handles `[0x81bef8 + 4*state]` from resource ids at 0x6e0de8 (0x477c50); setter 0x477c90. [C]

| state | id | state | id | state | id | state | id |
|---|---|---|---|---|---|---|---|
| 1 | 130 | 5 | 132 | 9 | 138 | 13 | 142 |
| 2 | 131 | 6 | 133 | 10 | 139 | 14 | 143 |
| 3 | 105 | 7 | 144 | 11 | 140 | 15 | 145 |
| 4 | 127 | 8 | 136 | 12 | 141 | 16 | 150 |

- Chooser 0x469e40 [C]: self +0x12c non-empty 16; dragging an item over a valid target 13; over a window 5; pending use or
  target out of range 11. No object: Ctrl+Alt 7, attack mode 15, Ctrl 6; map 0x309..0x30b with self gfx 0x1792/0x1793: 4.
  Object class (0x4cdd30, jump table 0x46a358): 2/3 -> 8 if attack mode and weapon byte 0x28/0x3c, else 10; 4/14/15 -> 9;
  7/13 -> 14; 8 -> 14 if tile flag bit 2 else 3; 9 -> 3; 5/11 -> Alt 7, attack 15/8, else 6/3; 6/12 -> attack+Ctrl 15, else
  4 if +0x129 == 0, else 3; 10 (monster/NPC) -> Alt 7, attack 15/8, else 6.
- Modifier flags (2026-11-22): [0x702dd6] = Ctrl and [0x702dd5] = Alt (key handler 0x456bb0, VK 0x11 / 0x12); [0x703105] = Ctrl+X
  attack mode; [0x6f1dac] = the "pk" option toggle (0x403dbd). Ctrl with or without an object is checked before the object class.
  Class from 0x4cdd30: sprite type 10 -> 10 (dead 2; pet whose master is the player 6, other pet 5); types 5/6/12 dead -> 3; 0xb doors
  by action; 4/7/8/9/13/14/15/16/0 as their type; else 1. Fork: `elegir_cursor_original` (NPCs and own pet 4, not the sword).

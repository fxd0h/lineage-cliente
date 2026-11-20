# Sprites and rendering in S3EP1U.bin (reverse-engineered)

Findings from the 2009 US client, used to implement sprite attributes and blend modes in the fork.
Tags: (C) confirmed from decompile/disassembly, (I) inferred, (U) unknown.

## Frame renderer and blend flags

- `FUN_00465c30` (0x465c30) is the frame renderer. Per map cell it draws the ground tile, then the per-cell
  list at cell+0x48c (cells at `DAT_0081675c + cell*0x494`). Entries are 16 bytes: +0 layer (sort key, ascending,
  inserted by `FUN_00460230`), +4 sprite, +8 flags, +12 byte param. (C)
- Some flags are gated by user options: bits 0x1000, 0x2000, 0x4000, 0x8000, 0x10000, 0x20000 tested against
  `FUN_0044bf40(0x3b6/0x3e9/0x527/0x8b8/0xcdc|0xcde)`. (C)

Dispatch order of the flags word (0x466200-0x4663b0) (C):

```
if flags & 0x40:   50% average, only when local_90 is set (else not drawn)  (gate: [0x8ed57c] in 1..3,
                   [0x6f5d1c]==0, FUN_0044c150(&DAT_006de438); meaning U)
elif flags & 0xc00: alpha fade, level = byte param 0..31 (DAT_008ebf64)
elif flags & 0x02:  not drawn
elif flags & 0x08:  screen       (DAT_008ebf58)
elif flags & 0x101: 50% average  (DAT_0082470c)
elif flags & 0x80:  multiply     (DAT_00824710)
elif flags & 0x10:  additive, clamped (DAT_00824708)
elif flags & 0x200: tint with a global colour DAT_005005c4 (math U)
elif flags & 0x20:  pulsing brightness, param |0xc - tick%0x18| + 0x10 (math U)
else:               opaque (FUN_0049cfc0)
```

Pixel math (C): `FUN_004a03b0` builds per-channel 32x32 tables (5-bit channels, 565 or 555); the generic
blitter `FUN_0049d420` does `out = T[(dst<<5)+src]` per channel. With a = destination, b = source (0..31):

| Flag | Math |
|---|---|
| 0x8 | screen: `max + min*(31-max)/31` (truncated) |
| 0x10 | additive: `min(a+b, 31)` |
| 0x1, 0x100 | average: `(a+b)>>1` |
| 0x80 | multiply: `a*b/31` (white leaves the ground unchanged: the disc under street torches, gfx 1523) |

Map loader tile objects (`FUN_00461ff0`, `FUN_00460580`) (C): object attribute bit 0x10 -> flag 0x8 (screen),
0x04 -> 0x100 (average), 0x20 -> 0x80 (multiply), in that priority.

Body flags (C), `FUN_004cc980`: `obj+0x1c = attr104[gfx]`, then ORs from the object packet status byte:
`&1` -> 0x20 (pulse), `&8` -> 0x200 (tint), `&0x80` -> 0x2 (hidden), else `&2` -> 0x100 for the local player,
0x40 for others. The shadow (attr 101) part is drawn with `(flags & 0xffff0fff) | 5`. Clothes (attr 105) use
their own attr 104 `|0x40` or `|1` from the object flags, and are skipped when the body has 0x2.

## Sprite list (list.spz) loader `FUN_00440100`

- Number reader `FUN_0043ffe0` skips every non-digit; `-` right before digits makes it negative. (C)
- Header `#id N[=M] name`: N image count (`DAT_00701328`), `=M` file group (`DAT_007012fc`, default 0xffff). (C/I)
- Action `N=G` copies action N's frames from gfx G (images also from G). `N.name(dirFlag count, frames...)`
  defines a body; frame `group.idx:dur`, duration `dur*1000/framerate` (attr 110, default 24), scaled by
  `DAT_00701350` percent for actions 0, 4, 11, 20, 24, 40, 46, 50, 54, 58, 62. Image index = group + heading
  when dirFlag != 0. (C)
- Fallback: if actions 0x2e..0x31 are missing and 4..7 exist, copy 4..7 into them; same for 0x32..0x35. (C)

| Id | Meaning | Parse | Behaviour |
|---|---|---|---|
| 100 | switches / extra blocked tiles | n, n x (x1,y1,x2,y2) | `FUN_004cba00` after object creation: for x in x1..x2 step 2 (half tiles), y in y1..y2, relative to the object's tile, mark the tile blocked (`FUN_00460360(2)`); if the player is inside, reposition (`FUN_00468240`). (C) |
| 101 | shadow gfx | 1 | same image index as the body, -1 none. (C) |
| 102 | type | 1 | 0 effect, 1 scenery, 4 sign, 5 player, 6 NPC, 7 chest, 8 door, 9 ground item, 10 monster, 11 gate, 12 shop NPC, 14 board, 15 auction board. (C, values from data) |
| 103 | interaction radius | 1 | `FUN_004c7460` mode 2: reached when `|dx| <= 2v, |dy| <= v`. (I) |
| 104 | draw flags | 1 | see above. (C) |
| 105 | clothes | n, n gfx | overlays at the body's image index. (C) |
| 106 | weapon overlays | 5 shorts (global list) | slot = table `DAT_006e51bc[baseAction]` (4->1, 11->2, 20->3, 24->4, 40->5, 46->1); overlay gfx short[slot-1] at the same image index with its own attr 104. (C) |
| 107 | size | 2 numbers, stored `a | b<<4` | click/arrival box half extents (x doubled). (C) |
| 108 | stance | 1 | 2 = weapon-stance humanoid: action 0x2c end -> stance 2, base action 4 (11 if stride); 0x2d end -> stance 1, base 0. (C) |
| 109 | effect class, next gfx | 2 | object subclass at spawn (`FUN_00487780`): 1 default, 2, 3, 4/5/7 get the next gfx, 8 none. (C; class meanings I) |
| 110 | framerate | 1 | ticks per second for later actions on the line. (C) |
| 111 | stride | 1 | walk action from `{0,4 | 11,20 | ...}` by stance and a per-object step counter (`FUN_004c6ff0`). (C) |
| 112 | alternative gfx | 1 | when the object has no frames for its action and heading is 6, use this gfx's images. (C) |
| 113 | morph effect | 1 | after an object changes to this gfx (`FUN_00488f60`), spawn that effect gfx at its position. (C) |
| 114 | footprint | 1 | 2, 3, 8 allocate 1, 3, 5 occupancy tiles. (I) |

Frame suffixes (C): `<N` sound (overlap allowed), `[N` sound (only if not already playing), `^N` sound scaled by
distance; up to 3 ids per run, one picked at random when the frame shows. `]N` spawns effect gfx N at the frame.
`!` impact frame (applies the pending hit). `>` flag bit 2 (reader U). `{` `}` do not exist in 2009 (newer lists).

## Equipment window images (audit U-C2)

`[0x81b328]` is the sex byte of the selected character (char-list entry +0x1f; +0x1e is the class). (C)
There is no `1516.img` / `1518.img` in this installation (only unrelated monster thumbnails `1516.png`,
`1518.png`); the 1440 / 983 fallbacks are correct. (C)

## Bookmark picker (audit U-F1..U-F4)

- Picker condition (0x409690 item use, 0x4ac830 spell cast): ring ability in 1..3, or class 2 (elf) with
  level >= 50 and the `/telcon` flag `[0x6f1e4a]`. `/telcon` flips it for elves level 50+, else forces it off;
  cleared at login unless class 2. (C; game meaning of telcon I)
- Without the picker: item sends `cdhhh 0x77, objid, curMap, 0, 0`; spell sends `ccchhh 0x81, id>>3, id&7,
  curMap, 0, 0`. Refused on maps 2, 5, 6, 0x53, 0x54, 0x1be, 0x1bf, >0x3fff. (C)
- Confirm (0x418b50): first selected row; item `cdhhh 0x77, objid, map, x, y`, spell `ccchhh 0x81 ...`. A row
  with x = y = 0 sends the current map. With no row selected nothing happens (empty window is inert). (C)
- Delete: Del/Backspace on the selected row opens the yes/no text prompt string 0x34e; on `y` sends `cs 0x80,
  name` (by name) and removes the row. (C)
- `/bookmark` takes no argument: opens a name prompt (string 0x246), then `cs 0x00, name`; name over 27 chars ->
  string 0xcc; refused maps (above plus 8..0xf, 0x12, 0x14, 0x15 and four rectangles on map 4) -> string 0xd6. (C)
- Scroll bar internals and default position: not decoded (U).

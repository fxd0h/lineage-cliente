# Protocol in S3EP1U.bin (reverse-engineered)

Server packet dispatch `FUN_00493240`; S_PacketBox (opcode 0x3e) `FUN_0048e780`, subtypes 0x00-0x4b.
Before entering the world (`DAT_007030b4 != 3`) only opcodes 4, 5, 0x17, 0x1d, 0x29, 0x38, 0x3e, 0x3f, 0x42,
0x4a, 0x4f, 0x50, 0x54, 0x58, 0x59, 0x66, 0x74, 0x7d are dispatched. Payload offsets are after opcode/subtype.
Tags: (C) confirmed, (I) inferred.

## S_PacketBox subtypes l1j-en sends

| Sub | Payload | Exe behaviour |
|---|---|---|
| 0-2 | c castle, h 0 | siege message, siege flag, music (C) |
| 3, 4 | - | siege music start/stop (C) |
| 5 duel | d id, d time | id 0 clears the duel; else message 0x285 with the object's name; time unused (C) |
| 10, 11 | c | weight / food (C) |
| 12 | c min, c max | below min: message 0x2a0; above max: 0x2a1 (item level limit) (C) |
| 15 | h n | elf spirit message, string 0x2a7+n, sound (C) |
| 17-19 | block list | (C) |
| 21 | c b0..b3 | four class icons, duration b*16-8 s: dark elf 0x1f,0x1d,0x1e,0x20; illusionist b3 -> 0x74; others 0xe,0x11,0xf,0x10 (C) |
| 22 | c aura, h s, [c extra] | aura switch 0x4903b0 (see hechizos_ui.cpp) (C) |
| 23 | s name | "%s is town leader", string 0x2fd, sound (C) |
| 34 | h t | icon slot 0x26 (0x5b when DAT_0070138c==0x9a1) (C) |
| 35 | h t | polymorph icon: 0x4b if sprite in 0x16dd-0x16e6 else 0x27; t 0 removes 0x27/0x6d/0x70/0x73 (C) |
| 36 | h t | chat ban icon 0x29 (C) |
| 37 | c, d petId, c, c | pet equipment; reads "cd", replies a packet (C/I) |
| 40 | h t | icon 0x3f (C) |
| 41 | d len, bytes | stored config blob (len <= 0x400), applied on the next screen (C/I) |
| 42 | - | logout to character select using the cached list (state 2, FUN_004749f0) (C) |
| 43 | - | message + sound (can't restart now) (C/I) |
| 45 | c n, list | sorted list to chat and bot_list.txt (C) |
| 49 | c id | colosseum / chaos-war message, sound (C) |
| 52 | 5 bytes, c idx | byte 5 = 0: message 0x77; = 1: cooking window, index+1 (C) |
| 53 | c x6, h, c type, c, h t | cooking buff icons 0x53+(type-8)... duration h (C) |
| 55 | - | fishing UI (C) |
| 56 | h t, [h] | t>0 icon 0x52 (0x5c if second h in 0xc20-0xc29); t<=0 removes (C) |
| 57 | c 0x2c, h | icon 3, duration x4 (C) |
| 59 | h 0 | removes icon 0x87 (C) |
| 74 | h t | icon 0x4a, sound (C) |
| 75 | c n | n>0 icon 0x87 with string 0xc9e-n; 0 removes (C) |
| 0x40-0x48 | race / deathmatch | 0x40 c countdown; 0x41 -; 0x42 hh total,index + n x s; 0x43 hh max lap, lap; 0x44 s name, d ms; 0x45 game over; 0x46 end; 0x47 h timer on; 0x48 off (C) |

Icon numbers are slots of the buff table at 0x6e1020 (20-byte entries, field 0 = classic icon id).

## Server opcodes

| Op | l1j | Exe format | Note |
|---|---|---|---|
| 0x1a | S_WarTime | "hs": h count (max 6), s timezone, count x (c, d unix time); timetable window | must not be decoded as MAPID |
| 0x1b | S_CharReset | by first byte: 1 "hhcc", 2 "cchhhcccccc", 3 c points, 4 d nibbles | |
| 0x26 | S_ChangeName | "ds": id, name (may be "$nnnn") | renames the object |
| 0x44 | S_HireSoldier | FUN_0048c4c0 (not decoded) | rare |
| 0x49 | S_MapID | "hc": map, underwater byte | |

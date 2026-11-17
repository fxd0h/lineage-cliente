# Audit: original UI (`ui_simple=0`) vs the 2009 US client (S3EP1U.bin)

Read-only audit, 2026-11-16, branch `linux-fx` at `bd5f330`. It lists every visible or behavioural difference found between
the fork's original-UI mode and the 2009 US client, so the original UI can be made identical.

**Evidence conventions**
- Addresses are VAs in `re/S3EP1U.bin` (VA = file offset + 0x400000), read from an objdump disassembly.
- `img N` is a Sprite-pak image id. Coordinates are in the 640x480 block, or relative to the window's frame image
  (0,0) inside window tables.
- Reference frames are in `video/catalog/*.png`. They come from the JP client, which has the same layout.
- Colours are RGB555 words from the `\fX` palette at 0x5004f0 + 4*(c-0x30), the same table as 0x500430.

**Common draw primitives in the original**
- `0x4991e0` / `0x498f20`: image blit.
- `0x431390`: plain text.
- `0x4334f0`: outlined text.
- `0x432aa0`: chat text run.
- `0x47c2d0`: plain image button.
- `0x47c6d0`: text/image button with a hover image.
- `0x47cd80`: scroll bar.
- `0x47bcd0`: drag handle.
- `0x47ede0`: invisible tab hit-rect with a tooltip string.
- `0x4a9a20`: play a sound.

**Status values**
- MATCH: proven equal.
- DIFFERENT: exists in both but differs.
- MISSING: in the original, not in the fork.
- EXTRA: fork-only, but visible in the original UI.

Rows that could not be proven are not in the tables; they are listed under UNKNOWN at the end.

Areas are ordered by how visible they are to a player.

---

## 1. Bottom HUD panel (always on screen)

Original HUD object: ctor 0x4b08e0, draw 0x4b2c20 -> 0x4b2b60, mouse 0x4b1c50.

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Side panels 1028 / 1019 | 1028 at (0,368) (0x4b2bbd); 1019 at (485,366) (0x4b22bd) | same | MATCH | - |
| Chat backdrop 1042/1044/1043, input bar 1039 | 0x416160: 1042/1044 x=115, 1043 x=121 | 1042 (115,358), 1044 (115,359), 1043 (121,371), 1039 (120,462) | MATCH | - |
| Weight gauge | img 1035/1036/1037 (green <50 %, <83 %, else red) at (22,464), width cur*43/max, h 10 (0x4b2660, 0x4b26de); text `"%d%%"` at (43-2*len,466) | text only at (24,464) | MATCH (fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| Food gauge | img 1038, or 1495 when full, at (88,464), width cur*43/max (0x4b2800, 0x4b28bc); text at (109-2*len,466) | text only at (88,464) | MATCH (fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| Alignment icon | img 1032 (<0) / 1033 (0..500) / 1034 (>500) at (70,446) from `[0x8f07e0]` (0x4b2970..0x4b2b4b); ref frame `hud_original_1.png` shows the icon | none | MATCH (fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| Alignment number position | `"%d"` at (109-2*len, 449) (0x4b2970) | (88,447) left-aligned | MATCH (fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| HP / MP numbers | `"%d/%d"` at x=83-2*len, y=410 (HP) / 429 (MP); white 0x500528 with a 1 px 4-way outline in 0x5005bc (180,82,74) (0x4b13b8..0x4b1412, 0x4b15bc) | left-aligned at x=35, black drop shadow | MATCH (fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| Level / EXP text | level `"%2d"` at (37,391); percent at (80,391) if level <40, else (72,391); format `%d.%02d%%` <40, `%d.%04d%%` >=40 (0x4b0c80, fmt 0x6e4d50/0x6e4d5c); plain white, no shadow; ref `hud_original_1.png` "52  24.8613%" | one string `"%d  %.2f%%"` at (35,~391), shadowed, always 2 decimals | MATCH (fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| EXP/HP/MP bar fill | whole image clipped to pct*129/100, h 15/21/19; not drawn when width 0 or HP<=0 (0x4b0e0c `cmp eax,0x81`, 0x4b1231, 0x4b14ed). The "Lev/Hp/Mp" label is engraved in panel 1028 | always keeps the first 28 px of the bar image (`ROTULO_BARRA`), clips the rest | MATCH (fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| HP bar image by status | `[0x8f0e4c]+0x21` bit 0x8 -> img 1358, bit 0x1 -> 1357, else 1030, at (3,402) (0x4b1247) | always 1030 | MATCH (verified at 60/242 HP in a test instance: the missing part is not drawn by the HP routine 0x4b1180 at all, it only blits the source rect (0,0,hp*129/max,21) of the bar image at (3,402) and returns at HP <= 0; the empty part shows the panel image 1028, drawn first at (0,368) by 0x4b2b60 (item [+0x1010]) before 0x4b0c80/0x4b1180, whose engraved silver rail is therefore the original's look; fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| AC number | `"%d"` at (43-2*len, 449) (0x4b0c10) | (24,447) left-aligned | MATCH (fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| F-key pages | 3 pages of 8 (0x4b2240 `idiv 3`); digit img 1025+page at (489,414); arrows 1021 (next page, callback arg 0) / 1023 (previous, arg 1) at (489,387)/(489,429), callback 0x456960 (page +-1, sound 0x7f) | 3 pages (`atajos_pg`, persisted as 24 entries per character), digit 1025+page, both arrows work; the 0x7f click sound is not played | MATCH except the sound; F1-F3 page keys still differ (below) | hechizos_ui.cpp `cambiar_pagina_f`, pantalla_juego.cpp `dibujar_panel` |
| F-key selected-slot frame | img 1020 at (502+34c, 388+34r) on the last-clicked slot `[0x703100]` (0x4b24d0..0x4b2549); ref frame shows a white frame on F7 | `a.tecla_f_sel`, frame 1020 at (x+17+34c, y+22+34r) in the original UI; a press on a box selects it | MATCH | hechizos_ui.cpp `dibujar_teclas_f` |
| F-key hover name | hovered filled slot: name at (500+34c, 400+34r), white, outline 0x5004f0, clamped to x 640 (0x4b2549..0x4b264a) | drawn in the original UI (`texto_contorno`, black outline) | MATCH (spell name or item name) | `dibujar_teclas_f` |
| F-key box origin | (502+34c, 389+34r), 4x2 (0x4b23e5, pitch 0x22) | (502,389) pitch 34 | MATCH | - |
| F-key overlays (cooldown shade, seconds countdown, MP-short dim, count) | no such draw found in 0x4b2260 (contents drawn by 0x4aad90 / 0x445fd0) | drawn | EXTRA | `dibujar_teclas_f`: gate on `ui_simple` (see UNKNOWN U-A10 for item count) |
| Panel buttons (8 at y=460) | help 491 (1017/1018), mail 507 (1585/1586), status 527, magic 544, inventory 563, social 580, options 601, quit 618 (0x459595..0x45980a, ctor 0x47c6d0) | same x, y=460 | MATCH | - |
| Panel button rest/hover art | draw 0x47c760: rest = FIRST id (odd, e.g. 1005); hover = 2nd ctor id; pressed = id+1; all via 0x4b7fb0 with flag 0x8003 (blend not decoded) | rest = id+1 converted to sepia mono and drawn 1 px lower; hover = id+1 in colour ("fitted" to a screenshot, ventanas.cpp `boton_panel`) | DIFFERENT | ventanas.cpp `boton_panel`: draw id n at rest; decode flag 0x8003 (U-A1) |
| Panel button tooltips ("Help (Ctrl+H)" ...) | no tooltip in button class 0x47c760 | black box + white tooltip | EXTRA | ventanas.cpp: remove in original mode |
| "A" chat-mode button 1041 / scroll-to-bottom 1045 | 1041 at (136,464) (0x459b30); 1045 at (470,463), hover 1281, callback 0x421430 (0x4594d8) | drawn at those coordinates, inert, no hover | DIFFERENT | pantalla_juego.cpp: wire up 0x421430 / 0x474a47 behaviour (U-A11) |
| Chat scroll bar | track 1047 (470,401), arrows 974/972, thumb 976; ctor 0x47cd80 cb 0x41f650 | implemented at those positions | MATCH | - |
| Colour palette (Ctrl+O) | 0x456dc3, 16 swatches 2x8 | implemented | MATCH | - |
| Buff icons, top right | 32x32 at y=5, x = 638-35*n with the FIRST buff rightmost and newer ones to the left; wraps to y=40 when x<0 (0x47df7c..0x47df97) | y=4, x from W-36, first at left, newest at right | DIFFERENT | hechizos_ui.cpp `dibujar_efectos_activos` |
| Buff icon blend | screen blend (0x47e6f0 -> 0x49d420, table 0x4a03b0) | PANTALLA blend | MATCH | - |
| Buff icon hover | message-table text for the slot (`[0x6e1020+slot*20]`), box by 0x498b10, no timer (0x47e6f0..0x47e817) | name + "m:ss" | DIFFERENT | `dibujar_efectos_activos`: table text only |
| Buff icon ids marked "Inferred" | slot table 0x6e1020 | holy weapon 5248, decrease weight 5291, EDEX 5247, EStr 5246, Holy Walk 5243 guessed (hechizos_ui.cpp:668) | DIFFERENT | finish the 0x6e1020 table |
| Pet panel ("Bag") and party list drawn at left | not in the window set built by 0x458640..0x459dce; the original uses windows 1170/1197 | party list (`dibujar_grupo`) now only in the simple UI (MATCH); `dibujar_mascotas` still always drawn | party: MATCH; pet: MATCH (the panel is simple-UI only; the original UI has the 1170 window, mascotas_ui.cpp) | done |
| Auto-buff / auto-potion / shortcut bar / minimap | absent | hidden in original mode | MATCH | - |

## 2. Over-the-world rendering (names, bars, captions, cursors)

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| When names are shown | only the single hovered object `[0x816684]` (0x4cc1d0/0x4cc220, bbox test 0x4676bb..0x4676f5) | hovered object + attack target | DIFFERENT | pantalla_juego.cpp ~550: drop the `a.objetivo` exception |
| Attack target name in yellow | no target colour in the label code | yellow (255,230,90) | EXTRA | remove in original mode |
| Monster/NPC name colour | when `[0x702b7c]` is 1 or 5 and type 0xa: d = monster lvl - my lvl: >=10 red `\f3`, >=3 (131,131,0), >=-1 blue `\f1`, >=-(lvl/2) green (0,189,0), else default (0x467856..0x4679e2) | 4 tiers white/green/yellow/red; non-player default (200,220,255) | DIFFERENT | pantalla_juego.cpp `COLORES`: 5 tiers + gate (U-A3) |
| Player name colour | 0x4cc140: red fade <0, white 0..499, blue fade >=500, PK pink 0x767d; brightened if option `[0x6fa678]` | same formulas | MATCH | - |
| Name text style / title line | 0x4334f0 outlined (black 0x5004f0); 2nd line (+0x60 string) 13 px above | black shadow; title line above | DIFFERENT | use a 1 px outline instead of a drop shadow |
| Overhead HP bar (others) | frame 930 + fill 931 clipped by HP% when `[obj+0x55]!=0xff` (0x466c2e..0x466dbc) | same | MATCH | - |
| Overhead HP/MP bars (self) | gated by `[0x8169ac]`: 930+931 then 930+932 (blend flag 0x10) | only with `cfg.barra_personaje` (default off) | DIFFERENT | default of `barra_personaje` (U-A2 for the flag's default) |
| Cursor caption (spell target etc.) | message-table text 0x231+`[0x6ddf9c]`, green 0x500548 with shadow 0x50051c, at (cx+30, cy+48), box image at (x-30,y+25) (0x466590..0x466694) | "<spell>: choose a target" yellow at (mx+14,my+4); "Choose the pet's target" | DIFFERENT | pantalla_juego.cpp target prompt |
| Damage numbers | none; damage is a chat line `"%d Damage"` (ref frame shows "12 Damage") | "-N"/"Miss" rising text (opt-in, default off) | EXTRA | keep off in original mode |
| "PK" badge top-right | not created in the screen init | drawn at (w-40,40) | EXTRA | gate on `ui_simple` |
| Paralysed/Asleep/Frozen/Stunned banner | not created in the screen init | top-centre banner | EXTRA | gate on `ui_simple` |
| "Leave spectator mode" button | not created in the screen init | drawn | EXTRA | gate on `ui_simple` |
| NPC "[Merchant]" title, True Target "V", vendor shop title | label code draws only the +0x60 string | drawn | EXTRA | U-A7 before removing |
| Cursor table | 16 cursor groups loaded from table 0x6e0de8 (0x477c50/0x477c90): arrow RT3 over HUD (0x4745f0), hand RT2 in world, RT5 over windows, sword RT6 with Ctrl, RT13 while dragging an item over a valid target (0x469e92), RT16 attack mode, RT17 busy (0x469e62); group 135 / RT7 (lips) is not in the table | hand, sword, target RT8, lips RT7 for NPC talk, RT13 as "shop" coin; roles "chosen by visual match" (cursores.cpp:5-9,120) | DIFFERENT | cursores.cpp: drop lips; RT13 only for item drag; add RT3 over the HUD, RT5 over windows, RT16, RT17 |
| Level-up effect 2127 | per README, recovered from the binary | drawn | MATCH | - |

## 3. Chat

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Default text colour | `\fH` 0x500550 (255,246,180) cream, used at 184 call sites | white | DIFFERENT | src/mundo/chat.cpp `color_chat` |
| Colour per message type | jump table 0x484a14: normal/shout/default cream; type 1 (255,205,205); type 3 global (172,238,238) or red when text[1]=='*'; type 4 clan (197,238,180) or (255,131,255) if text starts `{`; type 11 party (205,213,255); type 12 trade (189,164,106); whisper in `"(%s) %s"` (255,205,205), sent echo (255,230,205) (0x4839de, 0x48391d) | shout (255,170,80), global (255,255,130), clan (130,255,255), party (130,255,130), trade (255,200,130), alliance, whisper (255,150,220), system: no source cited | DIFFERENT | chat.cpp `color_chat` (type mapping: U-A4) |
| Rows / origin / pitch | 5 rows (`[0x500484]=5`) at (138,402), pitch 12 (0x41f4ee..0x41f52d) | 5 rows at (144,395), pitch 14 | DIFFERENT | pantalla_juego.cpp `CHAT_X/CHAT_Y`, row pitch 12 |
| Wrapping | 52 visible chars per line (0x34), colour codes not counted, continuation indented 4 spaces (0x5004a8..0x5004b8) (0x41ee10/0x41ebe0) | by pixel width 342, no indent | DIFFERENT | chat.cpp / `dibujar_chat` |
| Text shadow | drawn with 0x432aa0 (no shadow); ref frame shows plain text | black shadow | DIFFERENT | `dibujar_chat`: plain text |
| History size | 150-line ring (`idiv 0x96`), 102-byte lines | 256 x 190 | DIFFERENT | chat.h capacities |
| Input box | edit ctor 0x45f1f0 (x=152, y=466, w=318, h=12, cb 0x459e20); capacity w/6 = 53 (0x45f23d) | (154,465), w 330, max 60 (`MAX_ENTRADA_CHAT`) | DIFFERENT | pantalla_juego.cpp `ENTRADA_*` |
| Typing opens chat | any unhandled printable key / arrows / Home / End / Del -> 0x4574fd -> 0x408230 opens the edit with that char | needs Enter (`chat_con_enter`), "Enter: chat" placeholder | DIFFERENT | app.cpp / pantalla_juego.cpp: open the input on the first printable key |
| `"` key whisper prefill | `'"'` pre-fills `"<last whisper target> ` (fmt 0x4ffb64, names 0x6e85c4) | parser handles `"` but does not pre-fill | MISSING | pantalla_juego.cpp chat input |
| Closed-input placeholder "Enter: chat", "^" scroll marker | not drawn by 0x416160/0x41f4xx | drawn | EXTRA | gate on `ui_simple` |
| Ctrl+T size cycle | 0x421330 | 4 sizes, plus an invented rgba(0,0,0,110) backdrop | DIFFERENT | drop the extra backdrop (U-A12) |
| `\fX` codes in server messages | parsed by 0x432aa0 with the 0x500430 table | same table | MATCH | - |
| Chat double click | 0x41f680 | implemented | MATCH | - |
| "Select a target." (GM Ctrl+Y/Z) | string 179 into chat (0x409430/0x409380, 0x41f0a0) | chat line (255,230,120) | DIFFERENT | use the default cream unless 0x409430 passes a colour |
| Ctrl+P screenshot | `%sCapture\%s%04d.bmp`, prefix "LinC", counter 0x6f1e54; message "Saved current screen to %s" (0x4ff36c), non-GM only (0x402680) | "captura.bmp" overwritten; "Screenshot saved: captura.bmp" | DIFFERENT | pantalla_juego.cpp case 'P' |

## 4. Mouse in the world

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Right click | 0x469a62: re-runs the hit test 0x469d90 with `[0x816564]++`, so it cycles through overlapping objects under the cursor; Shift+right-click on a player puts its name into the chat line (edit vtable+0x68) | nothing (only cancels targeting) | MISSING | pantalla_juego.cpp: object stack under the cursor + Shift+RClick name insert |
| Middle click | 0x45ba6a: uses the selected F-key slot `[0x703100]` (0x4b1ae0(slot,1)), repeats after 500 ms, then every 180 ms | `rueda_teclas_f`: fires the selected F-key box on press, after 500 ms, then every 180 ms while held (original UI) | MATCH (not run against the exe) | hechizos_ui.cpp |
| Mouse wheel over no window | 0x45aa70: if option `[0x6da618]` is set, changes the selected F-key slot (wraps 0..7) | `rueda_teclas_f` (called from `dibujar_chat` when the pointer is outside the chat zone): option `shortcutscroll` moves `tecla_f_sel`, a notch up -1 (0 wraps to 7), down +1 (7 wraps to 0) | MATCH (the exe's [0x702948] window-capture test is approximated by `sobre_alguna_ventana`) | hechizos_ui.cpp |
| Click anti-spam | presses <102 ms apart are dropped, and so is their release (0x45b9a2, 0x45b9f7) | none | MISSING | app.cpp input |
| Item drop hold time | dropping on the world needs a hold of >=384 ms (`[0x816550]`, 0x469867), otherwise it counts as a click | drop on release | DIFFERENT | inventario_ui.cpp |
| Shift+click turns in place | Shift only matters for right click (0x469630/0x468c20) | Shift+click turns | EXTRA | pantalla_juego.cpp `mover_jugador` |
| Ctrl+click attack, cursor changes on Ctrl | `[0x702dd6]`, cursor 6; Ctrl+Alt 7 (0x469fb1..0x469fe7) | Ctrl+click attacks | MATCH | - |
| Hold left button to keep walking/attacking | WM_MOUSEMOVE + MK_LBUTTON -> 0x468c20 | yes | MATCH | - |
| Left double click | same as single click (0x469630 handles 0x203 like 0x201) | same | MATCH | - |
| Right double click (GM) | sends opcode 0x28 with map coords; closes window `[0x82467c]` (0x4699bc) | none | MISSING | GM only |

## 5. Keyboard

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| F1 / F2 / F3 | select F-key page 0/1/2 (0x45b087 -> 0x4b2240) | key help / UI toggle / debug overlay | DIFFERENT | app.cpp: page select in original mode; move the dev keys behind a flag |
| Esc | closes ALL pop-up windows (0x8f2f54..0x8f2f64), the palette and shop/trade (0x456c65) | closes the focused window only | DIFFERENT | pantalla_juego.cpp Esc handling |
| Ctrl+V | opens HTML page "option1" in the NPC dialog (0x4569a0, 0x6daa34) | same (`alternar_opciones`); fork Options window only in the simple UI | MATCH | - |
| Ctrl+C | HTML "option2p" when the char-list class byte (`[0x81b534]`, record +0x1e, set 0x470aed) is 0 (prince), else "option2"; page already open -> close (0x456a30) | same (`alternar_comunidad`); Social window only in the simple UI | MATCH | - |
| Ctrl+H (Ctrl+Backspace) | HTML page "help" (0x458360, 0x6daa2c) | same (`alternar_ayuda`); key-help overlay only in the simple UI | MATCH | - |
| Ctrl+X | toggles the local attack-mode byte `[0x703105]` (0x456e30, jump-table entry 11 of 0x4575a0); no packet, no chat line | flips `modo_pk` silently (the fork's `/pk` command is the same flag plus a chat line; it never sent a packet) | MATCH | pantalla_juego.cpp |
| Ctrl+R | flips the option byte `[0x6ddf90]` (the Options action `showfloathpmp`) if the floating HP/MP pair window `[0x8f0810]` exists (0x456d26); that window is built at 0x4b56c0 (buttons 0x53b/0x53d/0x53e/0x540, draw 0x4b5c60) | `float_hpmp_ui.cpp` (U-D9); Ctrl+M has no entry in the 0x4575a0 table of the US client (only the GM paths 0x45a790 / 0x41b380) | MATCH | float_hpmp_ui.cpp |
| Tab | same char as Ctrl+I: inventory (0x456cd4) | only mail/board focus | MISSING | app.cpp SDLK_TAB |
| Alt+X | exit window, same as Ctrl+Q (0x8058) | not mapped | MISSING | app.cpp |
| Alt+C | chat size cycle, same as Ctrl+T (0x8043 -> 0x421330) | ignored in original mode | MISSING | app.cpp |
| Alt+1..4 emotes | 0x40e0d0 -> anim 0x4c96c0 + opcode 0x26 `cc`, per-gfx action table 0x456b30 | none | MISSING | new command + table |
| Arrow-key walking | arrows only go to the chat edit (0x456ba0 / 0x45b087) | arrows walk | EXTRA | U-B3 before removing |
| Shift+F5 filter, Shift+F3 fps, Shift+F12 shot, Ctrl +/-/0 zoom, Alt+Enter, Shift+F4 shadow | no handler in 0x456ba0 / 0x45b087 / 0x45b475 | present | EXTRA | keep as platform keys or move behind a dev flag |
| Ctrl+I/A/S/Q/T/O, Ctrl+1..8, F4, F5-F12, Alt+F10, Ctrl+Y/Z | 0x456ba0 jump tables 0x45755c/0x4575a0, 0x45b2fd, 0x4087c0, 0x4b1ae0 | implemented | MATCH | - |
| GM Alt+I/K/N/R/T, Ctrl+F1-F12, Ctrl+M | 0x4094d0, 0x409640, 0x4095b0, 0x409570, 0x4095f0, 0x45a790, 0x41b380 | none | MISSING | GM only, low priority |

## 6. UI sounds

Play primitive: `call 0x4a9a20` with the id in ECX and volume 0x7f. The fork plays no UI sound at all, only animation
sounds and S_Sound.

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Panel button opens a window | 0xed (237) from callback 0x4d1210 / 0x4583f0 | none | MISSING | ventanas.cpp `boton_panel` |
| HTML page open / close (options, help) | open 0xed, close 0xee (0x4569fd, 0x456a21, 0x456ae5) | none | MISSING | npc_ui.cpp |
| Window close buttons | 0xee (0x4a0ba3, 0x4a0fcb, 0x491bdd) | none | MISSING | original_window.cpp close handler |
| Scroll arrows / thumb | 0xf1 (0x4d1230/0x4d1260), 0xf3 (0x4d1290.., slider 0x47d4c6) | none | MISSING | original_window.cpp scroll bar |
| List row / spellbook tab change | 0xf2 (0x41d8b1, 0x47ec68) | none | MISSING | hechizos_ui.cpp tabs |
| F-key slot assign / clear | 0xf0 after slot writes (0x4489aa, 0x4afe6c, 0x4d3b8b) | none | MISSING | `poner_tecla_f` |
| F-key page arrows | 0x7f (callback 0x456960) | none (arrows inert) | MISSING | with the F-key pages |
| Login / char-select / create buttons | id 1 at 10 callbacks 0x4703f0..0x472f80 | none | MISSING | pantalla_login/personajes/crear |
| Windows opened by hotkey | silent (0x4d1200 plays nothing) | silent | MATCH | do not add sounds here |
| Button hover | no sound | none | MATCH | - |

## 7. Window chrome and window management (all framed windows)

Original base window: ctor 0x4d0700, vtable 0x4f5c00, child container 0x422570, children drawn by 0x4228f0.

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Close button position (inventory, status) | img 969/1061 at (168,18) (0x44cd4d, 0x4b4754); ref `inventory_1.png` "x" centre ~(174,24) | (164,6) | DIFFERENT | original_window.cpp `CLOSE_X/CLOSE_Y` |
| Close button position (spellbook) | (181,18) (0x4aed78) | (164,6) | DIFFERENT | hechizos_ui.cpp: pass 181,18 |
| Close button art | rest 1061; 1062 only while the mouse is held on it (draw 0x47c350, 0x47b4e0/0x47b510) | always 1062, grey tint unless hovered | DIFFERENT | original_window.cpp |
| Close position (trade, shop, quit, NPC, mail) | trade/shop (174,18), quit (108,11), NPC (226,15), mail (209,305)/(213,297) | same | MATCH | - |
| Hover/pressed art, text buttons (0x47c6d0) | hover = 2nd ctor id (quit 1278/1279/1280, shop OK 1291, Cancel 1288); pressed = id+1 (0x47c760) | shows id+1 (the pressed art) on hover | DIFFERENT | original_window.cpp `draw_original_button` |
| Plain image buttons (0x47c2d0: trade OK/Cancel, mail) | no hover art; id+1 only while pressed | id+1 on hover | DIFFERENT | same |
| Window dragging | inventory handler 0x44d1b0 has no move code; only z-order 0x4d0ed0 | every window draggable (ventanas.cpp:196 `arrastrar_ventana`) | EXTRA | note: inventory/status ctors also build a drag handle 0x47bcd0 (0,5,191,25), see U-C1 |
| Drag region (where one exists) | rect (0,5,w,25) (0x47bcd0) | top 22 px from y=0 | DIFFERENT | ventanas.cpp |
| Window count | max 4 pop-ups (0x8f2f54..0x8f2f64); a 5th closes the oldest (0x4d0f30) | unlimited | DIFFERENT | ventanas.cpp `actualizar_ventanas` |
| Right double click closes | WM_RBUTTONDBLCLK inside the inventory -> 0x4d1200 (0x44d2ce) | not present | MISSING | ventanas.cpp (other windows: U-B5) |
| Default positions | inventory (450,0) (0x44cf6a); spellbook (433,0) (0x4aefcb); quit (250,100) (0x4a1485); mail list (0,0) (0x417d50); map (0,0) | inv/status/spells stacked leftward at y=36; quit centred y=H/3; mail centred, y=50 | DIFFERENT | ventanas.cpp `por_defecto` |
| Inventory x clamp | snaps to x=450 if off-screen (0x44d8d2) | clamps with a 40 px margin | DIFFERENT | ventanas.cpp |
| Background opacity | option `[0x6e1008]`: 31 = opaque, else alpha; 0 = no bg (0x44d9c3); steps 31/20/13/7 from the `alpha` action (table 0x6e100c) | `opacidad_ventanas`: 255/165/107/58 by `cfg.alpha_orig`, default opaque | MATCH | - |
| bg/frame/ornament layers | inv 996 (6,8)/997/998 (0,11); status (6,9)/983/984 (0,11); spells 933 (8,10)/934/935 (2,10); quit 1058/1059/1060; trade 1199/1201/1200; NPC 1355/1356/1101 | same | MATCH | - |
| Bring to front on click | 0x4d0ed0 | `al_frente` | MATCH | - |

## 8. Inventory (ctor 0x44ccc0, draw 0x44d830, grid 0x44e010)

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Tab filters | 0 = all (0x44de10); 1 = equipment, type in {1,2,10,18..25,37,40} (0x44dd90); 2 = quest items via table 0x701358 (0x44de40); 3 = misc (0x44df00) | potions+food / equipment / scrolls+magic / other (guessed `tab_of`) | DIFFERENT | inventario_ui.cpp `tab_of` |
| Tab tooltips | strings 878-881 "All", "Equipment", "Quest", "Miscellaneous" (ctor 0x44cfc8) | "Potions and food", ... | DIFFERENT | `TAB_NAMES` |
| Equipped marker | img 536 (35x35) at (x-1,y-1) behind the icon (0x44e22b..0x44e291) | brown box + yellow "E" | DIFFERENT | inventario_ui.cpp |
| Item order | list order | worn items first | DIFFERENT | `orden_inventario` |
| Icon overlays 2700/2701/2702 | drawn by name/flag conditions (0x44e131..0x44e39a) | none | MISSING | meaning: U-C6 |
| Icon position | cell origin (21+34c, 42+34r) (0x445fd0) | +1 px | DIFFERENT | `draw_item_icon` |
| Tab hit-rect | (19+35i, 249) 35x17 (0x47ede0) | 18+35i | DIFFERENT | 1 px |
| Scroll bar | up 974 (161,48), track 971 (161,56), down 972 (162,235), thumb 976 | x=162 everywhere, up y=41, down 233 | DIFFERENT | Grid scroll constants |
| Item hover text | grid draw 0x44e45b: hovered cell (index [0x4c], no delay). Item with the equipped flag ([+8], set by the select call 0x44f19b) and attribute lines ([+0x14], from the S_AddItem status bytes through 0x482630/0x446520): bevel box 142 px at the grid x (21), y = cell y + 32 (above, `-12 k - 52`, when below grid y + 161; clamped to grid y + 10 / + 330), name wrapped to 23 columns, attribute lines to 22, 12 px pitch, text at +5, all in the item colour (0x8f0f34[blessed/normal/cursed/unidentified]); any other item: name only with shadow under the cell, no attributes | same (`armar_tooltip_item`, `draw_original_hover_box`) | MATCH | inventario_ui.cpp |
| Trash tooltip | none in draw 0x44d830 | "Drag an item here to delete it" | EXTRA | remove |
| Tab strip 999+tab at (17,249); grid (21,42) 4x6 pitch 34; trash 1003/1004 at (163,252); no weight text | 0x44db21, 0x44e056, 0x44db9d | same | MATCH | - |

## 9. Options, community and help (HTML in the NPC dialog)

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Options (Ctrl+V, button 1009) | toggle of HTML page `option1` (0x4569a0 -> 0x4568a0; open 0xed / close 0xee): System (option11), Graphics (option14), Volume (`action=volume`), Community (option12), Action (option13), Help; values via `<var src="#N">` (switch 0x438766..0x43a56e: flag set = string 567, clear = 568; accepttrade 573/574; chat size / transparency 893..896; combatMusic inverted), each toggle flips a byte and reopens the page (0x43c510 + vtable [0x54]) | `alternar_opciones` (npc_ui.cpp) opens option1 as a local page; `accion_opciones` / `datos_opciones` (social_ui.cpp) drive getall, bubbleon, dochat, dotradechat, dowhisper, showMargetMsg, randomParty, cPartyReject, combatMusic, shortcutscroll, tooltip, accepttrade, showhpmp (= `cfg.barra_personaje`), alpha (window opacity 255/165/107/58, `cfg.alpha_orig`), chatstate (`chat_tam`), telcon (elf 50+), infracon (elf/dark elf); persisted as `opciones_orig` / `alpha_orig`; the fork option window stays in the simple UI; `volume` opens the original volume window (volume_ui.cpp) | MATCH for pages, links, values and the wired effects; see U-D8 for what stays open | social_ui.cpp, volume_ui.cpp |
| Community (Ctrl+C, hands button 1015) | `option2` / `option2p` -> optionpl/optionplp, optiona/optionap, optionpt, optionb (all in Text.pak, `link=` pages, back image 331); `action=` ids run client commands in dispatcher 0x40b8b0..0x40c8d0: party = packet 0x4d, outparty 0xb, join 0x5e, buddy/refreshbuddy 0x6d, banish 0x48 / ban 0x67 / addbuddy 0x38 / delbuddy ask a name in an input box (vtable [0x702dd8]+0x64), invite = click-on-target mode (0x402490, message 0xb3, then 0x82) | the pages open in the NPC window as local pages (`abrir_pagina_local`, npc_ui.cpp); `activar` -> `accion_comunidad` (social_ui.cpp) for party, outparty, join, leave, pledge, buddy; banish, ban, addbuddy, delbuddy, title, war, surrender, cease, join and leave use the original input box (chat line in prompt mode, `PromptChat`); invite is the click-on-target mode | MATCH for pages, links and prompts; emblem, checkbmp and alliance* stay open (U-D7) | social_ui.cpp |
| Help (Ctrl+H, button 1017) | toggle of HTML page `help` (0x458360, 0x6daa2c) and its ~120 sub-pages (helpgen..helpfont, helpmgc*, helplv*; only `link=` and back image 331, no actions) | `alternar_ayuda` opens `help` as a local page; key list overlay only in the simple UI (F1 too) | MATCH | - |
| Game Tip / Help parchment window | 0x4badd0, img 808/811 (234x180) | absent | MISSING | optional |

## 10. Status window (ctor 0x4b46d0, draw 0x4b4a00)

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| HP/MP/EXP gauge box (attributes page) | img 1263 at (69,46); EXP 1264 (69,46), HP 1265 (69,62), MP 1266 (69,77), clipped by % (0x4b3e8f, 0x4b4562, 0x4b3ff2) | not drawn | MISSING | original_window.cpp draw_attributes |
| Level / EXP text | `"%d"` at (100,50); `"%02d.%02d%%"` at (115,50) (0x4b459b, 0x6e4dbc) | `"%d  (%.2f%%)"` at (70,44) | MATCH (fixed; digits use the 4x6 bitmap font 0x431410, table 0x501410) | -
| HP / MP text | `"%d/%d"` at (100,66) colour (246,189,246) / (100,80) colour (189,222,255) (0x4b4199) | `"%d / %d"` at (54,61)/(54,74), white | DIFFERENT | draw_attributes |
| Weight / food | `" %d%%"` centred in x 107..150, y 94/107 | centred 109..156, y 92/105 | DIFFERENT | draw_attributes |
| Bottom gauge 990-995 | 990 (76,233), 991 (83,232), marker 992 at (83+v,233) (0x4b3930) | not drawn | MISSING | meaning: U-C4 |
| Tab tooltips | strings 882 / 883 (0x4b48b6) | none | MISSING | |
| Tab hit-rects | (22,245),(57,245) 34x17 | y 248, 35x18 | DIFFERENT | |
| Equipment AC text | centre x 120.5, y 231 (0x4b3740) | centre 118, y 232 | DIFFERENT | equipo_ui.cpp |
| Frame/bg per tab and class | attributes frame 0x5a1+0x4d = 1518; equipment bg 1516 when class!=0 (0x4b4a24, 0x4b4c20) | 983 / 1440 always | DIFFERENT | needs 1516/1518 (U-C2) |
| Tab strip 985/986 (22,248); STR..CHA boxes; magic level/bonus; elements; AC; equipment slots table 0x8f0818 | 0x4b4bdd, 0x4b4291, 0x4b42e9, 0x483d44, 0x4b2f50 | same (<=1 px) | MATCH | - |
| Status equipment tab: bg 1440 at (6,9) + frame 1441 at (0,0); icons (32x32, 0x445fd0, no 536 marker) at window + table 0x8f0818 (13 slots filled by 0x4b2f50); type to slot by 0x4b30d0: helm 0, necklace 1, T-shirt 2, armor 3, cloak 4 (types 10/19), gloves 5, ring 6 then 7, weapon 8, shield 9, boots 10, belt 11, earring 12; empty slots draw nothing; AC value white, centred in window x 92..149 at 6 px/char, y 231 (0x4b3740, called at 0x4b4d24); tabs 985/986 at (22,248) | same slots and positions; AC now at the same place and white | MATCH (the selection [+0xa4] is the slot under the pointer, set by the mouse-move handler 0x4b36c8..0x4b3704 and cleared on leave; the second loop of 0x4b3220 draws only the item name, white 0x7fff with a black 0x5004f0 4-way outline via 0x4334f0, at (slot x - 32, slot y + 34) in window coordinates: done, replaces the fork's attribute box in the original frame; AC is the byte [0x8f07df] sign-extended with 0x100 subtracted when >= 0x28 (0x4b0c16, 0x4b3b09, 0x4b4ce2): `ca_original` in decodificar.cpp, so wire bytes 0x28..0x7f read as -216..-129; open: the name is drawn without the "(Worn)" / "(Armed)" suffix, the original's item object [+0xc] text is not decoded) | equipo_ui.cpp |
| Alignment box | img 987/988/989 at (121,167), value in box x 123..158 y 184 (0x4b4d9f..0x4b4e6b) | same after commit 8af9ca5 (the user's known guess) | MATCH | class 3/5 variant: U-C3 |

## 11. Spellbook (ctor 0x4aebe0, draw 0x4af3a0)

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Tab strips | 4 strip groups at (16,38), LEFT of the grid: 936-946 (11 frames), 947-953 (7), 954-958 (5), 959-968 (10) (0x47e860 calls at 0x4aec58..0x4aed4a) | one strip 936+tab at (173,38), right of the grid; grouping by 3 levels (README: inferred) | DIFFERENT | hechizos_ui.cpp `PESTANAS_X/Y` (logic: U-C5) |
| Grid origin | (37,43) (0x458a10, 0x4aaef0 args) | (36,42) | DIFFERENT | `GRILLA_X/Y` |
| SP text centre | x 83 (0x4ae4f7) | 85.5 | DIFFERENT | `SP_BOX_X` |
| Close | (181,18) | (164,6) | DIFFERENT | see section 7 |
| bg/frame/ornament, MR centre 157, y 260 | 0x4af5c4, 0x4ae583 | same | MATCH | - |

## 12. NPC dialog (ctor 0x43f130, draw 0x43f7f0); the window of ctor 0x42dc30 is the private shop

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Polymorph (scroll of polymorph / Shape Change) | no typed prompt: the page `monlist` of Text.pak ("Recent Polys", Release `none`, favourites `mon_fav1..3`, links `monlist0..9` = category sub-pages with `action="re orc"`-style monster names, back link `monlist`) opens in the NPC window: scroll use via 0x408fb7 -> 0x409cf0 (state [0x6f1e60]=2), S_ShowHTML "monlist" via 0x488885 (state 1); any action goes to the default handler 0x40c855..0x40c97b which sends the name and closes the window (0x4d1200): state 1 C_NPCAction with the own id, state 2 the item-use reply (0x77 "cdds") | `abrir_polimorfia` / `accion_polimorfia` (npc_ui.cpp, `npc_poly`); the old typed dialog with suggestions (forma_ui.cpp) is kept for `ui_simple=1` only | MATCH for page, links and reply; open: the original sends an empty name for `none` (this server needs the word), page vs typed prompt is settled (see U-D9): the US default is the page, so nothing else is modelled, the scroll is detected by the server's use type 16 instead of the exe's item type 0x27, `mon_fav1..3` are sent as names like the original default path (no favourites in the US exe) | npc_ui.cpp |
| Window at ctor 0x42dc30 (frame 1284, 4x2 grids "Selling item" / "Buying item", price bars at (34,224)/(34,241), 696 mark, ornament 1285, OK 1291 / Cancel 1288) | it is the PRIVATE shop (callbacks 0x483410, adds items from the player inventory 0x701444, max 8 per grid), not the NPC shop; OK callback 0x42d8f0 builds C_Shop (sell list, buy list, then the two title strings of the edit boxes at (34,224) / (34,241), 40 chars each) | `tienda_ui.cpp` `dibujar_tienda_original` (frame 1284, bg 1283, ornament 1285, two 4x2 grids at x 43 / y 51 and 145 pitch 34 with mark 696, hover box with name and price, OK 1289 / Cancel 1286 / close 1061, draggable `IdVentana::TIENDA_PRIV`); the buyer's side (S_PrivateShop types 0 and 1, both requested on opening) reuses it, a click on a cell asks how many | MATCH (layout); open: how the exe picks the grid for a new item (fork: last clicked grid), line removal (fork: click on its cell), joining of the two title lines (fork: a space), the hover box art (fork: dark box) | tienda_ui.cpp | see section 18 for the NPC lists |
| NPC scroll bar | up (226,47), track 1354 (226,55), down x=227 (0x43f3df, ctor 0x47cd80) | up (226,47), track 55, down x 227 | MATCH | - |
| NPC bg/frame/ornament, close (226,15) | 0x43f25b | same | MATCH | - |

## 13. Prompts: yes/no question and quantity

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Yes/No question | bottom-screen bar: img 701 (320x32) drawn by 0x41fe30, "YES" at (361,447), "NO" at (401,447), no title (ctor 0x4d1cd0 x2, strings 0x6e5288/0x6e5284) | boxed 300x92 dialog with title (social_ui.cpp `dibujar_pregunta`) | DIFFERENT | social_ui.cpp (bar x: U-D4) |
| Quantity / text input | input box 0x4cfa90: img 895 (134x93) + edit field, buttons 916/917, 918/919, 920/921; NOT used by the NPC lists (they step the amount with the row arrows) | generic 260x112 window with -/+ | DIFFERENT | cantidad_ui.cpp `dibujar_cantidad` |

## 14. Mail (list 0x417a90 / draw 0x417d50, letters 0x413500, 0x413a80, 0x416b50, 0x417320, compose 0x455800)

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| List position | (0,0), no drag handle | centred x, y=50 | DIFFERENT | correo_ui.cpp `x0()/y0()` |
| Mailbox structure | separate window classes per role | one window, 3 mailboxes switched by clicking the header | DIFFERENT | roles: U-C7 |
| Row text | id `"%04d"` (0x500308), name w72, date w48, ink (65,65,65) | id unpadded, date "%m-%d %H:%M" w80 | DIFFERENT | `draw_original_mail` |
| Header | white (0x7fff) at (30,15), row -2 marker 657 at (18,15) | "%s  %d/%d" in ink | DIFFERENT | |
| Reading letter 0x417320 | paper 671; buttons 686 (159,302), 678 (213,297), 674 (90,299); 672 (56,312), 676 (110,312); text (20,20) and (18,269) | paper 671, buttons 664 (60,302), 580 (120,299), "Keep" link | DIFFERENT | correo_ui.cpp |
| Compose 0x455800 (paper 577) | edits at (85,47), (85,64) 136x16, body (45,103) 192x160 | field y 66/86/108 | DIFFERENT | correo_ui.cpp `fy[]` |
| Memo editor (paper 1545: 0x47fd10/0x453120/0x4541b0) | draggable, format buttons 0x4ff-0x503, close (230,5) | none | MISSING | |
| List buttons, paper 656, 8 rows, marker 657 | 0x417b25..0x417c7d | same | MATCH | - |

## 15. Trade (ctor 0x42bf20, created at 0x48c07b, draw 0x42d280)

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Name labels | two white lines at (43,31) and (43,159) (0x42d4e4) | "%s offers" at (27,159) only | DIFFERENT | social_ui.cpp `dibujar_intercambio` |
| Scroll bars | two bars at x=168, y 54 and 183, imgs 1206/1208/1210/1212 (0x42c327, 0x42c3bd) | none; only the first 12 items visible | MISSING | `dibujar_intercambio` |
| OK/Cancel hover | no hover art (0x47c2d0) | 1203/1205 on hover | DIFFERENT | section 7 |
| Frame 1199/1201/1200, close (174,18), OK (53,282), Cancel (100,282) | 0x42c0c9, 0x42c13a | same | MATCH | - |

## 16. Quit window (ctor 0x4a1180, draw 0x4a1570)

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Position | (250,100) (0x4a1485..0x4a14af) | (255,160) | DIFFERENT | ventanas.cpp |
| Hover art | 1278/1279/1280 | 1064/1066/1068 | DIFFERENT | section 7 |
| Images, close (108,11), buttons at (31,34)/(31,52)/(31,70) | 0x4a1218 | same | MATCH | - |

## 17. World map (ctor 0x46aff0, draw 0x46ba90)

| item | original (evidence) | fork | status | fix hint |
|---|---|---|---|---|
| Drag rect (54,45) 200x125 | 0x46b0db | same | MATCH | - |
| Right double click closes, (39,30)-(245,170) | 0x46c1ca..0x46c213 | same | MATCH | - |

## 18. Pets, summons, warehouse, lists and other windows

| item | original (evidence) | fork | status | fix hint (file/function) |
|---|---|---|---|---|
| Pet control window | 0x494820: frame 1170 (NAME/LEVEL/Exp/HP/MP, State, Hunger, [Equip] 4 + AC, [Command] 4x3), 1169, 1171, bars 1172/1173; command grid 0x493e90 sends aggressive/defensive/stay/alert/attackchr/getitem/changename/extend | `dibujar_mascota_orig` (mascotas_ui.cpp): bg 1169, frame 1170, ornament 1171, command buttons 1183 / 1185 / 1193 / 1187 / 1179 / 1191 at x 45 / 79 / 113, y 180 / 214 (the NPC action of the pet: aggressive, defensive, stay, extend, alert, attackchr; the exe's case 7 closes the window), Bag 1351 at (73,135), HP fill 1173 from the S_HpMeter %; opens on S_PetCtrlMenu, closes with the X | PARTIAL (the server sends no NAME / LEVEL / Exp / MP / State / Hunger data; name and HP bar positions guessed on the art; the exe's second ctor variant (0xb type, 9 buttons incl. getitem and 0x547) not used) | mascotas_ui.cpp |
| Summon control | 1196/1197 (0x4a9cd0, 0x4a6c90) | none | MISSING | mascotas_ui.cpp |
| NPC lists: shop buy / sell, warehouse (personal 3, clan 5, elf 9; deposits 2 / 4 / 8), pet bag, spells, repair | one window class 0x4f55d8 (ctor 0x4bef40, draw 0x4bf690, mouse 0x4bfdb0) over a row pane 0x4f557c (draw 0x4be220, rows 0x4bd250); type set by 0x4d1000 (byte table 0x4d115c: 0 buy, 1 sell, 2-5/8-12/19-21 OK, 7 buy, 16 OK ...); frame 1100 (258x335) bg 1099 (8,11), ornaments 1101 (2,11), close (225,15), label 1106 (29,31) + number (80,31) "total/adena" (red 0x5ca7 when it exceeds), pane (24,44)-(225,304) 7 rows of 36 from (25,51), scroll up 974 (225,47) track 1354 y 55 down 972 (226,293) thumb 976; row: icon at origin, name (+38,+3) with its 2nd line (+38,+17), amount right-aligned to +176 (y +19) between arrows 556 (left) / 554 (right at +180), price left of the left arrow with coin 325; selected row mark 536 at (-2,-2); chosen rows tinted; hover tooltip: 142 px box darkened 50 %, 2-tone border, text at (+5,+5); buttons y 306: Buy 1109 / Sell 1107 / OK 1114 at x 79, Cancel 1111 at x 131 (pressed art id+1); "%d/150", "/200", "/100" at (193,309) for retrievals 3 / 5 / 9; fee 30 per chosen row; the window type is the C_Result type | built from the shop-style 4x2 grid of the NPC dialog | MATCH (see open items) | npc_ui.cpp `draw_list_original` |
| Party / alliance / buddy lists | frame 979+980 (978 back), scroll 971: 0x497240 ("party" 0x406994), 0x42fb40 ("alliancelist" 0x406900), 0x419040 ("buddy") | `lista_ui.cpp`: the S_ShowHTML pages party / pledge / buddy open a window (bg 978, frame 979, close, scroll bar, 17 rows of 13 px from (25,49), online rows tinted) in the original UI | PARTIAL (scroll bar / title positions, row height and which flag the exe tints are guessed; the alliance list has no packet here) | lista_ui.cpp |
| Calculator | 0x41da00, imgs 1359/1360/1361, "calc" command (0x4ff5a8) | absent | MISSING | optional |
| Social window | none in the original (community = option2 HTML) | simple UI only; the original UI opens option2/option2p | MATCH (original mode) | - |
| Castles, light, weather, auto-buff, auto-potion, shortcut bar | no window in the original | none, or hidden in original mode | MATCH | - |

Windows whose original form is unproven are listed in U-D2: private shop, board, houses, shape change, markers,
attribute reset.

---

## Known fork guesses (flagged)

- **Shop text bars and tooltip** (npc_ui.cpp `draw_shop_original`): the original values are now recovered, see section 12.
  Both are DIFFERENT.
- **Status window alignment box:** it matches 0x4b4d9f..0x4b4e6b. The class 3/5 variant at (125,164) is not handled (U-C3).
- **Chat scrollbar and "A" button positions:** they match 0x47cd80 / 0x4599ea and 0x459b30. The "A" and 1045 buttons are
  inert (section 1).
- **Panel button rest look** (ventanas.cpp `boton_panel`): sepia conversion fitted to a screenshot. DIFFERENT.
- **Cursor roles "chosen by visual match"** (cursores.cpp): lips and coin are wrong (section 2).
- **Spellbook tab strip placement and level grouping** (README: "inferred"): DIFFERENT (section 11).
- **Buff icons marked "Inferred"** (hechizos_ui.cpp:668).
- **Ctrl+X -> "/pk"** (settled: it is the same local flag, now silent) and the **"Ctrl+R dragon-power panel"** comment (settled: Ctrl+R is the floating HP/MP window, U-D9).
- **Inventory tab grouping** (`tab_of`): now the original filters (all / equipment by use type / quest via itemquest.tbl / misc).
- **Chat colours** (chat.cpp): no source cited; wrong.
- **Game-time orb:** placed by template matching, not traced in the binary (U-A9).
- **Zone labels:** placed by screenshot only (U-A8).
- **NPC text rect** (`NPC_TEXT_*`): marked "unverified" (U-C8).
- **Status equipment slots:** equipo_ui.cpp calls them a "deduction", but they are verified against 0x8f0818.

## UNKNOWN (not proven; what would settle it)

- **U-A1** Blend flag 0x8003 in 0x4b7fb0 (panel button rest look, overhead bars) — decompile 0x4b7b80 and the overlay
  `[0x8f0b14]`.
- **U-A2** Default of the self-bars flag `[0x8169ac]` — find its writers.
- **U-A3** Meaning of `[0x702b7c]` (the gate for level colours; also the alignment 3/5 variant) — find its writers.
- **U-A4** Mapping of chat jump-table types (0x484a14) to L1J chat packets — trace the S_Chat handlers to the index they
  push.
- **U-A5** Click behaviour on chat lines (no handler found, not proven absent) — trace 0x41f650's neighbours.
- **U-A6** Speech bubbles / overhead chat: which object field and draw site; the fork's style is invented — trace the
  chat-packet store into the object.
- **U-A7** (emblem settled, see U-D11) Clan emblem, crown, vendor title, True Target mark: whether the original draws them — trace the label draw
  after 0x4676f5.
- **U-A8** Zone label colours, font and y (matched to a screenshot only) — trace the label draw.
- **U-A9** Game-time orb frame mapping — trace its draw.
- **U-A10** F-key slot item/spell overlays (count, dimming) in 0x445fd0 / 0x4aad90.
- **U-A11** Actions of the 1041 "A" and 1045 buttons (0x421430, 0x474a47).
- **U-A12** Exact Ctrl+T sizes (0x421330).
- **U-A13** Blindness overlay, death/restart dialog, Ctrl+P message for GMs.
- **U-A14** Original chat-input prefix parsing (`! & $ @ # %`), input history, whisper reply.
- **U-D9** Polymorph page vs typed prompt, and page scrolling (decoded in S3EP1U). 0x409cf0 (reached from the item-use switch for the polymorph scroll, 0x449860 / 0x44c4a3 / 0x4b355d, and from 0x408fef) tests byte [0x4ff208], initial value 1: non-zero opens the page `monlist` (0x4ffce4; or the page named in [0x7011b4] for other item types), zero opens the typed-name prompt (0x409e00, callback 0x409e40 sends `cdds` 0x77 with the name). The byte is only toggled by the chat command `morphwin` (0x40b693), so the US default is the page. [0x8ed570] is slot 2 of the six server-set protected ints at 0x8ed558 (setter 0x4a5620, reset to 0 at 0x45a360, set by the packet handler 0x488da0 from a 2-field message): when it decodes to 1 the scroll skips the 0x77 round trip and calls 0x409cf0 directly (0x408fe2), otherwise the 0x77 request goes to the server first. Neither the `morphwin` toggle (typed prompt, kept for `ui_simple=1` only) nor slot 2 is modelled. Page scrolling (page object, pixel offset [+0x74], content height [+0x78]): line up/down (0x43c080 / 0x43c0b0) step 13 px (font height, 0x435370) and stop at content height - 13; page up/down (0x43c0f0 / 0x43c120) step 277; the thumb maps y 34..262 to offset total * (y - 34) / 228 (0x43c170); the draw rect is intersected with the page rect (OffsetRect / IntersectRect at 0x4367e5..0x436816). The fork steps whole 13 px lines with arrows and thumb (no 277 px page-step buttons, thumb quantized to lines), shows the bar only when the content overflows the view (the exe seems to scroll any page; its minimum content height at 0x43bb7d is not decoded), and the mouse wheel (one line per notch) is a fork convenience: the exe page has no wheel handler (the 0x20a handlers at 0x466045 / 0x4c16e4 belong to other controls). Clipping: images are cropped to the page rect, text runs are shown only when they fit whole (the GPU has no scissor), so a half-visible text line disappears instead of being cut.
- **U-D8** Options pages (0x40b1de..0x40c660), settled: `volume` opens the volume window (0x4aa3b0 draw, 0x4a9cd0 ctor, volume_ui.cpp: frame 1197, background 1198, close 969, labels 792/793/794/1016, three 0..100 sliders with bar 1269 / thumb 1272 / arrows 1270 and OK 1274; music and effects drive `cfg.volumen_*`, the third ("Environmental Sound", [0x6e42b8]) has no audio channel in the fork and is only remembered). dochat / dotradechat / dowhisper send `ccc` 0x19 (what 0 / 6 / 2, new flag) = C_LoginToServerOK (`Cli::LOGINTOSERVEROK`, wire 75 jp363 / 25 s3ep1, not mapped for tw380). Effects wired: getall off asks how many of a stack on pickup (0x408529 -> prompt 0x79 -> 0x4086f0); randomParty is the type byte of the invite packet (0x4075b3); cPartyReject refuses party invitations (question 951) by itself with string 1005 (0x4887e6); accepttrade off refuses trade requests (question 252) silently (0x48876d); bubbleon draws the balloon frame (0x41a6c7); showMargetMsg lets trade chat ($) show over the speaker (0x41a4f2); tooltip gates the panel-button hover hints (0x4bbd20 / 0x4bbd30). Not acted on by the fork: combatMusic (flag read at 0x46816f / 0x4c53ed to pick the combat music; the fork has no combat music), shortcutscroll (wheel handler 0x45aa70 cycles `[0x703100]` 0..7 and the 0xb4 ms timer 0x45ab60 then fires slot `4b1ae0(slot, 1)`: done, `rueda_teclas_f`), telcon / infracon (elf-only vision toggles, fork keeps the flag). showhpmp is `[0x8169ac]` and the Options action `showfloathpmp` is `[0x6ddf90]` (the floating HP/MP window, U-D9); the fork maps showhpmp to its own-character bar, unverified against 0x466e18. Also in the dispatcher and not in the fork's list: doreceive / domail / doexchange (flags 0x4ff205..07, same packet 0x19 via 0x406dbb..), wantparty, morphwin, autotrade, keyboard, obscene, violence. The Text.pak pages option11/option14 also list actions the US exe lacks (dropItemView, npcNameView, HPMPBarThicknessSet, mouseCursorChange, uitype, windowMode, fullScreenMode, win*) and vars 1032, 1037, 2026-2028: no handler/case in S3EP1U, kept as no-ops with empty values.
- **U-B1** (settled: option*/help actions and vars decoded, see section 9 and U-D8)
- **U-B2** Click decision tree (attack / talk / pick up / walk, own pet) in 0x468c20, 0x469d90, 0x4083b0, 0x4cd9b0,
  0x4cdd30. This settles whether the fork's "click own pet talks" rule is original.
- **U-B3** Keyboard walking and PgUp/PgDn: imports are packed — unpack the IAT or trace under Wine.
- **U-B4** Shift+Space (0x414be0) and the Kana key.
- **U-B5** Whether every window shares the inventory's no-drag and right-double-click-close behaviour — check the slot
  +0x2c of vtables 0x4f5394, 0x4f5220, 0x4f27b0.
- **U-B6** Double click and right click on an inventory item (use/other) — decode the inventory slot handler.
- **U-B7** (settled: default is 1, initial byte of 0x6da618; the wheel handler is analysed in U-D8 and left open)
- **U-B8** Esc side effect `[0x8f0e22]`.
- **U-B9** Hold-to-walk repeat timing.
- **U-B10** Sounds 0x11b7/0x11b8 (spell target), 0x120b, 0xb14, 0xd1, item sounds from map 0x700ab0; error beeps.
- **U-B11** Which windows' close buttons use the 0xee callbacks.
- **U-C1** Whether inventory/status are draggable through their 0x47bcd0 handle even though 0x44d1b0 has no move code;
  and whether buttons fire on release (0x47b510) versus the fork's `e.clic`.
- **U-C2** Images 1516 / 1518 (not in the src_ui manifest) and `[0x81b328]` (class or sex) — decode Sprite.pak.
- **U-C3** The alignment-image variant at (125,164) when `[0x702b7c]` is 3 or 5.
- **U-C4** Meaning of the status-page gauge 990-995 (0x4b2e31, 0x4b2cc0).
- **U-C5** Which spellbook strip group is shown and what a tab selects.
- **U-C6** Inventory hover popup (0x44be70 / 0x44bdf0), overlays 2700-2702, stack count, item name colours.
- **U-C7** Roles of the mail classes 0x416b50 / 0x417320 / 0x413500 / 0x413a80 and string ids in 0x416e40.
- **U-C8** NPC text rect; default positions of status, NPC, shop and trade (no code in their ctors; possibly a saved
  layout).
- **U-C9** Trade grid rects (base 0x42cb60), the "Waiting" state, equipment icon size.
- **U-C10** Quit buttons 1/2 mapping to Restart / Character select (0x4a0ba0, 0x4a0fc0).
- **U-C11** Weight/food denominators 29 vs 240/225 (`[0x6f1da8]` >= 70083) — find its writer.
- **U-C12** Map window internals (image table 0x6de444, mask, marker) were taken from fork comments and not re-verified.
- **U-D1** Default screen position of the remaining framed windows (window manager 0x422650 / 0x4d0f30, saved layout).
- **U-D2** Original windows for the board (S_Board), houses, private shop (done, see section 12), teleport markers and
  attribute reset — trace the server-opcode dispatch to the window global each one touches.
- **U-D3** (mostly settled) Party / alliance / buddy / pledge list windows (0x497240 / 0x42fb40 / 0x419040 / 0x4b92f0): same shape (bg 978 at (8,11), frame 979, scroll 971, up to 17 rows from (25,49) via 0x497160 with the 0x498d90 tint over 145 px for a flagged row); `lista_ui.cpp` draws party / pledge / buddy from the S_ShowHTML data (party: name, members; pledge: clan, online, all; buddy: list, online). The mapping window -> exact frame (0x42fb40 = alliancelist, 0x4b92f0 = pledge?) and the row height are not verified.
- **U-D4** Exact x of the Yes/No bar (0x41fe30 adds the view origin + 0x89).
- **U-D7** (mostly settled) The original input box (vtable [0x702dd8]+0x64 = 0x4451d0) is the chat edit line in prompt mode: the prompt string goes to the chat log and the next typed line goes to a callback (Enter: arg 0 + text; Esc: arg 0x1b) instead of the chat; the fork does the same (`PromptChat`, `abrir_prompt` / `confirmar_prompt`). Prompt strings: 0x24c add buddy (callback 0x40d470 -> 0x38), 0x24d delete buddy (0xa), 0x199 banish (0x406600 -> 0x48), 0x6b ban (0x406550 -> 0x67), 0x24a title (0x40d3b0 -> 0x406000 splits "<name> <title>" -> 0x42), 0x241 war / 0x242 surrender / 0x243 cease (princes only; C_War 0x65 with type), 0x313 join (prince only; others send 0x5e directly; callback 0x405f60 needs y), 0x1e4 leave (0x405fb0 needs y -> 0x1b). Invite is click-on-target mode (0x402490: message 0xb3, mode 3 = reach 6 x 3 cells, callback 0x407550 writes string 0x3bb and sends 0x82 with the target id). Emblem 0x40d030 / checkbmp 0x40cfd0: see U-D11. Alliance: the exe sends `cc` 0x4e sub 3 / 4 / 2 with no name (0x4053b3..0x4053e3, 0x40c818..); join is face to face, leave asks y/n first (0x401d86); the fork maps RANK = 78 for s3ep1 and l1j-en handles it (patch 0100). The list window of 0x42fb40 is not the alliance list (it is a global used by an unrelated toggle at 0x406900). Still unported: pledgefocus*. War: the original callback 0x40d120 goes through 0x402300 (extra checks); the fork sends C_War type 0 with the typed clan name. The party / pledge / buddy list windows the server answers with (U-D3) are shown by the existing HTML path.
- **U-D9** (settled) Floating HP/MP pair window `[0x8f0810]` (ctor 0x4b56c0, vtable 0x4f53cc, draw 0x4b5c60), toggled by Ctrl+R and the `showfloathpmp` action (`OPC_FLOATHPMP`, default on): `float_hpmp_ui.cpp`. Frame 1330 + 3 * style at the top centre, bars 1331 / 1332 cut to value / max, poison / frozen HP bar 1345 / 1346 + 2 * style, "%d/%d" with the 0x5949 / 0x3214 outlines, style button 1339 (cycles 0..2, [0x8169b0]), close 1342, drag handle clamped to y <= 430. Unverified: the exact rects of the three controls (read from the ctor arguments), the hover images of the buttons (1341 / 1344 assumed) and the 0x8 / 0x1 bits of the object state mapping to frozen / poisoned.
- **U-D5** 1480 window (0x4d43f0) versus pet window 1170; uses of 0x4208a0 (frame 1321) and 0x4b56c0 (1330).
- **U-D6** Whether 0x44f5c0 (frame 997) is the pet inventory or the warehouse deposit side.
- **U-E1** (NPC lists, mostly settled): (a) the chosen-row tint is the table blend 0x498d90 with table 0x8f133c: the tables are built at 0x4d1600..0x4d16b7 (0x421aa0: for each low byte, blue = fn(low 5 bits), the rest copied; the high-byte half is fn2 = `ret`, i.e. unchanged) and table 0 uses 0x4d1590: blue -> (2 * blue + 31) / 3, red and green untouched, which a screen blend of (0,0,85) reproduces exactly (`rectangulo(..., Mezcla::PANTALLA)`); (b) the tooltip border is the eight 0x498720 lines (x, y, w, h, colour: light [0x50050c] = 189 along the top 141 px / 140 px and the left 2 px, dark [0x500514] = 123 along the bottom and right, each inner line one pixel in), over a box whose pixels are halved by 0x49a440: implemented exactly; (c) the property lines come from the status bytes of S_ShopSellList (kept in `ITEM_TIENDA` s[1], `FilaTienda::atributos`, decoded by `describir_atributos` like the inventory tooltip; rows built from the inventory use their own bytes); the exe's exact wording and line height of those lines is not verified (the fork reuses its inventory strings, 12 px lines); (d) the hover row is (y - first row) / 34 from the first row at y 51 (the 0x78787879 magic at 0x4be1b6), within x 25..60 of the row, while rows are drawn 36 apart: the drift is reproduced. Still open: (e) types 0xd / 0xe (repair, 0x4beb30) and the pet bag (0x0b) keep the fork flow (click on the row), their Buy / OK art follows the byte table only for 0xd; (f) deposit-pledge / deposit-elven checks (clan membership, message strings 0x2d8 / 0xd0 at 0x40a2ba) are left to the server; the "donate" action (type 0xa, 0x40a436) and retrieve-char (type 0xc) are not opened by the client; (g) `0x4bf690` mode word [0xa0] values other than 7 (0xbb8 for some NPC 0x8f states) hide the Adena label: not modelled; (h) the quantity wrap at the shop cap 1000 per row and the 1 000 000 / 10 000 000 price limits are from 0x4bd840 / 0x4bd8a0 (read, not run).

## Polymorph page `monlist` follow-up

- `<var img src="#2021..2023" tooltip=...>` (inside `<may>`) in the Text.pak `monlist` page are favourite-slot variables of a
  newer client (S3EP1U has no `may` tag and its var handler 0x4386a2 only knows ids 0x384..0x406; any other id is left empty at
  0x43a777). They were drawn as pictures 2021 (town map) and 2022/2023 (a 181x270 pet-status image). Now they draw nothing.
  UNSETTLED: the newer client's real favourites rendering (`mon_fav1..3`) is not modelled; no favourites without server data.
- The pet-status window in the report was not a pet window: it was picture 2022 from this page. `dibujar_mascotas` is only the
  small pet list and is empty without pets.

- **U-D10** NPC page text origin and tooltips (decoded in S3EP1U). Page widget ctor 0x43e170 passes rect (10,48)-(207,299); the layout (0x43b3e0, SetRect 0x43b4ea) works in the inset rect (13,51)-(200,296) and the draw (0x436680 / 0x43cf90, run RECT at 0x43d0a4) adds the widget left (pane x + 10) to each run x, so the first column is at window x 23 and lines wrap at x 210 (width 187); applied in `npc_ui.cpp` (replaces the eyeballed 24 / 194). Vertical origin decoded: the draw rect top is line y (layout, starts at 51) + widget screen top (window y + 48, the rect passed by value at 0x43e815) + OffsetRect dy (-43 - scroll [+0x74], 0x4367cc and 0x436804), so the first line is at window y 56 (not 51); the clip region (0x4366cb..0x4366db: left-3, top-3, right+9, bottom+3 of the widget) is unchanged. Applied as `NPC_TEXT_ORIGIN_DY` in `npc_ui.cpp`. The HTML tag/attribute table (0x503604) has no `tooltip` (the string at 0x4ff420 is a chat command), so the original shows no image tooltips on pages; the fork draws none in the original UI (monlist tooltips were plain monster names, e.g. "Orc").

- **U-D11** (settled) Clan emblem and combat music, decoded in S3EP1U. **Emblem upload** (page action `emblem`, prompt string 0x240 -> 0x40d070 -> 0x4070c0): checks in order player exists (string 0xdb), clan name set (0xdc), own rank nibble 0 or 4 = prince (0x311), one upload per 59876 ms (0xe5); the file name is opened as typed (relative to the game dir), must be a BMP, 16 x 12, 1 plane, 24 bit, uncompressed (info header 12 or 40), else "Incorrect BMP File format". Pixels become 384 bytes of RGB555 (0x4247a0, bottom-up file rows -> top row first) and the client sends opcode 0x83 + two extra bytes (0x01, 0x78) + the 384 bytes (length 0x183). The fork sends only the 384 bytes (the L1J C_Emblem stores bytes 1..384 of the packet, so the two extra bytes would shift the image). `checkbmp` (0x40cfd0 -> 0x406fc0) runs the same validation and shows the image 10 s above the own character at tile anchor (-32, -86) (0x4669d1). **Download / cache**: S_Emblem (d clan, 384 bytes at +5, handler 0x489e80) is stored by 0x424a10 in a cache of up to 512 clans (a full cache replaces the oldest); `Emblem.dat` (`Emblem%d.dat` per server), u32 0x200, u32 count, count x {clan, offset, time} sorted by clan, count x 384 bytes; loaded at start (0x424640), written at exit when dirty (0x424870). Draw lookup 0x424930: miss = request `cd` (C_Clan, clan id, max 32 pending) and draw nothing. The fork keeps the file in the settings folder (`LIN_CONFIG` folder in tests), per server port (`Emblem.dat` for 2000). **Positions**: objects without a name label draw the emblem at tile anchor + (17, -52) (0x462d60; the hovered / targeted object has none there); with a name label it is 19 px left of the name (0x467534). Not modelled: the walk-interpolation offset tables (the fork anchors to its own smooth position), the cache hit refreshing the entry time. **Combat music** (`combatMusic`, byte 0x7030f0 non-zero = off, the fork's bit): 0x4c53a0 runs on every local attack / cast at a target (dl = 1) and when an object attacks the player (dl = 0); it stamps the combat time ([0x8f0df8]) and, unless the option is off, a war state is active or Hunting / PlayrtKilling already plays, starts music.def "PlayrtKilling" (music7.mp3) when the target is a character (type 5), else "Hunting" (music6.mp3), directly (no fade). The 500 ms tick (0x468113) ends it 15000 ms after the last event (or at once with the option off): 0x479ed0(-1) picks the zone track, and a playing track fades out first (timer 0x479dd0: volume -8/127 every 50 ms, about 0.8 s) before the zone track starts. War (0xb) and siege (0xe) tracks use the same switch; not modelled. Implemented in `sonido.cpp` (`combate_musica`) and `Audio::musica_fundido`.

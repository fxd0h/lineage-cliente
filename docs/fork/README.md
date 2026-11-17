# Fork documentation

This fork of [javiercorrea/lineage-cliente](https://github.com/javiercorrea/lineage-cliente) (GPL-3.0)
focuses on running Lineage 1 natively on Linux with a look and feel faithful to the original client.

## Conventions

- **Language:** all new code (identifiers, comments), commit messages and documentation are written in
  **English**. Existing Spanish code is translated gradually, only when it is touched or refactored.
- **Standard:** C++23 (`CMAKE_CXX_STANDARD 23`). The project builds without exceptions or RTTI, so use
  `std::expected` through `has_value()` / `*`, never `.value()`.
- **Upstream docs** (`AGENTS.md`, `ENTREGABLES.md`, `docs/DESCUBRIMIENTOS.md`, `docs/analisis/`) are kept
  as reference and not edited for fork-specific changes. Fork changes are documented here.
- Still in force from upstream: test only against the local server, never commit game assets, no
  allocations in the frame loop, zero warnings and all tests passing before every commit.

## Changes so far

### Linux port

- **Renderer:** GLSL shaders in `src/render/shaders/*.vert|frag`, compiled to SPIR-V by
  `herramientas/compilar_shaders.sh` into `src/render/shaders_spirv.h` (requires `glslangValidator`;
  rerun after editing a shader). `Gpu::iniciar` requests `MSL | SPIRV` and picks SPIR-V on Vulkan.
  Verified on Intel Iris Xe (Mesa ANV): `GPU: vulkan (SPIR-V)`. macOS/Metal uses the hand-written MSL twins in
  `src/render/gpu.cpp` and `shaders_msl_anime4k.h` (every shader has one, HD text included); a new or changed GLSL
  shader needs its MSL edit too. Verified on Apple M5 Pro: all `escalado` modes and `texto_hd` (`GPU: metal (MSL)`).
- **Case-insensitive paths:** `ruta_real()` in `src/plataforma/archivo_mapeado.*` resolves game files
  ignoring case on Linux (cached directory listings, mutex-protected). No-op on macOS/Windows.
- **Local server scripts:** `servidor_local/*.sh` work on Linux via `servidor_local/comun.sh`
  (apt instead of brew). MariaDB runs as the user on port 3307; game server on port 2000.
- **Build fixes:** missing `<string>` includes; four pre-existing warnings fixed.
- **Linux packages for SDL3:** see the "Requirements (Linux)" section in the main README.

### Original-client look (`ui_simple=0`)

- **HD text** (`texto_hd=1`, default): see [hd_text.md](hd_text.md).

- **Font:** `fuente_original=1` (default) draws printable ASCII with the game's `font/eng.fnt` instead
  of Departure Mono; other characters fall back to the TTF.
- **NPC HTML pages** (S3EP1U, all addresses are VAs): the 2009 client does not use eng.fnt there. Page text is a GDI
  font (`CreateFontIndirect` at 0x4359f0: charset 0, family 0x22, face "Arial" from 0x5037a0, height 13 from 0x435370,
  weight 400/700 for `<b>`), proportional (GetCharABCWidths, 0x435900), line height = tmHeight (0x43d90c), drawn with
  `ExtTextOut` (0x43d4a7), no outline, no shadow. Colors are raw COLORREFs: text 0xEEEDEE (0x435cc6), link 0xB0FFFF
  (0x437830), `fg=` hex not byte-swapped (0x43ad50), hovered link (10,240,50) (0x43d2ac), links not underlined (the
  underline flag, style +0x16, is never set by a tag). The text rectangle is the window rectangle inset (3,3,7,3)
  (0x43b4ea). The fork draws it with pre-rendered hinted 1-bit Liberation Sans glyphs at 11 ppem (lfHeight 13 = ascent 10 + descent 3;
  XP draws Arial that small unsmoothed; tools/generar_fuente_html.py). The page widget rect is (10,48)-(207,299) (0x4589b0,
  ctor 0x43e170), drawn +11 px in x from the frame origin. Unknown: exact Arial hinting (a stand-in), `<b>` advances (stand-in:
  hinted Liberation Sans Bold, `<b>` is rendered bold in pixel and HD mode), the x offset of the widget rect (inferred from the panel borders).
  eng.fnt itself (chat and other `0x4334f0` text): 95 glyphs x 12 bytes, fixed 6 px advance, 12 px rows, drawn with
  a black 1 px outline mask built at 0x431a20 (8x14 cells, blitted at x-1,y-1 first, 0x4333b3-0x433407).
- **Zoom:** the window title bar no longer counts toward the scale, so a 1080p screen reaches ×2
  (a 640×480 view, as in the original).
- **Names and bars follow the visible body** when the stepped camera (F4) is on, so they no longer
  oscillate while walking.
- **HUD:** names only for the hovered object and the attack target; HP above MP over the character;
  no hotbar, "Auto" slots or minimap; buff icons top-right without timers; map/zone name bottom-left
  (from `zone3-e.tbl`).
- **Overhead bars:** golden 1 px frame, dark background, three-row gradient fill.
- **Loading placeholder:** sprites still loading are not drawn (upstream drew a green block).
- **Sticky attack:** holding the button on a monster keeps attacking; dragging the pointer out of the
  window while holding latches the attack until a new click, a move, target death or map change.
  Releasing over the panel keeps the attack going.
- **Recovered from the 2009 US client binary** (`S3EP1U.bin`, read with Ghidra/objdump): original
  cursors (PE resources), buff icons 5240+ with their table and screen blend, overhead bars 930–932,
  level-up effect 2127, game-time orb 1213–1246, knight skill icons (table at 0x503c00), blessed items
  lightened toward white (LUT 0x8f173c) and cursed ones tinted red.
- **Zone type label** ("Normal zone" / "Safety zone" / "Combat zone") from the `.s32` attribute bits.
- **Original framed windows** (`src/app/original_window.cpp`), layers and offsets read from the 2009
  binary: inventory (996 background, 997 frame, 998 ornaments, category tabs 999–1002, trash can
  1003, image scroll bar 971/1206–1211) with a 4 × 6 grid and worn items first; status window with
  the equipment (1440/1441) and attributes (982/983) tabs (985/986), values in the image's boxes; spellbook (933 background, 934 frame, 935 ornaments, SP/MR in
  its boxes, tab strip 936–939) with a 4 × 6 icon grid and four level-group tabs (1–3, 4–6, 7–9, rest;
  the tab strip placement and grouping are inferred, not read from the binary).
- **Panel:** F5–F12 boxes take items as well as spells; equipped items are listed first in the inventory.

### View and scaling

- **Fixed view** (`vista=fija`, default): the original 640×480 view rendered off-screen and scaled to
  the window; `vista=amplia` keeps upstream's growing view.
- **Filters** (`escalado=`, Shift+F5 cycles them): `entero` (nearest), `xbr`, `fsr`, `crt`, `anime4k`.
- **Resizable borderless window** (SDL hit test on the edges).
- **Frame pacing:** MAILBOX present mode with a 60 fps limiter.
- **HD UI pack** (`hd=<dir>`): optional 4× upscaled UI images (`img_<id>.png`, `tbt_<id>.png`) drawn in
  place of the originals at the same logical size. The pack is not distributed (it is derived from game
  data); it was made with Real-ESRGAN x4plus-anime.

### Maps and sprites

- Second ground layer of `.s32` maps, translucent tiles (bit 0x20), void tiles (bit 0x08).
- Frames without tiles, `]N` frame effects, and sprites without a `sprite_table` row drawn by heading.
- Wand of polymorph asks for a target.
- **Plaintext sprite list** (`lista_sprites=<path>`, else `list.spz` / `spritelist/list.spz` in the game
  folder): `src/recursos/lista_sprites.cpp` parses the game's plaintext `list.spz` (`#gfx frames=alias
  name`, then `id.name(dir n,sub.frame:ticks[suffix] ...)` actions and `101/102/104/109/110` attributes)
  and adds the gfx that `sprite_table.txt` lacks (gfx already in the table keep their rows: list.spz comes
  from a much newer client whose rows may not match our 3.63c Sprite.pak; the startup log counts both). It covers newer gfx the old table lacks (e.g. 8913 young gilian). The file is third-party data:
  keep it out of the repo. Like the table, it lists each action's last frame first, so death rows get
  the same rotation.

### Local server

- Korean item, NPC and skill names in the L1J-JP database translated to English
  (`servidor_local/translate_names.py`, `name_fixes.tsv`, `name_translations.tsv`, applied from
  `corregir_db.sql`).

### Testing

- `herramientas/lanzar_prueba.sh <settings.ini> [log]` starts a test client on an invisible Xvfb display
  (lavapipe Vulkan) and prints its pid, so automated tests never cover or steal focus from a client the
  user is playing. `LIN_NO_FOCUS=1` keeps a window from taking focus when shown.
- The live-control socket no longer stops answering after a client closes its end.

### Upstream merges

- Upstream `main` is merged from time to time (last: auto-buff potions and NPC shop stack quantity,
  2026-11-15). Upstream's experimental branches (`agente/modelos-3d`, `agente/plan-portado`) are not merged.

### Known gaps

- Shop and options windows are still upstream's.
- New-monster animations from `list.spz` (encrypted) are not decoded.
- Windows port (DXIL shaders, Winsock, file mapping) not started.
- No CI yet.

## Playing locally

```sh
(cd servidor_local && nohup setsid ./arrancar.sh > servidor.log 2>&1 < /dev/null &)
./build/debug/lineage
```

Accounts are created on first login. Settings live in `~/.local/share/Lineage/LineageCliente/settings.ini`
(`juego=` must point to a full game installation, e.g. the L1Justice 3.63 client data).

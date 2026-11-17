# Lineage Client: Linux + macOS, original 2009 look (C++23 + SDL3)

A from-scratch Lineage 1 client in C++23 on SDL3 (SDL_GPU). It reads the resources of an original game
installation and speaks the L1J protocol. This repository is a **fork** of
[javiercorrea/lineage-cliente](https://github.com/javiercorrea/lineage-cliente) (GPL-3.0). The fork's goal is
to play Lineage 1 natively on **Linux** (and macOS) with a look and feel faithful to the **2009 US client**
(`S3EP1U`), which was reverse engineered to get windows, layouts, fonts and colours right.

No game assets are included: you need your own Lineage installation (the folder with `Sprite*.pak`, `Tile.pak`,
`Text.pak`, `map/`, `font/`).

| Original UI (earlier capture) | Against the l1j-en classic server (earlier capture) |
|---|---|
| ![Original panel](docs/capturas/readme_game_original.png) | ![World on l1j-en](docs/capturas/classic_mundo.png) |

## What the fork does

- **Platforms:** Linux (Ubuntu 24.04, Vulkan with SPIR-V shaders compiled from GLSL) and macOS (Metal, hand-written
  MSL twins of every shader). Verified on Intel Iris Xe (Mesa) and Apple M5 Pro. Windows is not started.
- **Original 2009 client UI is the default** (`ui_simple=0`): framed inventory, status, spellbook, options, volume,
  pet, party/pledge/buddy and private-shop windows, shop list and tooltips, F-key bar with 3 pages, HP/MP floating
  window, NPC HTML pages in the original GDI-style face, original cursors, buff icons, overhead bars, level-up
  effect, game-time orb, zone type label. Layers, offsets, colours and fonts are read from the 2009 binary.
  Upstream's simple text interface is still there (`ui_simple=1`, F2).
- **Fixed 640x480 view** scaled to any window (`vista=fija`) with filters: integer, xBR, FSR (default), CRT,
  Anime4K (`escalado=`, Shift+F5).
- **HD text** (`texto_hd=1`, default): UI and world text drawn after the scaling filter at output resolution,
  sharp at any scale with identical layout ([docs/fork/hd_text.md](docs/fork/hd_text.md)). Optional 4x HD UI
  image pack (`hd=<dir>`, not distributed).
- **Protocol profiles:** `jp363` (L1J-JP 3.63c), `tw380` (Taiwan 3.80C, with Big5 server text decoded to UTF-8)
  and `s3ep1` (US 2009).
- **Multi-folder game data:** `juego_extra=` fallback folders for sprites, tiles, maps, sounds, HTML pages and
  sprite lists missing from the main install; `lista_sprites=` / `list.spz` support for newer gfx.
- **Gameplay and social:** clan emblems (upload, cache, overhead draw), alliances (`C_Rank`), ship arrival
  (`C_Ship`), combat music, polymorph pages, pets and summons, player shops, mail, bulletin boards, auto-buffs and
  auto potion, sticky attack.
- **Three local test servers** with reproducible installers (below).
- **Test tooling:** live-control socket (debug builds), Python driver and scripted live tests, invisible test
  instances on Xvfb (`herramientas/lanzar_prueba.sh`).

Features inherited from upstream (login, character selection and creation, movement, chat, combat, inventory,
spells, NPCs and trading, party/trade/clan, houses, castles and sieges, boards and mail, shops, weather, zone
music, five interface languages, settings editor, resource explorer) work as before; their upstream-era status
write-up is kept in [docs/upstream/](docs/upstream/README.md).

## Requirements and build

Common: CMake 3.25+, Ninja, a C++23 compiler (GCC 13 / Apple Clang). SDL3 3.4.18, stb, dr_mp3 and doctest are
fetched by CMake (pinned).

**Linux** (Ubuntu 24.04 tested): `cmake ninja-build`, a Vulkan driver, and the SDL3 build dependencies
`libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libwayland-dev
libxkbcommon-dev libasound2-dev libpulse-dev libdecor-0-dev libdrm-dev libgbm-dev libudev-dev`.
`glslangValidator` is only needed to regenerate shaders (`herramientas/compilar_shaders.sh`; run it after editing
a GLSL shader, and update the MSL twin in `src/render/gpu.cpp` too).

**macOS**: Xcode Command Line Tools, `brew install cmake ninja`.

```sh
cmake --preset debug          # or release / debug-asan
cmake --build --preset debug
LINEAGE_DIR="/path/to/game" ctest --preset debug   # without LINEAGE_DIR, resource tests are skipped
```

Programs: `build/<preset>/lineage` (the game), `settings` (settings editor), `explorador` (resource explorer),
`lin_tests`. macOS packaging: `herramientas/empaquetar_macos.sh [VERSION]` builds `Lineage.app` and a `.dmg`
(not notarized: right-click, Open); the `v*` tag workflow in `.github/workflows/release.yml` is upstream's and
untested in this fork.

## Configuration

The client takes no command-line parameters. Everything comes from `settings.ini`:
`~/.local/share/Lineage/LineageCliente/settings.ini` on Linux,
`~/Library/Application Support/Lineage/LineageCliente/settings.ini` on macOS. Edit it with `build/<preset>/settings`
(close the game first) or by hand. Debug builds accept `LIN_CONFIG=/path/own.ini` to use another file.

```ini
juego=/path/to/Lineage-US-2009
juego_extra=/path/to/newer-client-data
servidor=Classic|127.0.0.1|2002|s3ep1|fx
servidor=JP|127.0.0.1|2000|jp363|
servidor=Whale|127.0.0.1|7001|tw380|
servidor_elegido=Classic
ui_simple=0
texto_hd=1
```

| Key | Meaning |
|---|---|
| `juego` | Main game folder |
| `juego_extra` | Fallback game folder, repeatable, tried in order |
| `servidor`, `servidor_elegido` | One line per server: `name\|host\|port\|profile\|last account`; profile is `jp363`, `tw380` or `s3ep1`. The chosen one is `servidor_elegido` |
| `ui_simple` | `0` (default): original 2009 UI; `1`: upstream's simple text UI (F2 toggles) |
| `texto_hd` | `1` (default): HD text pass; `0`: pixel text in the 640x480 frame |
| `fuente_original` | `1` (default): game's `eng.fnt` for ASCII; `0`: Departure Mono |
| `vista`, `escalado` | `fija`/`amplia`; `entero`/`xbr`/`fsr`/`crt`/`anime4k` |
| `hd`, `lista_sprites`, `idioma`, `logo` | HD UI pack folder, plaintext `list.spz` path, interface language, simple-login logo |

The full list (volume, effects, auto-buffs, shortcut bars...) is in [docs/fork/REFERENCE.md](docs/fork/REFERENCE.md).

## Local test servers

Development and testing happen only against local servers; never connect the client to a third-party server.
Each folder has `instalar.sh` (once; downloads a pinned server, applies our patches, creates the database),
`arrancar.sh` and `detener.sh`. Linux uses apt, macOS Homebrew (`servidor_local/comun.sh`).

| Folder | Server | Port | Profile | Database |
|---|---|---|---|---|
| `servidor_classic/` | l1j-en classic, US 2009 (**main one**) | 2002 | `s3ep1` | MariaDB 3309 |
| `servidor_local/` | L1J-JP 3.63c | 2000 | `jp363` | MariaDB 3307 |
| `servidor_380/` | L1JGO-Whale, Taiwan 3.80C/3.81C | 7001 | `tw380` | PostgreSQL 5433 |
| `servidor_classic363/` | l1j-en classic, 3.63 branch ([notes](servidor_classic363/README.md)) | 2001 | `jp363` | MariaDB 3310 |

```sh
servidor_classic/instalar.sh                      # once
(cd servidor_classic && nohup setsid ./arrancar.sh > servidor.log 2>&1 < /dev/null &)
build/debug/lineage
servidor_classic/detener.sh
```

Accounts are created on first login with the password typed that time. Server changes are kept reproducible as
`*.patch` files, idempotent `corregir_db.sql` and migration scripts (e.g. `migrar_fx.py`). Evaluations:
[l1j-en classic](docs/fork/SERVER_L1JEN_EVALUATION.md), [Whale 3.80C](docs/fork/SERVER_380_EVALUATION.md),
[3.81C data coverage](docs/fork/DATA_COVERAGE_381.md). The classic server also carries fork patches for alliances,
the Giran Bugbear Race and several l1j-en issues; the Whale server has Linux support, alliance and English item
names.

## Controls

F1 lists all keys in game. Highlights (original-client hotkeys): Ctrl+I inventory, Ctrl+A status, Ctrl+S magic,
Ctrl+C social, Ctrl+V options, Ctrl+Q restart/quit, F5-F12 spells/items, Ctrl+1..8 = F5..F12, Ctrl+R floating
HP/MP, Ctrl+T chat size, Ctrl+X attack mode, Ctrl+P screenshot. Hold the left button to walk or attack; Ctrl+click
forces an attack on NPCs/players; F2 toggles the UI mode; Shift+F5 cycles the scaling filter; Alt+Enter toggles
full screen. Complete list: [docs/fork/REFERENCE.md](docs/fork/REFERENCE.md).

## Development and testing

Rules, architecture and recipes for people and coding agents are in [AGENTS.md](AGENTS.md). Tests: doctest
(`lin_tests`). Debug builds open a live-control Unix socket driven by `herramientas/lin_ctl.py` and the scripts in
`herramientas/pruebas/`; launch test clients only with `herramientas/lanzar_prueba.sh <own.ini>` so they run on an
invisible display. Fork notes, changes and audits: [docs/fork/](docs/fork/README.md).

## Known gaps

- Shop and options windows are partly upstream's; the full list of differences from the 2009 UI is in
  [docs/fork/AUDIT_ORIGINAL_UI.md](docs/fork/AUDIT_ORIGINAL_UI.md).
- New-monster animations from the encrypted `list.spz` are not decoded.
- Windows port (DXIL shaders, Winsock, file mapping) not started; no CI.
- Interface translations (es, en, pt, de, fr) cover login, character screens and the settings editor only.
- Server text for `tw380` depends on Whale's Big5 output and the data in the game folder; see the evaluations above.

## Credits and license

Upstream client by Javier Correa ([javiercorrea/lineage-cliente](https://github.com/javiercorrea/lineage-cliente)).
Its original README and status tracker are kept in [docs/upstream/](docs/upstream/README.md). The format code derives
from [l1j-client](https://github.com/uglyoldbob/l1j-client) (GPLv3), as does `datos/sprite_table.txt`, so the client
is distributed under **GPL-3.0** ([LICENSE](LICENSE)). Servers: L1J-JP, l1j-en (classic) and L1JGO-Whale
(eddy7697 fork), each under its own license. Interface font Departure Mono: SIL OFL 1.1
(`licencias/DepartureMono-OFL.txt`).

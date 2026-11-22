# AGENTS.md (fork guide, archived)

> Kept for reference. The guide in force is [AGENTS.md](../../AGENTS.md) (Spanish): new code and comments
> are written in Spanish; only the README stays in English.

Guide for people and coding agents working on this fork. Read it before touching code.

- **What it is:** a new Lineage 1 client in C++23 + SDL3 (SDL_GPU). It reads the resources of an
  original game installation and speaks the L1J protocol through profiles `jp363` (JP 3.63c), `tw380` (TW 3.80C) and `s3ep1` (US 2009, the one the
  original UI replicates).
- **This fork:** Linux and macOS support and a look and feel faithful to the 2009 US client (original UI is the
  default, `ui_simple=0`). Fork-specific
  documentation lives in [docs/fork/](docs/fork/README.md).
- **Upstream:** [javiercorrea/lineage-cliente](https://github.com/javiercorrea/lineage-cliente), GPL-3.0.
  Its guide is [AGENTS.md](../../AGENTS.md).

## 1. Rules

1. **Language.** All new code (identifiers, comments), commit messages and docs are in **English**.
   Existing Spanish code is translated gradually, only when it is touched or refactored. Don't mass-rename.
2. **Local server only.** Test against the local servers on 127.0.0.1: `servidor_classic/` (l1j-en, US 2009, `s3ep1`,
   port 2002) is the main one; also `servidor_local/` (JP, 2000), `servidor_380/` (Whale, 7001). Never connect the client to a
   public or private third-party server.
3. **No game assets in the repo.** No `.pak`, `.idx`, maps, sprites, sounds or extracted images. They are
   read from the user's installation at runtime. Screenshots documenting work are the only exception.
4. **GPL-3.0** ([LICENSE](LICENSE)). The format code derives from l1j-client (GPLv3), and so does
   `datos/sprite_table.txt`. Keep upstream copyright notices.
5. **No allocations in the frame loop.** `tests/test_asignaciones.cpp` enforces it. Use the frame `Arena`,
   `TablaSoa` and reused buffers.
6. **Server changes must be reproducible:** steps in the server's `instalar.sh`, server files as its
   `*.patch` (`servidor_classic/patches/`), database fixes in its `corregir_db.sql` (idempotent). A change made
   only by hand doesn't exist.
7. **Every `settings.ini` option the player picks has a row in the Settings window** (`src/app/ajustes_ui.cpp`, simple UI):
   field in `Config` (`plataforma/rutas.h`, read in `rutas.cpp`), its row in the tab, its text in
   `src/i18n/catalogo.def` (five languages) and its row in the README options table.
8. **Before every commit:** zero warnings and all tests passing (section 4).

## 2. Setup

```sh
cmake --preset debug && cmake --build --preset debug     # also: release, debug-asan
servidor_classic/instalar.sh                              # once: JDK, MariaDB on 3309, database, patches
(cd servidor_classic && nohup setsid ./arrancar.sh > servidor.log 2>&1 < /dev/null &)
LINEAGE_DIR="/path/to/game/" build/debug/lin_tests        # without LINEAGE_DIR, resource tests are skipped
```

- **Linux:** SDL3 needs the X11/Wayland/ALSA/Pulse dev packages listed in the README; shaders need
  `glslangValidator` only when regenerating them (`herramientas/compilar_shaders.sh`).
- **Executable:** `build/<preset>/lineage`, no arguments. The game folder and servers come from
  `settings.ini` (`SDL_GetPrefPath`: `~/.local/share/Lineage/LineageCliente/` on Linux,
  `~/Library/Application Support/Lineage/LineageCliente/` on macOS).
- **Don't overwrite the user's settings in tests:** use `LIN_CONFIG=/path/own.ini` (debug builds only). It
  needs `juego=`; without `servidor=` it uses `127.0.0.1:2000 jp363`, so set `servidor=Name|127.0.0.1|2002|s3ep1|`
  (and `servidor_elegido=`) for the classic server. `ui_simple` defaults to 0 (original UI); `juego_extra=` adds fallback game folders.
- **Accounts** are created on first login with the password typed that first time.
- **GM access** (servidor_local; for `.item`, `.level`, `.spawn`, `.move`...): set `access_level=200` on both the account
  and its characters through the local MariaDB socket `servidor_local/datos/mysql.sock`, database
  `l1jdb_363`. Commands check the character's level; it applies on next login.
- **Servers:** `servidor_classic/` (l1j-en classic, US 2009, 2002, MariaDB 3309), `servidor_local/` (L1J-JP,
  2000, 3307), `servidor_380/` (L1JGO-Whale, TW 3.80C, 7001, PostgreSQL 5433), `servidor_classic363/` (l1j-en 3.63
  branch, 2001, 3310). Local only. Each has `instalar.sh`, `arrancar.sh`, `detener.sh`; evaluations in `docs/fork/`.

## 3. Architecture

```
socket ─► reader thread (decrypt) ─► SPSC ring ─► decodificar() ─► POD Evento (frame Arena)
                                                                     │
                     aplicar_eventos() (world: SoA table, player, chat, inventory)
                     + app handlers: eventos_npc, eventos_forma, eventos_social...
                                                                     │
screens (app/pantalla_*.cpp) ─► draw list ─► radix sort ─► 1 instanced draw call (SDL_GPU)
commands (proto/comandos.h: Escritor) ─► ring ─► writer thread (encrypt) ─► socket
```

| Module | Responsibility | Depends on |
|---|---|---|
| `src/core` | types, `Arena`, `MapaHash`, `TablaSoa`, `AnilloBytes`, allocation counting | nothing |
| `src/plataforma` | mapped files, sockets, paths and `settings.ini` | core, SDL |
| `src/proto` | encryption, opcode tables, decoder, events, commands, network threads | core, plataforma |
| `src/recursos` | `.pak/.idx`, Text.pak DES, images, tiles, sprites, maps, fonts, texts, HTML | core, plataforma |
| `src/mundo` | game state: objects (SoA), player, inventory, chat, map segments (loaded on a thread) | core, proto, recursos |
| `src/render` | SDL_GPU, texture-array atlas with LRU, high-level drawing, shaders | core, recursos, SDL |
| `src/audio` | SDL3 audio: MP3 music (dr_mp3), WAV effects, ambience | core, SDL |
| `src/ui` | immediate-mode UI (buttons, text boxes) | render |
| `src/app` | main loop, screens, windows, one `*_ui.cpp` per game feature | everything |

Facts worth knowing:

- **Opcodes:** code uses internal ids (`Srv`, `Cli` in `proto/opcodes.h`); wire numbers come from the
  per-version tables in `opcodes.cpp`.
- **Decoder:** `TABLA[]` in `decodificar.cpp` is an array of function pointers **in the same order as
  `enum class Srv`**. A `static_assert` checks the length, not the order.
- **Events:** `Evento` (`proto/eventos.h`) is POD: `e.tipo` (enum `Ev`), numbers in `e.v[]` with named
  indices, strings in `e.s[0..1]` pointing into the frame Arena (valid for one frame only).
- **App state:** everything hangs off `App` (`app/app.h`). Keep new fields grouped in a feature `struct`.
- **Platform code** stays in `plataforma/` and `render/`. Mark what's missing for another platform with
  `// MULTIPLATFORM: ...` (upstream uses `MULTIPLATAFORMA`; grep both).

### Recipe: add a server packet

1. `opcodes.h`: add the id **at the end** of `Srv`, before `CANTIDAD`; in `opcodes.cpp` its wire number in
   each version table and its name in `nombre_srv` (same order).
2. `decodificar.cpp`: write the function (one-line comment with the format) and append it **at the end of
   `TABLA[]`**, matching the enum position.
3. `eventos.h`: the `Ev` value and any `v[]` indices.
4. Apply it in `mundo.cpp` (game state) or the matching app handler.
5. Decoding test in `tests/test_proto.cpp` with real bytes (capture them with the live-control `traza on`).

### Recipe: add a client command

`Cli` in `opcodes.h` (at the end), its number in `opcodes.cpp`, an `inline` function in `comandos.h`, and
a byte test in `test_proto.cpp`. The exact format comes from the server source:
`servidor_local/l1j-jp/src/jp/l1j/server/packets/client/C_*.java` (and `server/S_*.java`). **The server
is the source of truth for the protocol.**

### Recipe: add a window or UI feature

A new file `src/app/<feature>_ui.cpp` (add it to `CMakeLists.txt`, alphabetical), its `IdVentana` in
`app.h` plus name and rect in `ventanas.cpp`, and calls to its `dibujar_*` / `procesar_*` from
`pantalla_juego.cpp`. Keep the logic in its own file; shared files only get one-line hooks.

## 4. Verification

- **Tests:** `cmake --build --preset debug && LINEAGE_DIR=... build/debug/lin_tests`. Before large
  changes, also the `debug-asan` preset.
- **UI images:** grep `docs/fork/UI_IMAGES.tsv` (id, size, description, users, binary refs) before viewing an image; when you do view one, fill/append its row.
- **Test instances must be invisible:** launch them ONLY with `herramientas/lanzar_prueba.sh <own.ini> [log]`
  (Xvfb + lavapipe; prints the pid). Never launch a visible window, never steal the user's focus, never
  `pkill` (the user may be playing): stop your instance with `kill <pid>`.
- **Live control (debug builds):** the client opens a Unix socket (`LIN_CONTROL=/path` or
  `$TMPDIR/lineage-control-<pid>.sock`) that takes one-line commands (`ayuda` lists them).
  - From Python: `herramientas/lin_ctl.py` — `c = Control.arrancar(config=...)`, then `c("login ...")`,
    `c("personaje ...")`, `c("esperar ...")`, `c("captura ...")`, `c.cerrar()`. Existing live tests are
    in `herramientas/pruebas/` (`todas.py` runs them all).
  - From a shell: `python3 herramientas/lin_ctl.py <socket> estado`.
  - Start with `mudo on` (otherwise it plays sound on the user's machine) and always finish with
    `c.cerrar()` / `salir`.
  - Use `esperar` (screen, packet, html, window, dialog, chat) instead of counting frames.
  - `release` builds must not include the control socket (`cmake/sin_control.cmake` fails the build).
- **Traces:** `traza on` (every packet, first 16 bytes), `traza-html on`, `perf on`, `paquetes`.
- **Stopping the client from a script:** `pkill -x lineage` (not `pkill -f`, which matches the shell
  running the command).

## 5. Git

- One branch per task, `fork/<topic>`. `main` must always build and pass tests.
- Commits in English, imperative, with a scope: `render: add SPIR-V shaders for Vulkan`.
- Pulling from upstream is optional and may conflict with translated code; do it deliberately.

## 6. Code style

- C++23, 4 spaces, ~120 columns. Built without exceptions or RTTI: use `std::expected` via
  `has_value()` / `*`, never `.value()`.
- **New code in English:** functions and variables `snake_case`, types `PascalCase`, enum values and
  constants `UPPER_CASE` (same shape as the existing Spanish code).
- **Comments** explain *why* and packet formats (`// S_Trade: s with whom`). Match the density of the
  surrounding code.
- **Data-oriented:** no inheritance or dynamic polymorphism on hot paths. Plain data, tables, free
  functions.
- **Dependencies:** only those in `cmake/dependencias.cmake` (SDL3, stb_image, dr_mp3, doctest, pinned).
  Don't add others without a strong reason.
- **Smaller files:** prefer splitting a growing file by responsibility over adding to `pantalla_juego.cpp`,
  `decodificar.cpp`, `control.cpp` or `app.h`.

## 7. Documents

| Document | Purpose |
|---|---|
| `README.md` | fork front page: features, build, configuration, servers, known gaps (English) |
| `docs/fork/README.md` | fork conventions, change log, index of fork docs (English) |
| `docs/fork/REFERENCE.md` | settings keys, controls, live-control commands (English) |
| `docs/fork/AUDIT_ORIGINAL_UI.md` | original-UI vs 2009 client differences |
| `docs/DESCUBRIMIENTOS.md` | what was learned about data formats, protocol and server (upstream, Spanish) |
| `docs/analisis/PROTOCOLO_RED.md`, `docs/analisis/opcodes/` | encryption, opcodes per version (upstream, Spanish) |
| `docs/upstream/README.md`, `docs/upstream/ENTREGABLES.md` | upstream's README and status tracker, history only |
| `docs/analisis/PLAN_CLIENTE.md` | upstream plan (Spanish) |

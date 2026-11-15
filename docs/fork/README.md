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
  Verified on Intel Iris Xe (Mesa ANV): `GPU: vulkan (SPIR-V)`.
- **Case-insensitive paths:** `ruta_real()` in `src/plataforma/archivo_mapeado.*` resolves game files
  ignoring case on Linux (cached directory listings, mutex-protected). No-op on macOS/Windows.
- **Local server scripts:** `servidor_local/*.sh` work on Linux via `servidor_local/comun.sh`
  (apt instead of brew). MariaDB runs as the user on port 3307; game server on port 2000.
- **Build fixes:** missing `<string>` includes; four pre-existing warnings fixed.
- **Linux packages for SDL3:** see the "Requirements (Linux)" section in the main README.

### Original-client look (`ui_simple=0`)

- **Font:** `fuente_original=1` (default) draws printable ASCII with the game's `font/eng.fnt` instead
  of Departure Mono; other characters fall back to the TTF.
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

### Known gaps

- Zone type label ("Normal zone" / "Safety zone" / "Combat zone"): the flag has not been found in the
  map data yet.
- Windows port (DXIL shaders, Winsock, file mapping) not started.
- No CI yet.

## Playing locally

```sh
(cd servidor_local && nohup setsid ./arrancar.sh > servidor.log 2>&1 < /dev/null &)
./build/debug/lineage
```

Accounts are created on first login. Settings live in `~/.local/share/Lineage/LineageCliente/settings.ini`
(`juego=` must point to a full game installation, e.g. the L1Justice 3.63 client data).

# Client reference: languages, settings, controls, live control

Reference for the fork's client. Carried over from the upstream README and corrected for the fork (defaults
differ: the original 2009 UI is the default, see `ui_simple`). Build and run instructions are in the
[root README](../../README.md). Spanish-named keys and commands (`servidor`, `juego`, `esperar`...) are
upstream's and kept as they are.

### Languages

The interface is available in **Spanish, English, Portuguese, German and French**. Choose it in the
settings editor (Interface tab) or with `idioma=es|en|pt|de|fr` in `settings.ini`. With no value it follows
the system language, and falls back to English if that is not one of the five. The game applies it at
startup; the editor switches as soon as you pick it.

| Español | Português |
|---|---|
| ![Login in Spanish](../capturas/readme_login_es.png) | ![Login in Portuguese](../capturas/readme_login_pt.png) |

| Deutsch | Français |
|---|---|
| ![Login in German](../capturas/readme_login_de.png) | ![Login in French](../capturas/readme_login_fr.png) |

- **What is translated so far:** login, character selection and creation, and the settings editor. The
  in-game windows are not migrated yet and keep their current text. Text sent by the server is shown as
  it arrives.
- **Translations** live in `src/i18n/catalogo.def`, one line per text with its five languages. The tests
  check that every text has all five and the same `%d` / `%s` in each.
- **Font:** interface text uses [Departure Mono](https://departuremono.com/) (SIL OFL 1.1, see
  `licencias/DepartureMono-OFL.txt`), because the game's bitmap font only has ASCII and Korean. It is
  embedded in the binary and drawn at 11 px, where its pixel grid lands exactly on the screen, without
  smoothing. Text in CP949 (Korean from the server) still uses the original font.
  `tools/generar_fuente_ui.py` regenerates the embedded data (needs `fonttools`).

### Settings reference

All of these can be changed from the editor; most can also be changed in the game.

| Key | Meaning |
|---|---|
| `juego` | Game folder (a full installation: `Sprite*.pak`, `Tile.pak`, `Text.pak`, `map/`, `font/`) |
| `juego_extra` | Extra game folder, repeatable (one line each, tried in order). Anything missing from `juego` (sprites, tiles, loose `sprite/` and `map/` files, HTML pages) is looked up there; text tables stay primary-only, so a newer or different client's data can fill gaps. Case-insensitive on Linux. Each folder's plaintext sprite list (`list.spz`, `spritelist/list.spz`, or a top-level list `.txt`) is merged too |
| `servidor`, `servidor_elegido` | Server list and the selected one |
| `idioma` | Interface language; empty = the system's |
| `ventana` | Window size (`WIDTHxHEIGHT`), saved on close |
| `zoom` | Game scale; 0 = automatic |
| `ui_simple` | `0` (default when absent): the original 2009 UI (framed windows, panel images); `1`: upstream's simple text interface. F2 toggles |
| `vista` | `fija` (default): fixed 640×480 view, as in the original, scaled to the window; `amplia`: the view grows with the window and the zoom |
| `escalado` | Filter used to scale the fixed view: `entero` (nearest), `xbr`, `fsr` (default), `crt`, `anime4k` (Shift+F5) |
| `texto_hd` | `1` (default): UI text is drawn after the scaling filter at the output resolution (sharp at any scale, same glyphs and layout; hd_text.md); `0` pixel text drawn into the 640×480 frame. Live: `texto-hd on|off` |
| `fuente_original` | `1` (default) ASCII text with the game's `font/eng.fnt`; `0` Departure Mono |
| `lista_sprites` | Path of a plaintext `list.spz` (your own copy; never shipped). It adds the gfx missing from `datos/sprite_table.txt`. Empty = `list.spz` or `spritelist/list.spz` in the game folder, if present |
| `hd` | Folder with an upscaled UI pack (`img_<id>.png`, `tbt_<id>.png`, 4× the original size); empty = off |
| `camara_a_saltos` | `1` camera follows the walking frames, like the original (F4) |
| `sombra_personajes` | Darken characters standing in a tree's shadow (Shift+F4) |
| `pantalla_completa` | Game in full screen (1) or windowed without border (0, default). Alt+Enter toggles it |
| `mostrar_fps` | Show frames per second and milliseconds per frame in the top right corner (Shift+F3) |
| `efecto_luz` | Dark nights and dungeons, with the light of characters, NPCs and items |
| `efecto_agua_tinte`, `efecto_agua_ola` | Underwater blue tint and wave |
| `efecto_infravision` | Green elf vision and dim dark elf vision |
| `opacidad_ventanas` | Window background opacity (0–255) |
| `volumen_musica`, `volumen_efectos` | Volume (0–100) |
| `chat_con_enter` | `1` Enter opens the chat line, `0` always typing |
| `barra_personaje` | HP/MP bar over your own character |
| `texto_dano` | Floating damage and "Miss" over your character |
| `globos_comic` | `1` speech and shouts in a rounded outline over the head (also in Options: "Speech balloons"), `0` plain text |
| `compensar_ataque` | Show your swing at once, without waiting for the server's echo |
| `logo` | PNG for the simple login screen, see below |
| `atajos`, `barra` | F5–F12 spells and shortcut bar, per character (bar pages separated by `/`) |
| `ventanas` | Saved window positions |
| `paginas_barra` | Shortcut bar pages, 1–9 (3 by default) |
| `autobuffs` | Auto-buffs per character: on/off and the six spells (`Name:1\|43,42,0,0,0,0`) |
| `ms_mantener_tecla` | Hold time before 1–0 casts a heal or buff on yourself, 150–1000 ms (300 by default) |
| `usar_autobuffs`, `usar_autopocion` | `0` hides and turns off auto-buffs or auto potion completely (both `1` by default) |
| `autopocion` | Auto potion per character: on/off and HP threshold in % (`Name:1,60`; off with 60 % by default) |

**Custom login logo (simple interface):** `logo=my_logo.png`, a PNG with transparency of up to 2048×2048,
as an absolute path or relative to the `settings.ini` folder. It is shrunk to fit 560×190 (best to supply
it at that size, because it is scaled without smoothing). Without a logo, or if it can't be read, the
title is shown as text.

## Controls

**F1** shows all the keys in the game. Window hotkeys follow the original client: Ctrl+I inventory, Ctrl+A status,
Ctrl+S magic, Ctrl+C social, Ctrl+V options, Ctrl+Q restart/quit, Ctrl+H help (simple UI: also Alt+I/C/S), Ctrl+P screenshot,
Ctrl+1..8 = F5..F12 (shortcut boxes), Alt+F10 = F10.
Original UI also: Ctrl+T chat size (4 steps), Ctrl+O chat colour palette, Ctrl+X attack mode (same as /pk), GM-only Ctrl+Y / Ctrl+Z (pick a target, send opcode 83; Z repeats).

### Interface and view

- **F2:** switches between the original 2009 panel (default) and upstream's simple interface (a text strip with chat, input and ASCII bars,
  and a text status window). It also applies to login, character
  selection and creation: with the simple interface they are text boxes (a list of all characters with
  their data; deleting asks for confirmation).
- **F3:** fps, CPU per frame (average, worst frame and decoding), allocations of the worst frame,
  instances, draw calls, objects and position.
- **F4:** in the original UI, pick up an item lying next to you (as the original client). In the simple UI:
  camera, "smooth" (default: follows the continuous position, with the character centered) or
  "stepped" (like the original: follows the walking frames, a ¼ cell at a time).
- **Shift+F3:** frames per second and milliseconds per frame in the top right corner, on or off.
- **Alt+Enter:** full screen or windowed. Windowed has no system border: the bar on top drags the window, and its
  buttons minimize and close it (on macOS they are on the left, like the system's). Full screen has no bar.
- **Shift+F5:** cycles the scaling filter of the fixed view (integer, xBR, FSR, CRT, Anime4K); a chat line
  names the new one.
- **Shift+F4:** darken characters standing in a tree's shadow (default) or not.
- **Shift+F12:** saves `captura.bmp`.
- **Shift+click on the world:** turns the character that way without walking (to face someone, e.g. to trade).
- **Window size:** with `vista=fija` the 640×480 view is scaled to any window size (drag the window border to
  resize it) and the zoom keys don't apply.
- **Ctrl/⌘ + and Ctrl/⌘ −:** zoom in and out (integer scale; zooming out shows more map).
- **Ctrl/⌘ 0:** automatic zoom, the largest that fits 640×480.
- **Game view (`vista=amplia`):** adapts to the window. Enlarging it shows more map, with the panel centered at the
  bottom. The login and character screens stay at 640×480.
- **Windows:** drag the title bar to move them (the position is saved). They look for a free spot when
  opened; if they overlap, a click brings one forward and only that one gets the mouse. They also open with
  buttons: those of the original panel, or the text ones to the right of the chat (simple interface).
- **Options** has the interface, camera, shade, FPS counter, full screen, light and water effects, elf vision, window opacity, zoom, music and effects volume, and
  a "Reset" for window positions.
- **Welcome text and server news at login:** wrapped to fit; the mouse wheel or Page Up / Page Down scrolls them.
- **Esc:** at login, quits.

### Moving

- **Walk:** hold the left button to walk toward the pointer, or use the arrow keys (combine them for diagonals).
- **Doors:** click a door to open or close it (the character walks to it). Closed doors block the way.
- **Portals:** to enter a dungeon or climb stairs, walk toward the entrance with the arrows, or click right on it.
- **Roofs and trees:** when you enter a house or pass behind a tree, whatever covers the character becomes
  translucent and its silhouette shows.
- **Change character:** Exit > "Character select".

### Combat

- **Attack:** click a monster and hold. The character walks to it and attacks at the pace of its attack
  animation, even if the camera moves and the pointer is no longer over it (the target is fixed when
  pressing). Releasing stops. The target's name turns yellow.
- **A** (chat closed): keeps attacking the target, or the monster under the pointer, until it dies, you walk
  with the arrows, or you click elsewhere.
- **Force attack:** Ctrl+click (or ⌘+click) attacks an NPC, another player or another player's pet or summon.
  A plain click only attacks monsters; a polymorphed player still counts as a player.
- **PK mode:** `/pk` in the chat toggles it (a red "PK" indicator at the top right): clicking another
  player attacks without Ctrl. This is client-side; the server decides whether the attack counts (it does no
  damage in safe zones).
- **Monsters without a sprite:** those missing from this installation show as a magenta block and are
  attacked like any other. Players with an unknown gfx show the same block but are not attacked by a plain click.
- **On death:** a notice appears with a **Restart** button, which sends `C_Restart`.
- **Status effects:** poisoned (green), frozen (gray), blind (black screen except your own character), and a
  notice at the top if paralyzed, asleep, stunned or bound (you can't move or act).

### Items

- **Ctrl+I (or Alt+I):** opens and closes the inventory (the original client's hotkeys are Ctrl+letter).
  - Shows the original icons (`N.tbt`), the name on hover, equipped items with an "E", and the weight.
  - Double-click uses an item: equips or removes weapons and armor, drinks potions.
  - Drag an item out of the window to drop it on the ground.
  - **Item details:** hovering shows the name and, if identified, its attributes (damage or defense,
    material, weight, enchantment, classes that use it, attribute bonuses, MR, resistances...). Otherwise it
    says "Unidentified".
  - **Scrolls used on another item** (identify, enchant weapon or armor): double-click the scroll, then click the
    item; Esc cancels.
  - **Trash can** (bottom right of the inventory): drag an item onto it to destroy it, for what can't be dropped
    or sold. It always asks first: how many from a stack, or to confirm a single one. The server rejects equipped
    and undeletable items.
- **Items on the ground:** click one to pick it up; if it is far, the character walks to it. The name shows on hover.
- **Giving items:** drag an item from the inventory onto an NPC or pet; from a stack, it asks how many.
- **1..0 (or Alt+1..Alt+0), simple UI only:** shortcut bar (bottom center). Drag a spell (from Alt+S) or an item (from Alt+I) onto
  a slot. Clicking it, or its number or Alt+number, uses it; right-click empties it; dragging between slots moves
  it. Middle-click on a slot selects it (gold frame), and the middle button on a character or monster uses the
  selected spell there; held, it repeats each time the wait ends (for an item, every half second). With nothing under the pointer it works like the slot's key (a targeted spell waits for the click). A dot next to the page number means the selected slot is on another page. Spell slots
  (bar and F5–F12) show the wait until the next spell: a shrinking shade and the seconds left. Saved per character.
  The bar has pages (3 by default; "Hotbar pages" in Options): the page selector on its right (up arrow, number, down arrow), or the mouse
  wheel over the bar, changes the visible one, and keys, clicks and drags act on it. The selected slot keeps
  working from any page. Holding 1–0 on a targeted spell that is not an attack (heals, buffs) casts it on
  yourself after a moment (300 ms; "Hold 1-0 to self-cast" in Options) and repeats while held; a tap still waits
  for the target click.
- **Auto-buffs:** six green slots right of the bar. Drag a buff there (from Alt+S or from a bar slot; attacks
  and spells without a duration are refused), reorder by dragging, right-click empties. With "Auto" (above them)
  on, each one is cast again when its effect ends (or has less than 2 s left), left to right and one at a time,
  only while you are not walking, attacking or casting something else. Off by default; saved per character.
  It sends the same packet as casting by hand.
- **Auto potion:** per character, in Options ("Auto potion", "Auto potion when") or in the settings tool. While HP
  is below the threshold (60 % by default) it drinks a healing potion as soon as the previous one's delay is over:
  condensed first, then ancient, then normal, and within each the colour that fits what is missing (clear for a
  big loss, orange for a medium one, red for a small one). "Pot<60%" next to HP shows it is on (red: no potions).
  The potion button right of the auto-buffs changes it quickly: Off -> 30 -> 60 -> 80 -> 90 % -> Off.

### Magic

- **Ctrl+S (or Alt+S):** spell window (click casts; with the pointer over a spell, F5–F12 assigns it to that key).
- **F5–F12:** cast the assigned spell (saved per character). A spell with a target waits for a click on the target;
  right-click cancels.
- **Learning magic:** at the magic teacher (Gereng on Talking Island...), "Learn magic" opens the list: click
  chooses and OK buys. Spellbooks are used with a double-click inside a temple.

### NPCs and trading

- **Clicking an NPC:** walks to it and talks (its page appears at the top left; Esc closes it). In a shop or warehouse:
  click adds 1, Shift+click adds 10, right-click subtracts; OK confirms.
- **Repairing weapons:** at a blacksmith, the repair link shows damaged weapons with their cost; a click repairs.
- **Pets and summons:** their list is to the left of the shortcut bar, with their health. Clicking a name opens its
  orders menu (attack, defend, stay...); "Bag" shows its inventory, where a click uses or equips the item. With
  "Attack", the next click picks its target.
- **Player shops** (in the market rooms): clicking a seller (seated, with the title above) shows what they sell and
  buy; clicking a line asks how many. `/shop Title` sets up your own: click an inventory item to set its price;
  "Open" opens it and `/shop` again closes it.
- **Town bulletin boards:** click the board: topics, reading, "Write" posts (300 adena), and authors can delete theirs.
- **Mail:** `/mail` (Inbox, Clan and Saved; "Write" to a character for 50 adena, or to the whole clan).

### Chat and social

- **Chat:**
  - **Enter opens the chat line** (the bottom says "Enter: chat"). Type, and Enter sends and closes it; Esc clears
    and closes it. While closed, keys are actions (1–0 use the bar). In Options, "Chat: Always typing" goes back
    to always typing, with the bar on Alt+number;
  - mouse wheel over the panel, or Page Up / Page Down, for older messages;
  - prefixes: `!` shout, `&` global, `$` trade, `@` clan, `#` party, `%` alliance, `*` chat group, `"name text` whisper.
- **Ctrl+A (or Alt+C):** opens and closes the status window (left of the inventory if both are open).
- **Social** (button, or the one on the original panel): party, trade and clan, with buttons and help. Server
  questions (invitations, trade or clan requests) appear with **Yes/No** (Enter/Esc).
- **Trade:** stand next to the other player, facing them. Drag an item from the inventory to the window (or double-click
  with the trade open), and OK when both agree.
- **Party:** at mid-height on the left, the list of other members with their health; the leader has a "*".
- **Names:** a player's color follows alignment gradually: white at 0, through light blue to blue (16000 or more),
  through pink to red (−16000 or less). The title goes above, with its `\fX` colors, and the clan emblem to the left.
  Pets and summons show their owner above the name ("Andromeda's").
- **Player shops:** the title goes over the seller with no background, and the cursor is a coin. Shop prices use a dot
  every three digits (10.000).
- **Bookmarks:** `/bookmark name` saves the place (if the map allows it) and `/delbookmark name` deletes it. With
  bookmarks, the teleport scroll and Teleport ask for the destination.
- **Chat commands:** anything that isn't a command goes to the chat.

| Area | Commands |
|---|---|
| Party | `/invite Name`, `/party`, `/leave`, `/kick Name` |
| Trade | `/trade` |
| Clan | `/clan Name` (create; prince with 30000 adena), `/join` (facing a prince), `/pledge`, `/leaveclan`, `/banclan Name`, `/emblem` (uploads a sample emblem) |
| Players | `/title [Name] Title`, `/who [Name]`, `/exclude Name` (block or unblock; no name: the list) |
| Friends | `/buddy`, `/addbuddy Name`, `/delbuddy Name` |
| Chat group | `/chatparty`, `/chatoutparty`, `/chatbanish Name` |

### Sound

Zone music, animation effects and ambience. Music and effects volume are in **Options**.

## Live control (tests and screenshots; development only)

The `debug` and `debug-asan` builds (CMake option `LIN_CONTROL`) open a Unix domain socket and take one-line
commands. The `release` build doesn't include it: a CMake step (`cmake/sin_control.cmake`) fails if it slips in.

- **Socket:** `$LIN_CONTROL`, or `$TMPDIR/lineage-control-<pid>.sock`. The path is printed at startup.
- **Responses:** each ends with `ok` or `error <reason>`.
- **Waits:** commands that wait for something (`login`, `personaje`, `esperar`, `captura`) respond when it
  happens or the time limit passes, without blocking the game.
- **Encoding:** text goes in CP949, like the game's.
- **`LIN_CONFIG=/tmp/x.ini`** (development only): uses another `settings.ini`, so as not to overwrite the user's and
  to run several clients at once.

| Commands | What they do |
|---|---|
| `ayuda` | Lists the commands |
| `estado`, `objetos [radio]`, `inventario`, `html`, `ventanas`, `grupo`, `chat [n]`, `paquetes [n]` | Queries: screen, view and character (and sieges), nearby objects (id, name, gfx, cell, sprite state, file alias, table row, and screen position), items, the NPC page with its links, open windows (and where the movable ones are), the party, the chat, the latest packets |
| `login <account> <password> [server]`, `personaje <name>`, `crear <name> <class>` | Enter (they wait for the character list or for being in the game) |
| `decir "text"` | As if typed in the chat (`/` commands and the GM's `.` included) |
| `hablar <obj>`, `atacar <obj>`, `parar`, `clic-objeto <obj> [der]` | `<obj>`: id, `#gfx` or part of the name (the nearest one) |
| `accion <link>`, `responder si\|no\|"text"`, `cantidad <n>` | NPC page, server questions, quantity dialog |
| `usar <item> [sobre <item>]`, `soltar`, `dar <item> <obj>`, `borrar`, `lanzar <spell> [obj]` | Items and spells |
| `girar <0-7>`, `ofrecer <item>`, `aceptar-intercambio`, `cancelar-intercambio` | Trade between two clients |
| `abrir\|cerrar inventario\|estado\|hechizos\|opciones\|social\|salida\|teclas` | Windows |
| `clic <x> <y> [der\|medio]`, `arrastrar x1 y1 x2 y2`, `mantener x y frames`, `tecla <name>` (`ctrl+`, `alt+`, `shift+`), `texto "..."`, `ctrl on\|off`, `shift on\|off` | Raw input (logical coordinates) |
| `esperar pantalla=\|paquete=\|html=\|ventana=\|dialogo=\|chat=\|frames=\|ms= [plazo=ms]` | Wait for something instead of counting frames (`paquete=` and `chat=` also count what arrived since the previous command) |
| `captura <path.bmp>`, `mudo on\|off`, `traza on\|off`, `traza-html on\|off`, `perf on\|off`, `depuracion on\|off`, `salir` | Screenshots and debugging |
| `forzar-hora <seconds 0-86399>\|off` | Fix the time of day for light and ambient sound, for tests |
| `rueda <x> <y> <steps>`, `barra-pagina [n]` | Mouse wheel; visible shortcut bar page (without `n`, reports it) |
| `tecla-mantener <1-0> <ms>` | Hold a number key (also `alt+1`...) that long, for the hold-to-self-cast |

`herramientas/lin_ctl.py` uses it from the terminal (`lin_ctl.py <socket> estado`, or interactively) or as a module
for writing tests.

**Live tests** (`herramientas/pruebas/`):

- one per topic: login, moving, chat and status, combat and spells, inventory, equipment, NPC, bulletin board and
  mail, auctions, social (two clients);
- **those that modify and restart the local server:** houses, castles and war. They restore it themselves with
  `herramientas/pruebas/servidor.py`.

```sh
herramientas/pruebas/todas.py /tmp/test.ini /tmp/screenshots              # those that leave no changes
herramientas/pruebas/todas.py /tmp/test.ini /tmp/screenshots --servidor   # all of them
herramientas/pruebas/npc.py /tmp/test.ini /tmp/screenshots                # just one
```

The test `settings.ini` needs `juego=`. Each test puts characters back where they were and closes its clients even
if it fails. For example, the beginning of `subastas.py`:

```python
c = Control.arrancar(config="/tmp/test.ini")
c("mudo on"); c("login prueba2 prueba2"); c("personaje Charla")
c("hablar Auction"); c("esperar ventana=subastas")
c("captura /tmp/auctions.bmp"); c.cerrar()
```

## Resource explorer

```sh
build/debug/explorador --juego="$LINEAGE_DIR" --gfx=0 --accion=3   # ←/→ gfx, ↑/↓ action
build/debug/explorador --juego="$LINEAGE_DIR" --png=814            # a single image (--img=N)
```

Shows a gfx in the 8 directions over reference diamonds. It was used to calibrate the sprite anchor.


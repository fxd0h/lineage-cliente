# L1JGO-Whale (TW 3.80C/3.81C) evaluation

Phase 1: install and probe. Nothing migrated; `servidor_local` (jp363, port 2000, MariaDB 3307) untouched.

## Verdict

- **Target:** the README says 3.80C, but `internal/net/packet/opcodes.go` is headed "Server Opcodes (V381)"
  and the commit log mentions yiwei (a 3.81C emulator) alignment. It is a 3.80C/3.81C TW server. Client
  opcodes work unchanged with the fork's `tw380` table.
- **Pin:** `9a19405` (2026-05-27, v0.3.32 merge) is *ahead of* the default-branch HEAD `ac4b670`
  (v0.3.31+). Keep the pin; there is nothing newer.
- **Linux:** `servidor_380/instalar.sh` now works (apt `golang-go` + `postgresql-16`, user cluster on 5433,
  Go toolchain `go1.23.4` auto-downloaded because go.mod wants 1.23 and Ubuntu ships 1.22; server binds
  127.0.0.1:7001). `sudo` was passwordless here; on a box that prompts, run
  `sudo apt-get install -y golang-go postgresql-16` first and re-run the script.
- **Result:** playable on the fork. Login, character creation, world, NPC dialogs, combat, polymorph,
  warehouse and GM commands work. The server is a far larger, more modern feature set than servidor_local,
  but it is Traditional Chinese (server text in Big5), and the fork cannot decode that yet.

## Test setup

`LIN_CONFIG` ini with `servidor=W380|127.0.0.1|7001|tw380|fx`, `juego=` either L1Justice or the TW 3.81C
client, run through `herramientas/lin_ctl.py` (Xvfb). Accounts are auto-created on first login. There is
no access-level check: `.level`, `.item`, `.gold`, `.move`, `.spawn`, `.poly`, `.class`, `.allskill`... work
for everyone (`.help` lists ~50).

## Feature matrix (live-tested unless noted)

| Area | servidor_local (363c) | Whale 3.80C | Fork on Whale |
|---|---|---|---|
| Login, auto account, char list | yes | yes | OK |
| Char create (knight) | yes | yes | OK (new char starts on map 2005 tutorial area) |
| World render (map 2005, 4) | yes | 1039 map files | OK with L1Justice data, sprites `ok` |
| NPC names | desc-e `$N` | server sends `$N` ids | OK with Justice desc-e; see 3.81C below |
| NPC dialogs (client html) | yes | yes | OK: Pledge Manager, teleporter, alchemist, blacksmith, storage, Dio show English pages |
| Warehouse | yes | yes | OK: retrieve list opens (SHOWRETRIEVELIST) |
| Shop | yes | yes | not reached (merchants gave no page when not adjacent; Dio gave `sell` page); retest |
| Combat vs mobs, death/drops | yes | yes | OK (frogs killed, "xp/drop" messages) |
| Inventory, +19 items via `.item` | yes | yes | OK |
| Polymorph (`.poly`) | yes | yes | OK (POLY packet) |
| Spells (`.allskill`, spell window) | yes | yes | window opens; casting not exercised |
| Stat raise page (level > 50 bonus points) | n/a | server forces `RaiseAttr` page when talking to NPCs with unspent points and PARALYSIS | works, blocks other dialogs until spent |
| Clan create / emblem | yes | yes | NOT working: `create` link on `bpledge2` gets only a SHOWHTML back, no name prompt; emblem packet 64 (CHARSYNACK) unmapped in fork |
| Alliance (`handler/alliance.go`), war, castles, pets | partial | implemented (castle, war, dolls, pets, auction, mail, board, buddies, cooking, dungeons, 3.80 Lua scripting) | not live-tested (needs 2 clients / siege setup); static opcode check below |

## Protocol gaps in the fork (`TW380_SRV`)

Server opcodes sent but not mapped in `src/proto/opcodes.cpp`: `INITPACKET` 150 (handled as handshake),
`CHARSYNACK` 64 (equipment slots / emblem / char reset sync), `CHANGENAME` 46, `REDMESSAGE` 105,
`CLANNAME` 72, `CLANATTENTION` 200, `CLANMATCHING` 0. Of these, CHARSYNACK and CLANNAME matter for clan
and emblem work. `paquetes` shows `?` for them during login. Client opcodes were not diffed (different table
shape); everything exercised worked. `LOGINTOSERVEROK` is not mapped for tw380 (already noted in the UI audit).

## Text and language

- Server sends **Big5** (system messages, GM output, item names such as adena and "+19 ..."). The fork only
  has CP949 (`recursos/cp949.cpp`), so this renders as mojibake (e.g. adena `�蕩�`). Need a Big5 decoder and
  a font with Big5 glyphs (not in the Korean-only fonts shipped with L1Justice).
- NPC names and dialogs are **not** server text: names come as `$N` ids resolved by `desc-e.tbl`, dialogs
  are client `Text.pak` html. With L1Justice both are English, so no `translate_names.py` equivalent is
  needed for them. The server's own Chinese lines (`.help`, system messages, NPC shouts) are the remaining
  text; `data/yaml` also stores Chinese item/NPC names used by `.item <name>` and some Lua dialogs.
- Items' names in inventory come from the server's item description ids; check each against the Justice
  `desc-e.tbl` (not systematically done).

## TW 3.81C client as data (`/home/fx/work/l1/ref/tw2013/.../休閒天堂381/`)

- The loader opens it unchanged: `Sprite*.pak/idx`, `Tile`, encrypted `Text.idx/pak`, `map/` all read; login,
  enter-world and render of the same scene work (screenshot compared with Justice: same terrain, same
  sprites for the player, Cuse, Karen, the pink Beginner helper).
- Text: it **does** have both `desc-e.tbl` / `string-e.tbl` / `spelldesc-e.tbl` and the `-c` versions
  (Chinese). But its `desc-e.tbl` is 114 KB against 365 KB in Justice, so many `$N` names stay raw
  (`$8446`, `$5085` in the screenshot) and some plain names appear without their title (`Cuse`, `Karen`).
  English coverage is therefore worse than Justice; the Chinese `desc-c.tbl` (340 KB) has them all, but
  the fork has no language switch for it (`idioma=` exists in settings; not tried with `c`) and no Big5 font.
- Same server content renders with both: with 3.81C the scene was identical for the starter area; sprite list
  format (`List.spr.bin` vs Justice `spritelist/list.spz` + `sprite_table.txt`) was not needed for this scene.
  Newer gfx ids (post-3.63) are more likely to be in 3.81C; not measured systematically.
- Recommendation: keep Justice as `juego=` for English; use the 3.81C only as a fallback source of newer
  sprites/maps, or add a text-source override (desc-e from Justice, sprites from 3.81C).

## Data gaps

- Server `map/` has 1039 maps; map 2005 and 4 render with Justice. Other maps and polymorph/gfx ids not
  swept; do a sweep of `data/yaml/spawn_list.yaml`/`npc_list.yaml` gfx ids against the sprite table.
- `data/yaml/maps_missing_sql.txt` lists maps the server itself lacks.

## Remaining work before migration

1. Big5 decoding + font (blocker for readable system messages and Chinese item names).
2. Map `CHARSYNACK`, `CLANNAME`, `CLANATTENTION`, `REDMESSAGE`, `CHANGENAME`; fix clan creation prompt
   (inspect `handler/clan.go` + `S_HYPERTEXT` text input) and emblem upload (`Cli::EMBLEM` 18).
3. Live-test shop buy/sell, spell casting, alliance, war/castle, pets (needs second client or GM `.spawn`).
4. Sweep gfx ids used by the server against Justice and 3.81C.

## Migration plan for `fx` (knight 92, +19 Demon/DK/Kurtz/Iron sets, Dragon Slayer...)

Do not touch the old DB. On the new server, as a fresh character `fx`: `.class 1`, `.level 92` (spend the
stat points through the `RaiseAttr` page or `.stat`), `.allskill`, `.gold <n>`, then `.item <id|name> <count>
+<enchant>` per piece, one line each for the +19 sets (`.item 20011 1 19` style; ids from the old
`etcitem/weapon/armor` tables, names in `data/yaml/armor_list.yaml`/`weapon_list.yaml`). Generate the list
from the old DB read-only (SELECT on `character_items`, no writes) into a script the control socket
(`decir ".item ..."`) replays. Verify each id exists in the Whale yaml first; missing ones need entries.

## Operation

`servidor_380/arrancar.sh` / `detener.sh`, log `servidor_380/servidor.log`, Postgres on 127.0.0.1:5433,
DB `l1jgo`. Neither the clone (`servidor_380/l1jgo/`) nor `datos/` is committed.

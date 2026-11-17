# l1j-en/classic (US 2009, s3ep1) evaluation

`servidor_classic/` is the user's new server. `servidor_local` (jp363, 2000, MariaDB 3307) and Whale (7001, PostgreSQL 5433) were not touched.

## Verdict and pin

- **Branch:** `master` (s3ep1, the final 2009 US client the fork's original UI replicates, profile `s3ep1`). It works; `servidor_classic363/` (branch 363, jp363 protocol) was not needed.
- **Pin:** `103e6f6` = upstream `master` HEAD (checked with `git ls-remote`, Nov 2026; `instalar.sh` already pinned it).
- **Linux:** `servidor_classic/instalar.sh` / `arrancar.sh` / `arrancar_db.sh` / `detener.sh` use `../servidor_local/comun.sh` (apt on Linux, Homebrew on macOS). Own MariaDB datadir `servidor_classic/datos`, port **3309** (socket `datos/my.sock`), db `l1jdb`, user l1j/l1j. Game port **2002** (2000 old server, 2001 classic363, 7001 Whale). Packages: `ant` and `default-jdk-headless` were already present, so nothing was installed with sudo. `detener.sh` kills only `-Dl1j.instancia=s3ep1`.
- **Reproducible changes:** `servidor_classic/patches/*.patch` (`git format-patch`, applied by `instalar.sh`, skipped when already applied, then `ant`), `corregir_db.sql` (idempotent, applied on every install), `migrar_fx.py` (character).

## Feature matrix (live-tested with test instances unless noted)

| Area | L1J-JP (servidor_local) | Whale 3.80C | l1j-en classic + fork |
|---|---|---|---|
| Login / auto account, char list | yes | yes | OK (`traza on`/`paquetes` showed no unknown opcode) |
| World, sprites (Giran, Hidden Dock, ships) | yes | yes | OK, `sprite=ok` |
| NPC dialogs, teleporters | yes | yes | OK. Wizard Merlin (Giran) -> "prison" = Giran Dungeon map 53 with the 3 h timer (`GiranPrisonController`) OK. Decavia hunting grounds were broken (issue 47, fixed) |
| Shops | yes | yes | buy list opens (SHOWSHOPBUYLIST); purchase click not driven |
| Warehouse (personal / pledge) | yes | yes | dialog and retrieve OK; pledge storage not driven |
| Combat, drops | yes | yes | OK (frog killed, "Frog gave you Meat") |
| Spells, GM buffs | yes | yes | `.allbuff` exists (`L1AllBuff`, 29 buffs): effects/animations (SKILLSOUNDGFX, SKILLICONSHIELD, messages) play. Needs access 200 |
| Polymorph | yes | yes | `.poly` OK (POLY packet) |
| Clan create / emblem | yes | yes | Pledge Administrator `create` asks the name (string 83) and the server answers "Only a Prince or Princess..." for the knight (correct). Prince flow and emblem not driven |
| Alliance | partial | patch | **Missing**: `C_Rank` only handles ranks 1-3, no alliance code in l1j-en, and the 2009 US client has no alliance UI. Not ported (decision: servidor_380/alliance.patch is for the 3.80 client) |
| War, castles, pets, trade, mail | yes | partial | code present (`C_War`, `C_Trade*`, `C_Mail`); not live-driven in this pass |
| Boards (Giran message board, Oren ranking board) | yes | n/a | window `tablon` opens (issue 16 not reproduced); ranking content not checked |
| Boat rides (ship maps 5, 6, 83, 84, 446, 447) | yes | n/a | **Fork gap**: the fork never sends `C_Ship` (opcode 33) at arrival, so the ride never ends (the server side is consistent). See issue 5 |

## Findings

- GM characters log in invisible and invulnerable (`commands.run_on_login`). NPCs and mobs ignore invisible GMs, so NPCs (cow, Daisy, hens) stand still and no MOVEOBJECT arrives. This is the cause of "wandering NPCs do not move" (issue 6) in GM tests. `corregir_db.sql` turns both off at login; use `.invis` / `.invul` when wanted.
- `.tele x y map` is the move command (`.move` does not exist). `accion "teleport giranD"` needs the quotes in `lin_ctl` (a space splits arguments).
- NPC names in the client: Decavia is "Dekabia", Wizard Merlin "Teleporter^Wizard Merlin" (class prefix before `^`).
- No decoder issue found in the paths exercised; the pledge rank nibble and shop/NPC formats match the `s3ep1` profile.

## l1j-en open issues (55 listed; client-download/support 54, 64, 68, 69 ignored)

| Issue | Verdict | Commit / note |
|---|---|---|
| 47 Decavia teleport | fixed | 1e0d541, `0047` (also Sharial 80146; Sharial order/prices assumed same as Decavia) |
| 43 Lasta / Resistance scrolls | partly fixed | 1e0d541 `corregir_db.sql` (40083, 40853 landing); per-floor Lasta escape landing needs work |
| 3 buy limit 1000 | fixed | 1e0d541, `0003` (long arithmetic, cap 2e9, non-stackable 1000) |
| 42 spell on dead target | fixed | 8a5d31a, `0042` ("Spell failed", no MP spent) |
| 39 Firewall from safety zone | fixed | 8a5d31a, `0039` (untested live) |
| 18 friends list DC | fixed (unverified) | 8a5d31a, `0018` clips the lists to 250 characters |
| 35 skill reuse timers | partly fixed | 8a5d31a: Fire/Lightning Storm 2.5 s; 380 skills still 0, needs audit |
| 6 wandering NPCs | not reproducible | cause was the GM auto-invisibility (see Findings); fixed for GMs by `corregir_db.sql` |
| 16 message / ranking boards | not reproducible | board windows open |
| 15 BP storage | not reproducible | upstream cannot reproduce either; "retrieve without title" is intended |
| 4 mob dies at screen edge | not reproducible | upstream cannot reproduce it either |
| 5 Pirate Island boat | needs work (fork) | server data and `C_Ship` are consistent; the fork never sends C_Ship, needs the original arrival trigger (REA) |
| 20 Ship Grave Yard / Deep Sea connections | needs work | no detail in the issue |
| 37 invisible mobs | needs work | not reproducible quickly |
| 19 G-haste spellbook name | won't fix | name comes from client string `$1651` |
| 26 food icon / description | needs work | client string / icon ids |
| 46 dice dagger hit animation | won't fix (server) | `L1Attack` turns 0 damage into a miss; not a fork bug |
| 53 death freeze (363 branch) | n/a | not the master/s3ep1 branch |
| 11 fid weapon sprites | won't fix | missing sprites in the client data |
| 2 spr_action merge, 8 L1Attack merge, 7 spawnlist update, 9 GM shop prices, 10 kennel, 12 gereng, 13 stray NPC, 14 HV/SI text, 17 lights, 21-25, 27-34, 36, 38, 40, 41, 44, 45, 48-52, 1 | needs work | balance / data / upstream housekeeping, not triaged individually |

## Character migration

`servidor_classic/migrar_fx.py` copies knight `fx` (objid 268479040) from `servidor_local/datos/mysql.sock` (SELECT only) into `l1jdb`: account fx/fx (old SHA1 hash; l1j-en rehashes on first login), access level GameMaster (200), level 92, stats, 14 skills, position Giran, and 78 of 82 items with enchant, equipped, identified, attribute enchant. Item ids matched by id+name, else by name (Greater Haste Potion 41477 -> 40018, Greater Mana Potion 41481 -> 90008). Skipped (no US equivalent): Create Zombie spellbook (47041), Greater Bravery potion (41480), Royal Valor Necklace (21248), Royal Earring (21249). Run `python3 -I servidor_classic/migrar_fx.py` with the DB up and the server stopped.

## settings.ini for the user's client

```
servidor=Classic|127.0.0.1|2002|s3ep1|fx
servidor_elegido=Classic
ui_simple=0
fuente_original=1
texto_hd=1
juego=/home/fx/work/l1/juego/Lineage Justice/
lista_sprites=/home/fx/work/l1/juego/spritelist/list.spz
```

Start with `(cd servidor_classic && nohup setsid ./arrancar.sh > server.log 2>&1 < /dev/null &)`, stop with `servidor_classic/detener.sh`. Log in as fx / fx.

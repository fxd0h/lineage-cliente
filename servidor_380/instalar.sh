#!/bin/bash
# Instala un servidor 3.80C local para probar el cliente: L1JGO-Whale (Go + PostgreSQL), macOS con
# Homebrew o Linux (apt).
#
# - Baja el servidor (eddy7697/L1JGO-Whale, commit fijo; fork of r0ptik's) en servidor_380/l1jgo y lo compila
# - PostgreSQL con su propio directorio de datos (servidor_380/datos), en el puerto 5433 para no
#   chocar con otra instalación; base l1jgo, usuario postgres sin clave (solo local)
# - La configuración se genera en servidor_380/l1jgo/local.toml (la del repo no se toca)
#
set -euo pipefail
cd "$(dirname "$0")"
REPO=https://github.com/eddy7697/L1JGO-Whale.git
COMMIT=f95af7d

# MULTIPLATFORM: Homebrew on macOS, apt on Linux (system go 1.22 downloads toolchain go1.23.4).
if [ "$(uname)" = Darwin ]; then
    for p in go postgresql@16; do
        brew list --versions "$p" >/dev/null || brew install "$p"
    done
    GO="$(brew --prefix go)/bin/go"
    PG="$(brew --prefix postgresql@16)/bin"
else
    if [ ! -x /usr/lib/postgresql/16/bin/initdb ] || ! command -v go >/dev/null; then
        # No system cluster: we run our own on 5433.
        sudo mkdir -p /etc/postgresql-common
        echo 'create_main_cluster = false' | sudo tee /etc/postgresql-common/createcluster.conf >/dev/null
        sudo apt-get install -y golang-go postgresql-16
    fi
    GO=go
    PG=/usr/lib/postgresql/16/bin
fi

if [ ! -d l1jgo ]; then
    git clone -q "$REPO" l1jgo
fi
# Pin the commit (also moves an older clone, e.g. r0ptik's 9a19405, to the current fork): drop our
# local edits (patches and translated yaml are reapplied below) and check out the pin.
git -C l1jgo remote set-url origin "$REPO"
git -C l1jgo cat-file -e "$COMMIT^{commit}" 2>/dev/null || git -C l1jgo fetch -q origin
git -C l1jgo checkout -q -f "$COMMIT"
# Server patches (idempotent): emblem_id (clan emblem id in the char packs) and alliance (C_Rank invite/withdraw, persisted in character_alliance).  allbuff_effects (GM buffs play their cast animation; .allbuff drops Enchant Venom).
for p in emblem_id.patch alliance.patch allbuff_effects.patch; do
    git -C l1jgo apply --check -R "../$p" 2>/dev/null || git -C l1jgo apply "../$p"
done
(cd l1jgo && GOTOOLCHAIN=go1.23.4 GOMAXPROCS=4 nice -n 19 "$GO" build -o bin/l1jgo ./cmd/l1jgo)

if [ ! -d datos ]; then
    "$PG/initdb" -D datos -U postgres --auth=trust -E UTF8 --locale=C >/dev/null
fi
./arrancar_db.sh
"$PG/psql" -h 127.0.0.1 -p 5433 -U postgres -tc "SELECT 1 FROM pg_database WHERE datname = 'l1jgo'" | grep -q 1 ||
    "$PG/createdb" -h 127.0.0.1 -p 5433 -U postgres l1jgo

# Configuración local: la base de arriba (las migraciones las corre el servidor al arrancar), sin
# el "#npc:gfx" de depuración en los nombres de los NPC y con el registro en "info".
sed -e 's#^dsn = .*#dsn = "postgres://postgres@127.0.0.1:5433/l1jgo?sslmode=disable"#' \
    -e 's#^show_npc_id = true#show_npc_id = false#' \
    -e 's#^bind_address = .*#bind_address = "127.0.0.1:7001"#' \
    -e 's#^level = "debug"#level = "info"#' \
    l1jgo/config/server.toml > l1jgo/local.toml
# English names: restore the pristine yaml, then replace Chinese item names by client-side $N ids.
git -C l1jgo checkout -q -- data/yaml
python3 -I translate_names.py apply l1jgo/data/yaml names_en.tsv
# Prune what no game data source can show and what the official 3.81C does not have (see prune_data.py,
# prune_ids.tsv; regenerate the list with prune_data.py generate when the data folders change).
python3 -I prune_data.py apply l1jgo/data/yaml prune_ids.tsv
# Teleports Whale lacks (see teleports_extra.tsv).
python3 -I add_teleports.py l1jgo/data/yaml/teleport_list.yaml teleports_extra.tsv
echo "Listo: ./arrancar.sh (puerto 7001)"

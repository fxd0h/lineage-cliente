#!/bin/bash
# Arranca la MariaDB de servidor_classic/datos (puerto 3309) si no está corriendo.
set -euo pipefail
cd "$(dirname "$0")"
source ../servidor_local/comun.sh
MARIA="$(dirname "$(maria_bin mariadb)")"
"$MARIA/mariadb-admin" --socket="$PWD/datos/my.sock" -uroot ping >/dev/null 2>&1 && exit 0
"$(maria_bin mariadbd-safe)" --datadir="$PWD/datos" --port=3309 --bind-address=127.0.0.1 \
    --socket="$PWD/datos/my.sock" --pid-file="$PWD/datos/m.pid" >/dev/null 2>&1 &
for i in $(seq 1 30); do
    "$MARIA/mariadb-admin" --socket="$PWD/datos/my.sock" -uroot ping >/dev/null 2>&1 && exit 0
    sleep 1
done
echo "la base no arrancó" >&2; exit 1

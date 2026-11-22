#!/bin/bash
# Arranca MariaDB con el directorio de datos local (puerto 3307), si no está corriendo.
set -euo pipefail
cd "$(dirname "$0")"
AQUI="$(pwd)"
source ./comun.sh
ADMIN="$(maria_bin mariadb-admin)"
if "$ADMIN" --socket="$AQUI/datos/mysql.sock" -uroot ping >/dev/null 2>&1; then
    exit 0
fi
"$(maria_bin mariadbd-safe)" --datadir="$AQUI/datos" --port=3307 --bind-address=127.0.0.1 \
    --socket="$AQUI/datos/mysql.sock" --pid-file="$AQUI/datos/mariadb.pid" >/dev/null 2>&1 &
for _ in $(seq 1 30); do
    "$ADMIN" --socket="$AQUI/datos/mysql.sock" -uroot ping >/dev/null 2>&1 && exit 0
    sleep 1
done
echo "MariaDB no arrancó; ver $AQUI/datos/*.err"; exit 1

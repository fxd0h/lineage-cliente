#!/bin/bash
# Detiene el servidor l1j-en/classic y su MariaDB.
cd "$(dirname "$0")"
source ../servidor_local/comun.sh
pkill -f "l1j.instancia=s3ep1" || true
"$(maria_bin mariadb-admin)" --socket="$PWD/datos/my.sock" -uroot shutdown 2>/dev/null || true

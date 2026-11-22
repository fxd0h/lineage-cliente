#!/bin/bash
# Arranca MariaDB y el servidor l1j-en/classic (3.0 English / US 2009) en el puerto 2002. Ctrl+C sale.
set -euo pipefail
cd "$(dirname "$0")"
source ../servidor_local/comun.sh
./arrancar_db.sh
cd l1jen
exec "$(java_bin)" -Dl1j.instancia=s3ep1 -Djava.util.logging.config.file=config/log.properties \
    -Xms1024m -Xmx1024m -cp "l1jen.jar:lib/*" l1j.server.Server

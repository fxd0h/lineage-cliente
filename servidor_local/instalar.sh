#!/bin/bash
# Instala un servidor L1J JP 3.63c local para probar el cliente (macOS con Homebrew o Linux con apt).
#
# - Baja el servidor (WantedGaming/L1J-JP_3.63c, commit 403e35a) en servidor_local/l1j-jp
# - Usa MariaDB con su propio directorio de datos en servidor_local/datos (no toca otras bases)
# - Crea la base l1jdb_363 y el usuario l1j/l1j, y deja el servidor apuntando a ellos
#
# MULTIPLATAFORMA: macOS usa Homebrew, Linux usa apt (default-jre-headless, mariadb-server);
# en Windows usar los instaladores de OpenJDK y MariaDB y los .bat del propio servidor.
set -euo pipefail
cd "$(dirname "$0")"
AQUI="$(pwd)"
REPO=https://github.com/WantedGaming/L1J-JP_3.63c.git
COMMIT=403e35a

source ./comun.sh
instalar_paquetes
MARIADB="$(maria_bin mariadb)"

if [ ! -d l1j-jp ]; then
    git clone "$REPO" l1j-jp
    git -C l1j-jp checkout -q "$COMMIT"
fi
# Teletransportes que no andan con esta instalación del cliente (ver los comentarios del parche).
git -C l1j-jp apply --check ../teleporter.patch 2>/dev/null && git -C l1j-jp apply ../teleporter.patch
# Test pooled DB connections before use (MariaDB closes them after 8 h idle).
git -C l1j-jp apply --check ../db_pool.patch 2>/dev/null && git -C l1j-jp apply ../db_pool.patch
if git -C l1j-jp diff --quiet -- src/jp/l1j/server/utils/L1DatabaseFactory.java; then :; else
    # The jar is prebuilt: compile the patched class into it (idempotent).
    tmp=$(mktemp -d)
    (cd l1j-jp && javac -nowarn -encoding UTF-8 --release 8 -cp "l1jserver.jar:lib/*" -d "$tmp" \
        src/jp/l1j/server/utils/L1DatabaseFactory.java && jar uf l1jserver.jar -C "$tmp" jp/l1j/server/utils)
    rm -rf "$tmp"
fi
# Map items (40373..40390): send S_UseMap instead of "cannot use" (the packet class existed but was never sent).
git -C l1j-jp apply --check ../map_use.patch 2>/dev/null && git -C l1j-jp apply ../map_use.patch
if git -C l1j-jp diff --quiet -- src/jp/l1j/server/packets/client/C_UseItem.java; then :; else
    tmp=$(mktemp -d)
    (cd l1j-jp && javac -nowarn -encoding UTF-8 --release 8 -cp "l1jserver.jar:lib/*" -d "$tmp" \
        src/jp/l1j/server/packets/client/C_UseItem.java && jar uf l1jserver.jar -C "$tmp" jp/l1j/server/packets/client)
    rm -rf "$tmp"
fi

# MariaDB en un directorio de datos propio, en el puerto 3307 para no chocar con otra instalación
if [ ! -d datos ]; then
    "$(maria_bin mariadb-install-db)" --datadir="$AQUI/datos" --auth-root-authentication-method=normal >/dev/null
fi
./arrancar_db.sh
"$MARIADB" --socket="$AQUI/datos/mysql.sock" -uroot <<'SQL'
CREATE DATABASE IF NOT EXISTS l1jdb_363 DEFAULT CHARACTER SET utf8;
CREATE USER IF NOT EXISTS 'l1j'@'localhost' IDENTIFIED BY 'l1j';
CREATE USER IF NOT EXISTS 'l1j'@'127.0.0.1' IDENTIFIED BY 'l1j';
GRANT ALL ON l1jdb_363.* TO 'l1j'@'localhost';
GRANT ALL ON l1jdb_363.* TO 'l1j'@'127.0.0.1';
SQL
if [ "$("$MARIADB" --socket="$AQUI/datos/mysql.sock" -uroot -N -e 'SELECT COUNT(*) FROM information_schema.tables WHERE table_schema="l1jdb_363"')" = "0" ]; then
    echo "Importando la base de datos (tarda un poco)..."
    "$MARIADB" --socket="$AQUI/datos/mysql.sock" -uroot l1jdb_363 < l1j-jp/db/l1jdb_363.sql
fi
# The dump split CSV fields with escaped commas (armor sets, 15 NPCs); rebuild those rows.
python3 reparar_csv.py | "$MARIADB" --socket="$AQUI/datos/mysql.sock" -uroot l1jdb_363
"$MARIADB" --socket="$AQUI/datos/mysql.sock" -uroot l1jdb_363 < corregir_db.sql

# El conector MySQL 5.1.25 que trae el servidor falla (NullPointerException) con MariaDB moderno.
# Se reemplaza por la última 5.1 (5.1.49) con el mismo nombre de archivo, que es el que pide el
# Class-Path del manifiesto de l1jserver.jar. El original queda como .orig.
LIB=l1j-jp/lib/mysql-connector-java-5.1.25-bin.jar
if [ ! -f "$LIB.orig" ]; then
    mv "$LIB" "$LIB.orig"
    curl -sSfL -o "$LIB" https://repo1.maven.org/maven2/mysql/mysql-connector-java/5.1.49/mysql-connector-java-5.1.49.jar
fi

# JAXB (javax.xml.bind) se quitó de Java 11; el servidor lo usa para cargar sus XML.
# arrancar.sh agrega lib_extra/ al classpath.
mkdir -p l1j-jp/lib_extra
for u in jakarta/xml/bind/jakarta.xml.bind-api/2.3.3/jakarta.xml.bind-api-2.3.3.jar \
         org/glassfish/jaxb/jaxb-runtime/2.3.9/jaxb-runtime-2.3.9.jar \
         org/glassfish/jaxb/txw2/2.3.9/txw2-2.3.9.jar \
         com/sun/istack/istack-commons-runtime/3.0.12/istack-commons-runtime-3.0.12.jar \
         com/sun/activation/jakarta.activation/1.2.2/jakarta.activation-1.2.2.jar; do
    [ -f "l1j-jp/lib_extra/$(basename "$u")" ] || curl -sSfL -o "l1j-jp/lib_extra/$(basename "$u")" "https://repo1.maven.org/maven2/$u"
done

# Configuración del servidor: base en el puerto 3307, usuario l1j y textos en EUC-KR (cliente coreano)
CFG=l1j-jp/config/server.properties.xml
sed_i \
    -e 's|jdbc:mysql://localhost/l1jdb_363?|jdbc:mysql://127.0.0.1:3307/l1jdb_363?useSSL=false\&amp;zeroDateTimeBehavior=round\&amp;|' \
    -e 's|<entry key="Login">root</entry>|<entry key="Login">l1j</entry>|' \
    -e 's|<entry key="Password"></entry>|<entry key="Password">l1j</entry>|' \
    "$CFG"
grep -q '127.0.0.1:3307' "$CFG" || { echo "No se pudo ajustar $CFG"; exit 1; }
# Like the original game: monster drops fall to the ground instead of going straight to the inventory.
sed_i -e 's|<entry key="AutoLoot">[0-9]</entry>|<entry key="AutoLoot">0</entry>|' l1j-jp/config/altsettings.properties.xml
echo "Listo. Arrancar con ./arrancar.sh (las cuentas se crean solas al primer login)."

#!/usr/bin/env bash
# Arma los paquetes de Linux (PLAN_PORTADO.md, Fase 4):
# - Lineage-<versión>-linux-<arq>.AppImage: un solo archivo ejecutable, con .desktop e ícono (linuxdeploy);
# - Lineage-<versión>-linux-<arq>.tar.gz: lineage, datos/, LICENSE y LEEME.txt (para sistemas sin FUSE).
# Uso: herramientas/empaquetar_linux.sh [versión]   (la arquitectura es la de la máquina: x64 o arm64)
# Deja los dos en dist/ e imprime sus rutas en las dos últimas líneas. Lo usa .github/workflows/release.yml.
#
# - Compila con el preset release (sin el control en vivo) en build/empaquetado-<arq>.
# - SDL3 va enlazado estático; las bibliotecas del sistema (X11/Wayland, Vulkan, ALSA/PulseAudio) las
#   abre SDL en ejecución. El binario pide la glibc de la máquina donde se compiló (en CI, Ubuntu 24.04).
# - datos/ va junto al ejecutable: SDL_GetBasePath() devuelve su carpeta.
# - No incluye recursos del juego: el jugador elige su carpeta.
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:-0.0.0}"
case "$(uname -m)" in
    x86_64) ARQ=x64 MAQ=x86_64 ;;
    aarch64 | arm64) ARQ=arm64 MAQ=aarch64 ;;
    *) echo "arquitectura sin paquete: $(uname -m)" >&2; exit 1 ;;
esac

BUILD="build/empaquetado-$ARQ"
cmake --preset release -B "$BUILD" "-DLIN_VERSION=$VERSION" >&2
cmake --build "$BUILD" --target lineage >&2

NOMBRE="Lineage-$VERSION-linux-$ARQ"
RAIZ="dist/$NOMBRE"
rm -rf "$RAIZ"
mkdir -p "$RAIZ"
cp "$BUILD/lineage" "$RAIZ/"
cp -R datos "$RAIZ/datos"
cp LICENSE "$RAIZ/LICENSE.txt"
cp herramientas/linux/LEEME.txt "$RAIZ/"
tar -C dist -czf "dist/$NOMBRE.tar.gz" "$NOMBRE"
rm -rf "$RAIZ"

# AppImage: el ejecutable en usr/bin y datos/ a su lado (SDL_GetBasePath() es la carpeta del ejecutable).
# linuxdeploy copia las bibliotecas compartidas que no son del sistema base; glibc, Vulkan, X11/Wayland y el
# audio se usan los de la máquina. Sin FUSE (contenedores, CI) se corre extrayéndose a sí mismo.
# shortcut: linuxdeploy de la release "continuous", no una versión fija; fijarla si un cambio de linuxdeploy rompe el paquete.
LD="build/linuxdeploy-$MAQ.AppImage"
if [[ ! -x "$LD" ]]; then
    curl -fsSL -o "$LD" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$MAQ.AppImage"
    chmod +x "$LD"
fi
APPDIR="$BUILD/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
cp -R datos "$APPDIR/usr/bin/datos"
APPIMAGE="dist/$NOMBRE.AppImage"
rm -f "$APPIMAGE"
APPIMAGE_EXTRACT_AND_RUN=1 ARCH="$MAQ" LINUXDEPLOY_OUTPUT_VERSION="$VERSION" OUTPUT="$APPIMAGE" "$LD" \
    --appdir "$APPDIR" --executable "$BUILD/lineage" \
    --desktop-file herramientas/linux/lineage.desktop --icon-file herramientas/linux/lineage.png \
    --output appimage >&2
[[ -f "$APPIMAGE" ]] || { echo "linuxdeploy no dejó $APPIMAGE" >&2; exit 1; }

echo "dist/$NOMBRE.tar.gz"
echo "$APPIMAGE"

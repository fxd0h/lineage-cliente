#!/usr/bin/env bash
# Arma el .tar.gz de Linux: lineage, datos/, LICENSE y LEEME.txt.
# Uso: herramientas/empaquetar_linux.sh [versión]   (la arquitectura es la de la máquina: x64 o arm64)
# Deja dist/Lineage-<versión>-linux-<arq>.tar.gz e imprime su ruta en la última línea. Lo usa
# .github/workflows/release.yml.
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
    x86_64) ARQ=x64 ;;
    aarch64 | arm64) ARQ=arm64 ;;
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
echo "dist/$NOMBRE.tar.gz"

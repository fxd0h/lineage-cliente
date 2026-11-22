#!/bin/sh
# Descarga, compila e instala el cliente de Lineage en Linux, macOS o BSD (en el usuario, sin root).
#
#   curl -fsSL https://raw.githubusercontent.com/javiercorrea/lineage-cliente/latest/instalar.sh | sh
#
# Volver a correrlo actualiza a la última versión. Variables opcionales:
#   LIN_REF=v0.2.0        versión (tag o rama) a instalar; por omisión `latest`, la última publicada
#   LIN_SRC=<carpeta>     dónde queda el código; por omisión ~/.local/src/lineage-cliente
#   LIN_PREFIX=<carpeta>  dónde se instala en Linux y BSD; por omisión ~/.local
#   LIN_SI=1              instalar sin preguntar las dependencias que falten (con sudo, o doas en BSD)
#
# Instala:
#   Linux y BSD: <prefix>/share/lineage-cliente/ (el ejecutable y datos/), <prefix>/bin/lineage, y el acceso
#                directo y el ícono del escritorio en <prefix>/share/applications e icons.
#   macOS:       ~/Applications/Lineage.app.
# El juego (Sprite.pak, map/, ...) NO se descarga: es de su dueño. El cliente pide la carpeta de una instalación
# original del juego la primera vez que se abre.
set -eu

REPO="https://github.com/javiercorrea/lineage-cliente.git"
REF="${LIN_REF:-latest}"
SRC="${LIN_SRC:-$HOME/.local/src/lineage-cliente}"
PREFIX="${LIN_PREFIX:-$HOME/.local}"

dice() { printf '\033[1m==> %s\033[0m\n' "$*"; }
falla() { printf 'error: %s\n' "$*" >&2; exit 1; }
tiene() { command -v "$1" >/dev/null 2>&1; }

# ---- Dependencias ------------------------------------------------------------------------------------
SO="$(uname -s)"
PAQUETES="" INSTALAR=""
case "$SO" in
    Linux)
        if tiene apt-get; then
            INSTALAR="apt-get install -y"
            PAQUETES="build-essential cmake ninja-build git pkg-config libx11-dev libxext-dev libxrandr-dev
                libxcursor-dev libxi-dev libxss-dev libxfixes-dev libxtst-dev libwayland-dev libxkbcommon-dev
                libasound2-dev libpulse-dev libvulkan-dev"
        elif tiene dnf; then
            INSTALAR="dnf install -y"
            PAQUETES="gcc-c++ cmake ninja-build git pkgconf libX11-devel libXext-devel libXrandr-devel
                libXcursor-devel libXi-devel libXScrnSaver-devel libXfixes-devel libXtst-devel wayland-devel
                libxkbcommon-devel alsa-lib-devel pulseaudio-libs-devel vulkan-loader-devel"
        elif tiene pacman; then
            INSTALAR="pacman -S --needed --noconfirm"
            PAQUETES="base-devel cmake ninja git libx11 libxext libxrandr libxcursor libxi libxss libxfixes
                libxtst wayland libxkbcommon alsa-lib libpulse vulkan-icd-loader vulkan-headers"
        elif tiene zypper; then
            INSTALAR="zypper install -y"
            PAQUETES="gcc-c++ cmake ninja git pkgconf libX11-devel libXext-devel libXrandr-devel libXcursor-devel
                libXi-devel libXss-devel libXfixes-devel libXtst-devel wayland-devel libxkbcommon-devel
                alsa-devel libpulse-devel vulkan-devel"
        fi ;;
    FreeBSD | DragonFly)
        INSTALAR="pkg install -y"
        PAQUETES="cmake ninja git pkgconf libX11 libXext libXrandr libXcursor libXi libXScrnSaver libXfixes
            libXtst wayland libxkbcommon vulkan-loader vulkan-headers" ;;
    OpenBSD) INSTALAR="pkg_add"; PAQUETES="cmake ninja git" ;;
    NetBSD) INSTALAR="pkgin -y install"; PAQUETES="cmake ninja-build git pkgconf" ;;
    Darwin)
        xcode-select -p >/dev/null 2>&1 || { xcode-select --install || true; falla "instalar las Command Line Tools de Xcode (se abrió el instalador) y volver a correr esto"; }
        if tiene brew; then INSTALAR="brew install"; PAQUETES="cmake ninja git"; fi ;;
    *) falla "sistema no soportado: $SO (Linux, macOS o BSD)" ;;
esac

# Sin cmake, ninja o git (o, en Linux y BSD, sin los encabezados de X11/Wayland), se instalan los paquetes del sistema.
falta_algo() {
    for p in cmake ninja git; do tiene "$p" || tiene "$p-build" || return 0; done
    [ "$SO" = Darwin ] && return 1
    [ -e /usr/include/X11/Xlib.h ] || [ -e /usr/local/include/X11/Xlib.h ] || [ -e /usr/X11R7/include/X11/Xlib.h ] || return 0
    return 1
}
if falta_algo; then
    [ -n "$INSTALAR" ] || falla "faltan herramientas (cmake 3.25+, ninja, git, un compilador de C++23 y los encabezados de X11/Wayland, ALSA y Vulkan); instalarlas a mano"
    raiz=""
    if [ "$SO" != Darwin ] && [ "$(id -u)" != 0 ]; then
        if tiene sudo; then raiz=sudo; elif tiene doas; then raiz=doas; else falla "hace falta sudo o doas para instalar: $INSTALAR $PAQUETES"; fi
    fi
    dice "Faltan dependencias. Se va a correr:"
    echo "  $raiz $INSTALAR $(echo $PAQUETES)"
    if [ "${LIN_SI:-}" != 1 ]; then
        # Con `curl | sh` la entrada estándar es el script: la respuesta se lee de la terminal.
        [ -r /dev/tty ] || falla "sin terminal para preguntar: correr con LIN_SI=1, o instalar esos paquetes a mano"
        printf '¿Instalar? [s/N] '
        read -r resp </dev/tty
        case "$resp" in s* | S* | y* | Y*) ;; *) falla "cancelado" ;; esac
    fi
    [ "$INSTALAR" != "apt-get install -y" ] || $raiz apt-get update -q
    # shellcheck disable=SC2086
    $raiz $INSTALAR $PAQUETES
fi
tiene cmake || falla "no está cmake"
tiene ninja || tiene ninja-build || falla "no está ninja"

# ---- Código -----------------------------------------------------------------------------------------
if [ -d "$SRC/.git" ]; then
    dice "Actualizando el código en $SRC ($REF)"
    git -C "$SRC" fetch --depth 1 --force origin "+refs/tags/$REF:refs/tags/$REF" 2>/dev/null ||
        git -C "$SRC" fetch --depth 1 --force origin "$REF"
    git -C "$SRC" checkout --force -q FETCH_HEAD
else
    dice "Descargando el código ($REF) en $SRC"
    mkdir -p "$(dirname "$SRC")"
    git clone --depth 1 --branch "$REF" "$REPO" "$SRC"
fi
cd "$SRC"
VERSION="$(sed -n 's/^## v\([0-9][0-9.]*\).*/\1/p' CHANGES.md | head -1)"
[ -n "$VERSION" ] || VERSION="0.0.0"

# ---- Compilación e instalación ---------------------------------------------------------------------
if [ "$SO" = Darwin ]; then
    dice "Compilando Lineage $VERSION (macOS)"
    LIN_SOLO_APP=1 LIN_ARQS="$(uname -m)" herramientas/empaquetar_macos.sh "$VERSION" >/dev/null
    mkdir -p "$HOME/Applications"
    rm -rf "$HOME/Applications/Lineage.app"
    cp -R dist/dmg/Lineage.app "$HOME/Applications/"
    rm -rf dist/dmg
    dice "Listo: ~/Applications/Lineage.app (abrirlo desde el Finder o con: open ~/Applications/Lineage.app)"
    exit 0
fi

dice "Compilando Lineage $VERSION ($SO)"
BUILD=build/instalado
cmake --preset release -B "$BUILD" "-DLIN_VERSION=$VERSION" >/dev/null
cmake --build "$BUILD" --target lineage
rm -rf "$PREFIX/share/lineage-cliente"  # sin restos de la versión anterior
cmake --install "$BUILD" --prefix "$PREFIX" >/dev/null
# El menú del escritorio no siempre tiene <prefix>/bin en el PATH: el acceso directo va con la ruta completa.
sed "s|^Exec=.*|Exec=$PREFIX/share/lineage-cliente/lineage|" herramientas/linux/lineage.desktop > "$PREFIX/share/applications/lineage.desktop"

dice "Listo: $PREFIX/bin/lineage (también en el menú de aplicaciones como Lineage)"
case ":$PATH:" in *":$PREFIX/bin:"*) ;; *) echo "Nota: $PREFIX/bin no está en el PATH; agregarlo o abrir $PREFIX/share/lineage-cliente/lineage." ;; esac

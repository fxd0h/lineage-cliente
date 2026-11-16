#!/usr/bin/env bash
# Compila los shaders GLSL de src/render/shaders a SPIR-V y genera src/render/shaders_spirv.h
# (bytes embebidos). Requiere glslangValidator. Volver a correrlo al cambiar un .vert/.frag.
set -euo pipefail
raiz="$(cd "$(dirname "$0")/.." && pwd)"
dir="$raiz/src/render/shaders"
out="$raiz/src/render/shaders_spirv.h"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
{
  echo "// Generado por herramientas/compilar_shaders.sh a partir de src/render/shaders/. No editar."
  echo "#pragma once"
  echo "#include <cstddef>"
  echo "namespace lin::spirv {"
  for f in sprite.vert sprite.frag texto.frag brillo.vert brillo.frag mundo.vert mundo.frag escalado_xbr.frag escalado_fsr.frag escalado_crt.frag anime4k_c1.frag anime4k_c2.frag anime4k_c3.frag anime4k_c4.frag anime4k_d2s.frag anime4k_resample.frag; do
    nombre="${f/./_}"
    glslangValidator -V --target-env vulkan1.0 -o "$tmp/$f.spv" "$dir/$f" >/dev/null
    echo "alignas(4) inline constexpr unsigned char ${nombre}[] = {"
    od -An -v -tx1 "$tmp/$f.spv" | sed 's/ *\([0-9a-f][0-9a-f]\)/0x\1,/g'
    echo "};"
  done
  echo "}  // namespace lin::spirv"
} > "$out"
echo "Generado $out"

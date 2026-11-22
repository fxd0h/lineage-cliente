#!/usr/bin/env python3
"""Genera las pasadas de convolución de Anime4K (GLSL para Vulkan + gemelos MSL) desde el hook oficial de mpv.

Origen: https://github.com/bloc97/Anime4K  glsl/Upscale/Anime4K_Upscale_CNN_x2_S.glsl  (MIT License,
Copyright (c) 2019-2021 bloc97). Los pesos se copian tal cual; solo se reemplaza la plomería de lectura de texturas.
Uso: generar_anime4k.py <ruta a Anime4K_Upscale_CNN_x2_S.glsl>
Salidas: src/render/shaders/anime4k_c{1..4}.frag, anime4k_c{1..4}.hlsl (Windows/D3D12) y src/render/shaders_msl_anime4k.h (parte de convolución).
"""
import re
import sys
from pathlib import Path

LICENSE = """// Anime4K v3.2 Upscale CNN x2 (S), https://github.com/bloc97/Anime4K (MIT License)
// Copyright (c) 2019-2021 bloc97. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
// following conditions: the above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
// KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// Generado por herramientas/generar_anime4k.py: no editar.
"""

root = Path(__file__).resolve().parent.parent
src = Path(sys.argv[1]).read_text()
passes = src.split("//!DESC")[1:]
convs = []
for p in passes:
    if "Depth-to-Space" in p.splitlines()[0]:
        continue
    defs = re.findall(r"^#define go_\d.*$", p, re.M)
    body = p[p.index("vec4 hook() {") + len("vec4 hook() {"):p.index("    return result;")]
    # Las funciones de lectura de mpv (MAIN_texOff, conv2d_*_texOff) pasan a ser tex_off(); mismos pesos, mismos desplazamientos.
    defs = [re.sub(r"\w+_texOff", "tex_off", d) for d in defs]
    convs.append((defs, body))
assert len(convs) == 4, len(convs)

glsl_head = """#version 450
%s// Pasada de convolución %d de 4. u.xy = tamaño de la grilla lógica, u.z = escala entera de la imagen muestreada (1 para los
// intermedios; destino_ guarda cada píxel lógico como un bloque de z*z, se lee en su centro).
// Gemelo MSL: fs_anime4k_c%d en shaders_msl_anime4k.h.
layout(set = 2, binding = 0) uniform sampler2D src;
layout(std140, set = 3, binding = 0) uniform Params { vec4 u; };
layout(location = 0) out vec4 salida;
ivec2 P;
vec4 tex_off(vec2 o) {
    ivec2 q = clamp(P + ivec2(o), ivec2(0), ivec2(u.xy) - 1);
    return texelFetch(src, q * int(u.z) + int(u.z) / 2, 0);
}
%s
void main() {
    P = ivec2(gl_FragCoord.xy);
    vec4 result;
"""
msl_parts = ["R\"MSL(\n#include <metal_stdlib>\nusing namespace metal;\n" + LICENSE + "struct A4kSalida {\n    float4 pos [[position]];\n    float2 uv;\n};\n"]
for i, (defs, body) in enumerate(convs, 1):
    d = "\n".join(defs)
    code = glsl_head % (LICENSE, i, i, d) + "    " + body.strip().replace("vec4 result = ", "result = ", 1) + \
        "\n    salida = result;\n}\n"
    (root / "src/render/shaders" / f"anime4k_c{i}.frag").write_text(code)

    # HLSL (Windows/D3D12): en GLSL mat4(a0..a15) * v es por columnas, o sea result_k = sum_j a[4j+k] v_j. En HLSL
    # float4x4(a0..a15) es por filas y mul(v, M) da result_k = sum_i v_i M[i][k] = sum_i v_i a[4i+k]: lo mismo.
    hbody = re.sub(r"(result\s*\+?=\s*)mat4\((.*?)\)\s*\*\s*(go_\d\(.*?\));", r"\1mul(\3, float4x4(\2));", body.strip())
    hbody = hbody.replace("vec4 result = ", "float4 result = ", 1).replace("result = vec4(", "result = float4(").replace("+= vec4(", "+= float4(")
    assert "mat4" not in hbody and "vec" not in hbody, hbody[:300]
    hdefs = "\n".join(x.replace("vec2(", "float2(").replace(", 0.0))", ", float4(0.0, 0.0, 0.0, 0.0)))") for x in defs)
    (root / "src/render/shaders" / f"anime4k_c{i}.hlsl").write_text(
        LICENSE.replace("(MIT License)\n", "(MIT License)\n", 1) +
        f"// Pasada de convolución {i} de 4. u.xy = tamaño de la grilla lógica, u.z = escala entera de la imagen muestreada (1 para los\n"
        "// intermedios; destino_ guarda cada píxel lógico como un bloque de z*z, se lee en su centro).\n"
        f"// Gemelo HLSL de anime4k_c{i}.frag (Windows/D3D12); etapa de vértices: vs_mundo (mundo.hlsl). La textura solo se lee\n"
        "// con Load, así que no se declara sampler. Convención de recursos: ver sprite.hlsl.\n"
        "[[vk::binding(0, 2)]] Texture2D<float4> src : register(t0, space2);\n"
        "[[vk::binding(0, 3)]] cbuffer Params : register(b0, space3) {\n    float4 u;\n};\n"
        "struct Salida {\n    float4 pos : SV_Position;\n    float2 uv : TEXCOORD0;\n};\n"
        "static int2 P;\n"
        "float4 tex_off(float2 o) {\n"
        "    int2 q = clamp(P + int2(o), int2(0, 0), int2(u.xy) - 1);\n"
        "    return src.Load(int3(q * int(u.z) + int(u.z) / 2, 0));\n}\n"
        f"{hdefs}\n"
        f"float4 fs_anime4k_c{i}(Salida in_) : SV_Target0 {{\n    P = int2(in_.pos.xy);\n"
        f"    {hbody}\n    return result;\n}}\n")

    mbody = body.strip().replace("mat4(", "float4x4(").replace("vec4(", "float4(").replace("vec2(", "float2(")
    mbody = mbody.replace("vec4 result = ", "float4 result = ", 1)
    mdefs = "\n".join(x.replace("vec2(", "float2(").replace(", 0.0))", ", float4(0.0)))") for x in defs)
    msl_parts.append(
        f"fragment float4 fs_anime4k_c{i}(A4kSalida in [[stage_in]], texture2d<float> src [[texture(0)]],\n"
        f"                                constant float4& u [[buffer(0)]]) {{\n"
        f"    int2 P = int2(in.pos.xy);\n"
        f"    int S = int(u.z);\n"
        f"#define tex_off(o) src.read(uint2(clamp(P + int2(o), int2(0), int2(u.xy) - 1) * S + S / 2))\n"
        f"{mdefs}\n    {mbody}\n    return result;\n"
        f"#undef tex_off\n#undef go_0\n#undef go_1\n}}\n")
MSL_TAIL = r"""
// Depth-to-space de la última convolución + bilineal del original (pasada 5), a 2x la grilla lógica.
// u.xy = grilla, u.z = escala de la textura de la escena.
float4 a4k_scene(texture2d<float> scene, float4 u, int2 q) {
    int S = int(u.z);
    return scene.read(uint2(clamp(q, int2(0), int2(u.xy) - 1) * S + S / 2));
}
fragment float4 fs_anime4k_d2s(A4kSalida in [[stage_in]], texture2d<float> conv [[texture(0)]],
                               texture2d<float> scene [[texture(1)]], constant float4& u [[buffer(0)]]) {
    int2 o = int2(in.pos.xy);
    int2 i0 = o & 1;
    float c = conv.read(uint2(o >> 1))[i0.y * 2 + i0.x];
    float2 q = (float2(o) + 0.5) * 0.5 - 0.5;
    float2 fl = floor(q);
    float2 f = q - fl;
    int2 q0 = int2(fl);
    float4 top = mix(a4k_scene(scene, u, q0), a4k_scene(scene, u, q0 + int2(1, 0)), f.x);
    float4 bot = mix(a4k_scene(scene, u, q0 + int2(0, 1)), a4k_scene(scene, u, q0 + int2(1, 1)), f.x);
    return float4(float3(c) + mix(top, bot, f.y).rgb, 1.0);
}
// Remuestreo final de la imagen de 2x al destino del tamaño de la ventana (sampler lineal).
fragment float4 fs_anime4k_resample(A4kSalida in [[stage_in]], texture2d<float> x2 [[texture(0)]],
                                    sampler sm [[sampler(0)]]) {
    return float4(x2.sample(sm, in.uv).rgb, 1.0);
}
"""
(root / "src/render/shaders_msl_anime4k.h").write_text(
    "// Generado por herramientas/generar_anime4k.py: no editar. Gemelos MSL de anime4k_*.frag.\n"
    "#pragma once\nnamespace lin {\n"
    "inline constexpr const char* SHADER_MSL_ANIME4K = " + "".join(msl_parts) + MSL_TAIL + ')MSL";\n}  // namespace lin\n')
print("ok")

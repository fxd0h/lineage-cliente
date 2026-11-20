#version 450
// SPIR-V para SDL_GPU (Vulkan). Traducción de vs_sprite (SHADER_MSL en gpu.cpp).
// Convención de SDL_GPU: vértice set 0 = texturas/storage, set 1 = uniformes.
struct Instancia { vec4 destino; vec4 uv; uint pagina; uint color; uint mezcla; uint relleno; };
layout(std430, set = 0, binding = 0) readonly buffer Instancias { Instancia inst[]; };
layout(std140, set = 1, binding = 0) uniform Uniformes { vec2 pantalla; vec2 relleno; } u;
layout(location = 0) out vec2 o_uv;
layout(location = 1) out vec4 o_color;
layout(location = 2) flat out uint o_pagina;
layout(location = 3) flat out uint o_mezcla;
const vec2 esquinas[6] = vec2[6](vec2(0,0), vec2(1,0), vec2(0,1), vec2(1,0), vec2(1,1), vec2(0,1));
void main() {
    Instancia i = inst[uint(gl_InstanceIndex) + uint(u.relleno.x)];
    vec2 e = esquinas[gl_VertexIndex];
    vec2 p = i.destino.xy + e * i.destino.zw;
    gl_Position = vec4(p.x / u.pantalla.x * 2.0 - 1.0, 1.0 - p.y / u.pantalla.y * 2.0, 0.0, 1.0);
    // relleno.y: 0 = pass of the normal blends, 1 = pass of the screen blend (bit 3 of mezcla), 3 = pass of the multiply blend (bit 7), 2 = HD text layer
    // (text, plus every non-additive instance as an attenuator of the text under it).
    const uint modo = uint(u.relleno.y + 0.5);
    const bool texto = (i.mezcla & 64u) != 0u;
    bool oculto;
    if (modo == 2u) oculto = !texto && (i.mezcla & 137u) != 0u;
    else if (modo == 1u) oculto = texto || (i.mezcla & 8u) == 0u;
    else if (modo == 3u) oculto = texto || (i.mezcla & 128u) == 0u;
    else oculto = texto || (i.mezcla & 136u) != 0u;
    if (oculto) gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    o_uv = mix(i.uv.xy, i.uv.zw, e);
    o_color = unpackUnorm4x8(i.color);
    o_pagina = i.pagina;
    o_mezcla = i.mezcla;
}

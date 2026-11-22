#version 450
// Anime4K v3.2 Upscale CNN x2 (S), https://github.com/bloc97/Anime4K (MIT License, Copyright (c) 2019-2021
// bloc97; aviso completo en anime4k_c1.frag). Pasada 5: depth-to-space de la última convolución (4 canales = los 2x2
// subpíxeles) sumado a una muestra bilineal del original, que da la imagen de 2x. Portado a mano del hook
// "Depth-to-Space". u.xy = tamaño de la grilla lógica, u.z = escala entera de la textura de la escena.
// Gemelo MSL: fs_anime4k_d2s en shaders_msl_anime4k.h.
layout(set = 2, binding = 0) uniform sampler2D conv;
layout(set = 2, binding = 1) uniform sampler2D scene;
layout(std140, set = 3, binding = 0) uniform Params { vec4 u; };
layout(location = 0) out vec4 salida;
vec4 S(ivec2 q) {
    q = clamp(q, ivec2(0), ivec2(u.xy) - 1);
    return texelFetch(scene, q * int(u.z) + int(u.z) / 2, 0);
}
void main() {
    ivec2 o = ivec2(gl_FragCoord.xy);
    ivec2 i0 = o & 1;
    float c = texelFetch(conv, o >> 1, 0)[i0.y * 2 + i0.x];
    vec2 q = (vec2(o) + 0.5) * 0.5 - 0.5;
    vec2 fl = floor(q);
    vec2 f = q - fl;
    ivec2 q0 = ivec2(fl);
    vec4 top = mix(S(q0), S(q0 + ivec2(1, 0)), f.x);
    vec4 bot = mix(S(q0 + ivec2(0, 1)), S(q0 + ivec2(1, 1)), f.x);
    salida = vec4(vec3(c) + mix(top, bot, f.y).rgb, 1.0);
}

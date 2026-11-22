#version 450
// Texto HD: el atlas guarda campos de glifos (0,5 = contorno, ver recursos/fuente_hd.h). El contorno se dibuja con un
// smoothstep en espacio de pantalla, así el borde mide un píxel de pantalla a cualquier escala.
layout(set = 2, binding = 0) uniform sampler2DArray atlas;
layout(set = 2, binding = 1) uniform sampler2DArray atlas_lin;
layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;
layout(location = 2) flat in uint i_pagina;
layout(location = 3) flat in uint i_mezcla;
// Dos destinos (ver Gpu::pase_texto): 0 = color del texto premultiplicado, 1 = cobertura en R. Ambos usan la mezcla "over"
// premultiplicada, así una instancia atenuante (da negro, alfa = su alfa) deja dst * (1 - alfa) en ambos.
layout(location = 0) out vec4 salida;
layout(location = 1) out vec4 cobertura;
void main() {
    // Las dos muestras al principio (las derivadas necesitan control de flujo uniforme); ambas leen la misma capa.
    vec4 tn = texture(atlas, vec3(i_uv, float(i_pagina)));
    vec4 tl = texture(atlas_lin, vec3(i_uv, float(i_pagina)));
    float w = max(fwidth(tl.r) * 0.7, 0.02);
    if ((i_mezcla & 64u) != 0u) {
        float a = smoothstep(0.5 - w, 0.5 + w, tl.r) * i_color.a;
        if (a <= 0.0) discard;
        salida = vec4(i_color.rgb * a, a);
        cobertura = vec4(a, 0.0, 0.0, a);
    } else {
        // Bit 2 = imagen del pack HD (bilineal, premultiplicada); todo otro tipo tiene alfa = alfa del texel * alfa del tinte.
        float a = ((i_mezcla & 4u) != 0u ? tl.a : tn.a) * i_color.a;
        if (a <= 0.0) discard;
        salida = vec4(0.0, 0.0, 0.0, a);
        cobertura = vec4(0.0, 0.0, 0.0, a);
    }
}

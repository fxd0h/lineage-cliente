#version 450
// Traducción de fs_sprite. Fragmento: set 2 = texturas, set 3 = uniformes.
layout(set = 2, binding = 0) uniform sampler2DArray atlas;
layout(set = 2, binding = 1) uniform sampler2DArray atlas_lin;
layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;
layout(location = 2) flat in uint i_pagina;
layout(location = 3) flat in uint i_mezcla;
layout(location = 0) out vec4 salida;
void main() {
    vec4 t;
    if ((i_mezcla & 4u) != 0u) {
        // Bit 2 = imagen del pack HD: texels premultiplicados, bilineal; de vuelta a alfa directo para el camino común.
        t = texture(atlas_lin, vec3(i_uv, float(i_pagina)));
        t.rgb = t.a > 0.0 ? t.rgb / t.a : vec3(0.0);
    } else {
        t = texture(atlas, vec3(i_uv, float(i_pagina)));
    }
    // Bit 1 de mezcla = silueta: el color del tinte con el alfa de la textura. Bit 0 = aditiva.
    vec4 c = (i_mezcla & 2u) != 0u ? vec4(i_color.rgb, t.a * i_color.a) : t * i_color;
    // Bit 5 = gris: la luminancia de la textura por el tinte (íconos monocromos del HUD).
    if ((i_mezcla & 32u) != 0u) c.rgb = vec3(dot(t.rgb, vec3(0.299, 0.587, 0.114))) * i_color.rgb;
    if (c.a <= 0.0) discard;
    // Alfa premultiplicado: con alfa 0 el color se suma al fondo sin taparlo (mezcla aditiva).
    // Bit 7 = multiply: the blend is dst * src, so a transparent texel must output white (leaves dst unchanged).
    if ((i_mezcla & 128u) != 0u) { salida = vec4(mix(vec3(1.0), c.rgb, c.a), 0.0); return; }
    salida = vec4(c.rgb * c.a, (i_mezcla & 9u) != 0u ? 0.0 : c.a);
}

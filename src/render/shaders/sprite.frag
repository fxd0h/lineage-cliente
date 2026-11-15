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
        // Bit 2 = HD pack image: premultiplied texels, bilinear; back to straight alpha for the common path.
        t = texture(atlas_lin, vec3(i_uv, float(i_pagina)));
        t.rgb = t.a > 0.0 ? t.rgb / t.a : vec3(0.0);
    } else {
        t = texture(atlas, vec3(i_uv, float(i_pagina)));
    }
    // Bit 1 of mezcla = silhouette: the tint's color with the texture's alpha. Bit 0 = additive.
    vec4 c = (i_mezcla & 2u) != 0u ? vec4(i_color.rgb, t.a * i_color.a) : t * i_color;
    if (c.a <= 0.0) discard;
    // Alfa premultiplicado: con alfa 0 el color se suma al fondo sin taparlo (mezcla aditiva).
    salida = vec4(c.rgb * c.a, (i_mezcla & 1u) != 0u ? 0.0 : c.a);
}

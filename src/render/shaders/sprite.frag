#version 450
// Traducción de fs_sprite. Fragmento: set 2 = texturas, set 3 = uniformes.
layout(set = 2, binding = 0) uniform sampler2DArray atlas;
layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;
layout(location = 2) flat in uint i_pagina;
layout(location = 3) flat in uint i_mezcla;
layout(location = 0) out vec4 salida;
void main() {
    vec4 c = texture(atlas, vec3(i_uv, float(i_pagina))) * i_color;
    if (c.a <= 0.0) discard;
    // Alfa premultiplicado: con alfa 0 el color se suma al fondo sin taparlo (mezcla aditiva).
    salida = vec4(c.rgb * c.a, i_mezcla == 1u ? 0.0 : c.a);
}

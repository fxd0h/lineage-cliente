#version 450
// HD text: the atlas holds glyph fields (0.5 = outline, see recursos/fuente_hd.h). The outline is rendered with a
// screen-space smoothstep, so the edge is one display pixel wide at any scale.
layout(set = 2, binding = 0) uniform sampler2DArray atlas;
layout(set = 2, binding = 1) uniform sampler2DArray atlas_lin;
layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;
layout(location = 2) flat in uint i_pagina;
layout(location = 3) flat in uint i_mezcla;
layout(location = 0) out vec4 salida;
void main() {
    float f = texture(atlas_lin, vec3(i_uv, float(i_pagina))).r;
    float w = max(fwidth(f) * 0.7, 0.02);
    float a = smoothstep(0.5 - w, 0.5 + w, f) * i_color.a;
    if (a <= 0.0) discard;
    salida = vec4(i_color.rgb * a, a);
}

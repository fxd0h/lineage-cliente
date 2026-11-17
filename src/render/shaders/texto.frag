#version 450
// HD text: the atlas holds glyph fields (0.5 = outline, see recursos/fuente_hd.h). The outline is rendered with a
// screen-space smoothstep, so the edge is one display pixel wide at any scale.
layout(set = 2, binding = 0) uniform sampler2DArray atlas;
layout(set = 2, binding = 1) uniform sampler2DArray atlas_lin;
layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec4 i_color;
layout(location = 2) flat in uint i_pagina;
layout(location = 3) flat in uint i_mezcla;
// Two targets (see Gpu::pase_texto): 0 = premultiplied text colour, 1 = coverage in R. Both use the premultiplied "over"
// blend, so an attenuating instance (output black, alpha = its alpha) leaves dst * (1 - alpha) on both.
layout(location = 0) out vec4 salida;
layout(location = 1) out vec4 cobertura;
void main() {
    // Both samples up front (derivatives need uniform control flow); both read the same layer.
    vec4 tn = texture(atlas, vec3(i_uv, float(i_pagina)));
    vec4 tl = texture(atlas_lin, vec3(i_uv, float(i_pagina)));
    float w = max(fwidth(tl.r) * 0.7, 0.02);
    if ((i_mezcla & 64u) != 0u) {
        float a = smoothstep(0.5 - w, 0.5 + w, tl.r) * i_color.a;
        if (a <= 0.0) discard;
        salida = vec4(i_color.rgb * a, a);
        cobertura = vec4(a, 0.0, 0.0, a);
    } else {
        // Bit 2 = HD pack image (bilinear, premultiplied); every other kind has alpha = texel alpha * tint alpha.
        float a = ((i_mezcla & 4u) != 0u ? tl.a : tn.a) * i_color.a;
        if (a <= 0.0) discard;
        salida = vec4(0.0, 0.0, 0.0, a);
        cobertura = vec4(0.0, 0.0, 0.0, a);
    }
}

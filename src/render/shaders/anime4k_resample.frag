#version 450
// Final resample of the Anime4K 2x image to the window-sized target (linear sampler).
// Skipped when the integer scale is already 2. MSL twin: fs_anime4k_resample in shaders_msl_anime4k.h.
layout(set = 2, binding = 0) uniform sampler2D x2;
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 salida;
void main() { salida = vec4(texture(x2, i_uv).rgb, 1.0); }

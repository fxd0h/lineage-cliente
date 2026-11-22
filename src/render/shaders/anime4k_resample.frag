#version 450
// Remuestreo final de la imagen de 2x de Anime4K al destino del tamaño de la ventana (sampler lineal).
// Se salta cuando la escala entera ya es 2. Gemelo MSL: fs_anime4k_resample en shaders_msl_anime4k.h.
layout(set = 2, binding = 0) uniform sampler2D x2;
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 salida;
void main() { salida = vec4(texture(x2, i_uv).rgb, 1.0); }

#version 450
// Composición del texto HD: la capa de texto (binding 0 = color premultiplicado, binding 1 = cobertura en R; ambas del tamaño del
// destino) dibujada sobre él con la mezcla "over" premultiplicada: out = color + dst * (1 - cobertura).
// Etapa de vértices: mundo.vert (quad de pantalla completa). Gemelo MSL: fs_texto_comp en gpu.cpp.
layout(set = 2, binding = 0) uniform sampler2D capa;
layout(set = 2, binding = 1) uniform sampler2D cob;
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 salida;
void main() { salida = vec4(texture(capa, i_uv).rgb, texture(cob, i_uv).r); }

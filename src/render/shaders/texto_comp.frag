#version 450
// HD text composite: the text layer (binding 0 = premultiplied colour, binding 1 = coverage in R; both the size of the
// destination) drawn over it with the premultiplied "over" blend: out = colour + dst * (1 - coverage).
// Vertex stage: mundo.vert (fullscreen quad). MSL twin: fs_texto_comp in gpu.cpp.
layout(set = 2, binding = 0) uniform sampler2D capa;
layout(set = 2, binding = 1) uniform sampler2D cob;
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 salida;
void main() { salida = vec4(texture(capa, i_uv).rgb, texture(cob, i_uv).r); }

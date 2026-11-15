#version 450
// Traducción de vs_mundo.
layout(location = 0) out vec2 o_uv;
const vec2 esquinas[6] = vec2[6](vec2(0,0), vec2(1,0), vec2(0,1), vec2(1,0), vec2(1,1), vec2(0,1));
void main() {
    vec2 e = esquinas[gl_VertexIndex];
    gl_Position = vec4(e.x * 2.0 - 1.0, 1.0 - e.y * 2.0, 0.0, 1.0);
    o_uv = e;
}

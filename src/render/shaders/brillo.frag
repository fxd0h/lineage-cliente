#version 450
// Traducción de fs_brillo: brillo (R8 por celda) × color por fila × tinte.
layout(set = 2, binding = 0) uniform sampler2D luz;
layout(set = 2, binding = 1) uniform sampler2D filas;
layout(std140, set = 3, binding = 0) uniform Tinte { vec4 tinte; };
layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec2 i_uvf;
layout(location = 0) out vec4 salida;
void main() {
    float b = texture(luz, i_uv).r;
    vec3 f = texture(filas, i_uvf).rgb;
    salida = vec4(tinte.rgb * b * f, 1.0);
}

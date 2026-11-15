#version 450
// Traducción de fs_mundo: ola, borrachera y visión sobre una copia del mundo.
layout(set = 2, binding = 0) uniform sampler2D escena;
layout(std140, set = 3, binding = 0) uniform Params { vec4 u[2]; };
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 salida;
void main() {
    vec4 p = u[0];  // ancho, alto, fase de la ola (< 0 sin ola), modo de visión
    vec4 b = u[1];  // fase de la borrachera (< 0 sin ella), amplitud
    vec2 uv = i_uv;
    if (b.x >= 0.0) {
        float fila = floor(uv.y * p.y);
        float dx = trunc(sin((fila + b.x) * 3.14159265 * 2.0 / 100.0) * b.y);
        float x = clamp(floor(uv.x * p.x) + dx, 0.0, p.x - 1.0);
        uv.x = (x + 0.5) / p.x;
    }
    if (p.z >= 0.0) {
        float xl = floor(uv.x * p.x);
        float yl = floor(uv.y * p.y);
        float dy = trunc(sin((xl + p.z) * 3.14159265 / 50.0) * 6.0);
        float y = clamp(yl + dy, 0.0, p.y - 1.0);
        uv = vec2((xl + 0.5) / p.x, (y + 0.5) / p.y);
    }
    vec3 c = texture(escena, uv).rgb;
    if (p.w == 1.0) c = c * vec3(1.0 / 16.0, 0.5, 1.0 / 16.0);
    else if (p.w == 2.0) c = c * ((2.0 * c.r + 7.0 * c.g + c.b) / 10.0);
    salida = vec4(c, 1.0);
}

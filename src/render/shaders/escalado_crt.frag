#version 450
// Final-present scaler "crt": lightweight CRT look (own implementation): horizontal linear
// interpolation between source pixels, per-source-row scanline profile, RGB triad mask, slight gain.
// u.xy = source size, u.zw = output size. MSL twin: fs_escalado_crt in gpu.cpp.
layout(set = 2, binding = 0) uniform sampler2D escena;
layout(std140, set = 3, binding = 0) uniform Params { vec4 u; };
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 salida;
vec3 P(vec2 p) { return texture(escena, (clamp(p, vec2(0.0), u.xy - 1.0) + 0.5) / u.xy).rgb; }
void main() {
    vec2 sp = i_uv * u.xy;
    vec2 c = floor(sp);
    vec2 f = sp - c;
    float nx = f.x < 0.5 ? -1.0 : 1.0;
    float k = abs(f.x - 0.5);
    vec3 col = mix(P(c), P(c + vec2(nx, 0.0)), smoothstep(0.0, 1.0, k * 0.8));
    float scan = 0.82 + 0.18 * cos(6.2831853 * (f.y - 0.25));  // 1.0 on the first half-row, 0.64 on the second
    int ox = int(i_uv.x * u.z);
    vec3 mask = vec3(0.8);
    mask[ox % 3] = 1.0;
    salida = vec4(clamp(col * scan * mask * 1.12, 0.0, 1.0), 1.0);
}

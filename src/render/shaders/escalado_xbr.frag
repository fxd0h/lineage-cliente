#version 450
// Final-present scaler "xbr": xBR-style edge-directed pixel-art upscaler (own implementation).
// Uses the xBR edge test (weighted colour distances along the two diagonals around each source-pixel
// corner, as published by Hyllian's xBR family) with a corner-cut rule (lv1-like, anti-aliased);
// it is NOT a port of xBR-lv2. Written for this project, GPL-3.0 like the rest of the client.
// Source: logical image sampled with nearest at texel centres. u.xy = source size, u.zw = output size.
// MSL twin: fs_escalado_xbr in gpu.cpp (keep both in sync).
layout(set = 2, binding = 0) uniform sampler2D escena;
layout(std140, set = 3, binding = 0) uniform Params { vec4 u; };
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 salida;
vec3 P(vec2 p) { return texture(escena, (clamp(p, vec2(0.0), u.xy - 1.0) + 0.5) / u.xy).rgb; }
float d(vec3 a, vec3 b) { return dot(abs(a - b), vec3(0.299, 0.587, 0.114)); }
void main() {
    vec2 sp = i_uv * u.xy;
    vec2 c = floor(sp);
    vec2 f = sp - c - 0.5;                 // -0.5..0.5 inside the source pixel
    vec2 s = vec2(f.x < 0.0 ? -1.0 : 1.0, f.y < 0.0 ? -1.0 : 1.0);  // nearest corner
    vec3 E = P(c);
    vec3 H = P(c + vec2(s.x, 0.0));        // horizontal neighbour towards the corner
    vec3 V = P(c + vec2(0.0, s.y));        // vertical neighbour towards the corner
    vec3 I = P(c + s);                     // diagonal
    vec3 Hb = P(c + vec2(-s.x, 0.0));      // E's opposite neighbours
    vec3 Vb = P(c + vec2(0.0, -s.y));
    vec3 C = P(c + vec2(s.x, -s.y));       // anti-diagonal pair around the corner
    vec3 G = P(c + vec2(-s.x, s.y));
    vec3 H2 = P(c + vec2(2.0 * s.x, s.y)); // outer taps next to I
    vec3 V2 = P(c + vec2(s.x, 2.0 * s.y));
    // Edge test for the corner: 'e' = cost of the E-I diagonal being a line separating H and V
    float e = d(E, C) + d(E, G) + d(I, H2) + d(I, V2) + 4.0 * d(V, H);
    float i = d(V, Hb) + d(V, V2) + d(H, H2) + d(H, Vb) + 4.0 * d(E, I);
    vec3 col = E;
    if (e < i && d(E, I) > 0.02) {
        vec3 n = d(E, H) <= d(E, V) ? H : V;           // neighbour closest to E carries the edge
        float t = abs(f.x) + abs(f.y);                // 0 centre .. 1 corner
        float w = 0.5 * length(u.xy / u.zw) * 1.5;    // ~1 output pixel in t units
        float a = smoothstep(0.5 - w, 0.5 + w, t);
        col = mix(E, n, a);
    }
    salida = vec4(col, 1.0);
}

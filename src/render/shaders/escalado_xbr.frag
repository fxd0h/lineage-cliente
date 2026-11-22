#version 450
// Escalador de la presentación final "xbr": escalador de pixel art dirigido por bordes al estilo xBR (implementación propia).
// Usa la prueba de bordes de xBR (distancias de color ponderadas a lo largo de las dos diagonales alrededor de cada esquina
// del píxel de origen, como la publicó la familia xBR de Hyllian) con una regla de corte de esquinas (tipo lv1, con suavizado);
// NO es un port de xBR-lv2. Escrito para este proyecto, GPL-3.0 como el resto del cliente.
// Origen: imagen lógica muestreada por vecino más cercano en el centro de los texels. u.xy = tamaño de origen, u.zw = tamaño de salida.
// Gemelo MSL: fs_escalado_xbr en gpu.cpp (mantener ambos sincronizados).
layout(set = 2, binding = 0) uniform sampler2D escena;
layout(std140, set = 3, binding = 0) uniform Params { vec4 u; };
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 salida;
vec3 P(vec2 p) { return texture(escena, (clamp(p, vec2(0.0), u.xy - 1.0) + 0.5) / u.xy).rgb; }
float d(vec3 a, vec3 b) { return dot(abs(a - b), vec3(0.299, 0.587, 0.114)); }
void main() {
    vec2 sp = i_uv * u.xy;
    vec2 c = floor(sp);
    vec2 f = sp - c - 0.5;                 // -0.5..0.5 dentro del píxel de origen
    vec2 s = vec2(f.x < 0.0 ? -1.0 : 1.0, f.y < 0.0 ? -1.0 : 1.0);  // esquina más cercana
    vec3 E = P(c);
    vec3 H = P(c + vec2(s.x, 0.0));        // vecino horizontal hacia la esquina
    vec3 V = P(c + vec2(0.0, s.y));        // vecino vertical hacia la esquina
    vec3 I = P(c + s);                     // diagonal
    vec3 Hb = P(c + vec2(-s.x, 0.0));      // E's opposite neighbours
    vec3 Vb = P(c + vec2(0.0, -s.y));
    vec3 C = P(c + vec2(s.x, -s.y));       // par antidiagonal alrededor de la esquina
    vec3 G = P(c + vec2(-s.x, s.y));
    vec3 H2 = P(c + vec2(2.0 * s.x, s.y)); // lecturas exteriores junto a I
    vec3 V2 = P(c + vec2(s.x, 2.0 * s.y));
    // Prueba de borde para la esquina: 'e' = costo de que la diagonal E-I sea una línea que separa H y V
    float e = d(E, C) + d(E, G) + d(I, H2) + d(I, V2) + 4.0 * d(V, H);
    float i = d(V, Hb) + d(V, V2) + d(H, H2) + d(H, Vb) + 4.0 * d(E, I);
    vec3 col = E;
    if (e < i && d(E, I) > 0.02) {
        vec3 n = d(E, H) <= d(E, V) ? H : V;           // el vecino más cercano a E lleva el borde
        float t = abs(f.x) + abs(f.y);                // 0 centro .. 1 esquina
        float w = 0.5 * length(u.xy / u.zw) * 1.5;    // ~1 píxel de salida en unidades de t
        float a = smoothstep(0.5 - w, 0.5 + w, t);
        col = mix(E, n, a);
    }
    salida = vec4(col, 1.0);
}

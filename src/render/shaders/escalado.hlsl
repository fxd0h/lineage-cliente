// Escaladores de la presentación final, gemelos HLSL de escalado_xbr.frag / escalado_fsr.frag / escalado_crt.frag (Windows/D3D12).
// Puntos de entrada fs_escalado_xbr, fs_escalado_fsr, fs_escalado_crt; etapa de vértices: vs_mundo (mundo.hlsl).
// Origen: imagen lógica muestreada por vecino más cercano en el centro de los texels. u.xy = tamaño de origen, u.zw = tamaño de salida.
// Convención de recursos: ver sprite.hlsl. Las lecturas de textura usan SampleLevel(.., 0): la textura de la escena tiene un nivel.
//
// xbr: escalador de pixel art dirigido por bordes al estilo xBR (implementación propia, GPL-3.0 como el resto del cliente). Usa
//   la prueba de bordes de xBR (distancias de color ponderadas a lo largo de las dos diagonales alrededor de cada esquina del píxel de origen, como
//   la publicó la familia xBR de Hyllian) con una regla de corte de esquinas; NO es un port de xBR-lv2.
// fsr: AMD FidelityFX Super Resolution 1.0, pasada EASU, reescrita desde el algoritmo público (FsrEasuF, ffx_fsr1.h).
//   Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. MIT License: permission is granted, free of
//   charge, to use, copy, modify, merge, publish and distribute this software and derived works, provided this notice
//   is kept; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND. No incluye el afilado RCAS.
// crt: aspecto CRT liviano (implementación propia): interpolación lineal horizontal entre píxeles de origen,
//   perfil de línea de barrido por fila de origen, máscara de tríadas RGB, leve ganancia.

[[vk::binding(0, 2)]] Texture2D<float4> escena : register(t0, space2);
[[vk::binding(0, 2)]] SamplerState muestreo : register(s0, space2);
[[vk::binding(0, 3)]] cbuffer Params : register(b0, space3) {
    float4 u;
};

struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

float3 P(float2 p) {
    return escena.SampleLevel(muestreo, (clamp(p, float2(0.0, 0.0), u.xy - 1.0) + 0.5) / u.xy, 0.0).rgb;
}

// ---- xbr ----

float d(float3 a, float3 b) { return dot(abs(a - b), float3(0.299, 0.587, 0.114)); }

float4 fs_escalado_xbr(Salida in_) : SV_Target0 {
    float2 sp = in_.uv * u.xy;
    float2 c = floor(sp);
    float2 f = sp - c - 0.5;  // -0.5..0.5 dentro del píxel de origen
    float2 s = float2(f.x < 0.0 ? -1.0 : 1.0, f.y < 0.0 ? -1.0 : 1.0);  // esquina más cercana
    float3 E = P(c);
    float3 H = P(c + float2(s.x, 0.0));         // vecino horizontal hacia la esquina
    float3 V = P(c + float2(0.0, s.y));         // vecino vertical hacia la esquina
    float3 I = P(c + s);                        // diagonal
    float3 Hb = P(c + float2(-s.x, 0.0));       // E's opposite neighbours
    float3 Vb = P(c + float2(0.0, -s.y));
    float3 C = P(c + float2(s.x, -s.y));        // par antidiagonal alrededor de la esquina
    float3 G = P(c + float2(-s.x, s.y));
    float3 H2 = P(c + float2(2.0 * s.x, s.y));  // lecturas exteriores junto a I
    float3 V2 = P(c + float2(s.x, 2.0 * s.y));
    // Prueba de borde para la esquina: 'e' = costo de que la diagonal E-I sea una línea que separa H y V
    float e = d(E, C) + d(E, G) + d(I, H2) + d(I, V2) + 4.0 * d(V, H);
    float i = d(V, Hb) + d(V, V2) + d(H, H2) + d(H, Vb) + 4.0 * d(E, I);
    float3 col = E;
    if (e < i && d(E, I) > 0.02) {
        float3 n = d(E, H) <= d(E, V) ? H : V;           // el vecino más cercano a E lleva el borde
        float t = abs(f.x) + abs(f.y);                   // 0 centro .. 1 esquina
        float w = 0.5 * length(u.xy / u.zw) * 1.5;       // ~1 píxel de salida en unidades de t
        float a = smoothstep(0.5 - w, 0.5 + w, t);
        col = lerp(E, n, a);
    }
    return float4(col, 1.0);
}

// ---- fsr ----

float L(float3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }

void tap(inout float3 aC, inout float aW, float2 off, float2 dir, float2 len, float lob, float clp, float3 c) {
    float2 v = float2(off.x * dir.x + off.y * dir.y, off.x * (-dir.y) + off.y * dir.x);
    v *= len;
    float d2 = min(v.x * v.x + v.y * v.y, clp);
    float wB = 2.0 / 5.0 * d2 - 1.0;
    float wA = lob * d2 - 1.0;
    wB *= wB;
    wA *= wA;
    wB = 25.0 / 16.0 * wB - (25.0 / 16.0 - 1.0);
    float w = wB * wA;
    aC += c * w;
    aW += w;
}

void setd(inout float2 dir, inout float len, float w, float lA, float lB, float lC, float lD, float lE) {
    float lenX = max(abs(lD - lC), abs(lC - lB));
    float dirX = lD - lB;
    dir.x += dirX * w;
    lenX = clamp(abs(dirX) / lenX, 0.0, 1.0);
    len += lenX * lenX * w;
    float lenY = max(abs(lE - lC), abs(lC - lA));
    float dirY = lE - lA;
    dir.y += dirY * w;
    lenY = clamp(abs(dirY) / lenY, 0.0, 1.0);
    len += lenY * lenY * w;
}

float4 fs_escalado_fsr(Salida in_) : SV_Target0 {
    float2 pp = in_.uv * u.xy - 0.5;
    float2 fp = floor(pp);
    pp -= fp;
    //    b c
    //  e f g h
    //  i j k l
    //    n o
    float3 b = P(fp + float2(0, -1)), c = P(fp + float2(1, -1));
    float3 e = P(fp + float2(-1, 0)), f = P(fp), g = P(fp + float2(1, 0)), h = P(fp + float2(2, 0));
    float3 i = P(fp + float2(-1, 1)), j = P(fp + float2(0, 1)), k = P(fp + float2(1, 1)), l = P(fp + float2(2, 1));
    float3 n = P(fp + float2(0, 2)), o = P(fp + float2(1, 2));
    float bL = L(b), cL = L(c), eL = L(e), fL = L(f), gL = L(g), hL = L(h);
    float iL = L(i), jL = L(j), kL = L(k), lL = L(l), nL = L(n), oL = L(o);
    float2 dir = float2(0.0, 0.0);
    float len = 0.0;
    setd(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bL, eL, fL, gL, jL);
    setd(dir, len, pp.x * (1.0 - pp.y), cL, fL, gL, hL, kL);
    setd(dir, len, (1.0 - pp.x) * pp.y, fL, iL, jL, kL, nL);
    setd(dir, len, pp.x * pp.y, gL, jL, kL, lL, oL);
    float2 dir2 = dir * dir;
    float dirR = dir2.x + dir2.y;
    bool zro = dirR < (1.0 / 32768.0);
    dirR = rsqrt(dirR);
    dirR = zro ? 1.0 : dirR;
    dir.x = zro ? 1.0 : dir.x;
    dir *= float2(dirR, dirR);
    len = len * 0.5;
    len *= len;
    float stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));
    float2 len2 = float2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
    float clp = 1.0 / lob;
    float3 min4 = min(min(f, g), min(j, k));
    float3 max4 = max(max(f, g), max(j, k));
    float3 aC = float3(0.0, 0.0, 0.0);
    float aW = 0.0;
    tap(aC, aW, float2(0, -1) - pp, dir, len2, lob, clp, b);
    tap(aC, aW, float2(1, -1) - pp, dir, len2, lob, clp, c);
    tap(aC, aW, float2(-1, 1) - pp, dir, len2, lob, clp, i);
    tap(aC, aW, float2(0, 1) - pp, dir, len2, lob, clp, j);
    tap(aC, aW, float2(0, 0) - pp, dir, len2, lob, clp, f);
    tap(aC, aW, float2(-1, 0) - pp, dir, len2, lob, clp, e);
    tap(aC, aW, float2(1, 1) - pp, dir, len2, lob, clp, k);
    tap(aC, aW, float2(2, 1) - pp, dir, len2, lob, clp, l);
    tap(aC, aW, float2(2, 0) - pp, dir, len2, lob, clp, h);
    tap(aC, aW, float2(1, 0) - pp, dir, len2, lob, clp, g);
    tap(aC, aW, float2(1, 2) - pp, dir, len2, lob, clp, o);
    tap(aC, aW, float2(0, 2) - pp, dir, len2, lob, clp, n);
    return float4(min(max4, max(min4, aC / aW)), 1.0);
}

// ---- crt ----

float4 fs_escalado_crt(Salida in_) : SV_Target0 {
    float2 sp = in_.uv * u.xy;
    float2 c = floor(sp);
    float2 f = sp - c;
    float nx = f.x < 0.5 ? -1.0 : 1.0;
    float k = abs(f.x - 0.5);
    float3 col = lerp(P(c), P(c + float2(nx, 0.0)), smoothstep(0.0, 1.0, k * 0.8));
    float scan = 0.82 + 0.18 * cos(6.2831853 * (f.y - 0.25));  // 1,0 en la primera media fila, 0,64 en la segunda
    int ox = int(in_.uv.x * u.z) % 3;
    float3 mask = float3(ox == 0 ? 1.0 : 0.8, ox == 1 ? 1.0 : 0.8, ox == 2 ? 1.0 : 0.8);
    return float4(clamp(col * scan * mask * 1.12, 0.0, 1.0), 1.0);
}

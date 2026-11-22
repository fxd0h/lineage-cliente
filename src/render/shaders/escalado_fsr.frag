#version 450
// Escalador de la presentación final "fsr": AMD FidelityFX Super Resolution 1.0, pasada EASU (escalado espacial
// adaptado a los bordes), reescrita desde el algoritmo público (FsrEasuF, ffx_fsr1.h).
// Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. MIT License:
// permission is granted, free of charge, to use, copy, modify, merge, publish and distribute this
// software and derived works, provided this notice is kept; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT
// WARRANTY OF ANY KIND. No incluye el afilado RCAS. Las lecturas son de punto (sin gathers).
// u.xy = tamaño de origen, u.zw = tamaño de salida. Gemelo MSL: fs_escalado_fsr en gpu.cpp.
layout(set = 2, binding = 0) uniform sampler2D escena;
layout(std140, set = 3, binding = 0) uniform Params { vec4 u; };
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec4 salida;
vec3 P(vec2 p) { return texture(escena, (clamp(p, vec2(0.0), u.xy - 1.0) + 0.5) / u.xy).rgb; }
float L(vec3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }
void tap(inout vec3 aC, inout float aW, vec2 off, vec2 dir, vec2 len, float lob, float clp, vec3 c) {
    vec2 v = vec2(off.x * dir.x + off.y * dir.y, off.x * (-dir.y) + off.y * dir.x);
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
void setd(inout vec2 dir, inout float len, float w, float lA, float lB, float lC, float lD, float lE) {
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
void main() {
    vec2 pp = i_uv * u.xy - 0.5;
    vec2 fp = floor(pp);
    pp -= fp;
    //    b c
    //  e f g h
    //  i j k l
    //    n o
    vec3 b = P(fp + vec2(0, -1)), c = P(fp + vec2(1, -1));
    vec3 e = P(fp + vec2(-1, 0)), f = P(fp), g = P(fp + vec2(1, 0)), h = P(fp + vec2(2, 0));
    vec3 i = P(fp + vec2(-1, 1)), j = P(fp + vec2(0, 1)), k = P(fp + vec2(1, 1)), l = P(fp + vec2(2, 1));
    vec3 n = P(fp + vec2(0, 2)), o = P(fp + vec2(1, 2));
    float bL = L(b), cL = L(c), eL = L(e), fL = L(f), gL = L(g), hL = L(h);
    float iL = L(i), jL = L(j), kL = L(k), lL = L(l), nL = L(n), oL = L(o);
    vec2 dir = vec2(0.0);
    float len = 0.0;
    setd(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bL, eL, fL, gL, jL);
    setd(dir, len, pp.x * (1.0 - pp.y), cL, fL, gL, hL, kL);
    setd(dir, len, (1.0 - pp.x) * pp.y, fL, iL, jL, kL, nL);
    setd(dir, len, pp.x * pp.y, gL, jL, kL, lL, oL);
    vec2 dir2 = dir * dir;
    float dirR = dir2.x + dir2.y;
    bool zro = dirR < (1.0 / 32768.0);
    dirR = inversesqrt(dirR);
    dirR = zro ? 1.0 : dirR;
    dir.x = zro ? 1.0 : dir.x;
    dir *= vec2(dirR);
    len = len * 0.5;
    len *= len;
    float stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));
    vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
    float clp = 1.0 / lob;
    vec3 min4 = min(min(f, g), min(j, k));
    vec3 max4 = max(max(f, g), max(j, k));
    vec3 aC = vec3(0.0);
    float aW = 0.0;
    tap(aC, aW, vec2(0, -1) - pp, dir, len2, lob, clp, b);
    tap(aC, aW, vec2(1, -1) - pp, dir, len2, lob, clp, c);
    tap(aC, aW, vec2(-1, 1) - pp, dir, len2, lob, clp, i);
    tap(aC, aW, vec2(0, 1) - pp, dir, len2, lob, clp, j);
    tap(aC, aW, vec2(0, 0) - pp, dir, len2, lob, clp, f);
    tap(aC, aW, vec2(-1, 0) - pp, dir, len2, lob, clp, e);
    tap(aC, aW, vec2(1, 1) - pp, dir, len2, lob, clp, k);
    tap(aC, aW, vec2(2, 1) - pp, dir, len2, lob, clp, l);
    tap(aC, aW, vec2(2, 0) - pp, dir, len2, lob, clp, h);
    tap(aC, aW, vec2(1, 0) - pp, dir, len2, lob, clp, g);
    tap(aC, aW, vec2(1, 2) - pp, dir, len2, lob, clp, o);
    tap(aC, aW, vec2(0, 2) - pp, dir, len2, lob, clp, n);
    salida = vec4(min(max4, max(min4, aC / aW)), 1.0);
}

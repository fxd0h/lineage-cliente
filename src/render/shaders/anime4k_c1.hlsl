// Anime4K v3.2 Upscale CNN x2 (S), https://github.com/bloc97/Anime4K (MIT License)
// Copyright (c) 2019-2021 bloc97. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
// following conditions: the above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
// KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// Generado por herramientas/generar_anime4k.py: no editar.
// Pasada de convolución 1 de 4. u.xy = tamaño de la grilla lógica, u.z = escala entera de la imagen muestreada (1 para los
// intermedios; destino_ guarda cada píxel lógico como un bloque de z*z, se lee en su centro).
// Gemelo HLSL de anime4k_c1.frag (Windows/D3D12); etapa de vértices: vs_mundo (mundo.hlsl). La textura solo se lee
// con Load, así que no se declara sampler. Convención de recursos: ver sprite.hlsl.
[[vk::binding(0, 2)]] Texture2D<float4> src : register(t0, space2);
[[vk::binding(0, 3)]] cbuffer Params : register(b0, space3) {
    float4 u;
};
struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};
static int2 P;
float4 tex_off(float2 o) {
    int2 q = clamp(P + int2(o), int2(0, 0), int2(u.xy) - 1);
    return src.Load(int3(q * int(u.z) + int(u.z) / 2, 0));
}
#define go_0(x_off, y_off) (tex_off(float2(x_off, y_off)))
float4 fs_anime4k_c1(Salida in_) : SV_Target0 {
    P = int2(in_.pos.xy);
    float4 result = mul(go_0(-1.0, -1.0), float4x4(-0.0057322932, 0.12928207, -0.056848746, 0.18680117, -0.0306273, 0.25602463, 0.053723164, 0.20419341, 0.0018709862, 0.022848232, -0.04105527, 0.10169034, 0.0, 0.0, 0.0, 0.0));
    result += mul(go_0(-1.0, 0.0), float4x4(0.009471417, -0.12957802, 0.096014425, 0.21836184, 0.00021601951, -0.22997683, 0.23666254, 0.41192335, 0.021762101, 0.0047863554, 0.008233427, 0.108514786, 0.0, 0.0, 0.0, 0.0));
    result += mul(go_0(-1.0, 1.0), float4x4(-0.01156376, -0.18988979, 0.04614705, -0.044767227, 0.01050636, -0.26426336, 0.23741047, 0.0027636609, -0.027718676, -0.14202335, -0.016650287, -0.06637125, 0.0, 0.0, 0.0, 0.0));
    result += mul(go_0(0.0, -1.0), float4x4(0.057809234, -0.11033858, 0.056533534, -0.06292466, 0.13880666, -0.18710336, 0.2441031, -0.25326246, 0.0032683122, -0.026437074, 0.0023248852, 7.640766e-05, 0.0, 0.0, 0.0, 0.0));
    result += mul(go_0(0.0, 0.0), float4x4(-0.49110603, 0.4429004, -0.44015464, -0.41174838, -0.87738293, 0.7808468, -1.0929365, -0.59699076, -0.18409836, 0.185138, -0.11773224, -0.17097276, 0.0, 0.0, 0.0, 0.0));
    result += mul(go_0(0.0, 1.0), float4x4(0.10580959, -0.055947904, -0.03431237, -0.080236495, 0.14862584, -0.15393938, -0.18872876, -0.3170681, 0.03559387, -0.003990826, 0.021298569, 0.012844483, 0.0, 0.0, 0.0, 0.0));
    result += mul(go_0(1.0, -1.0), float4x4(-0.040715586, -0.25781113, 0.08896714, -0.1225879, -0.15790503, -0.54010904, 0.29588607, 0.10401059, 0.003413123, -0.108357325, 0.0112870345, -0.11888622, 0.0, 0.0, 0.0, 0.0));
    result += mul(go_0(1.0, 0.0), float4x4(0.0049315444, 0.02376202, -0.08224771, 0.121118225, -0.041512914, -0.027994309, -0.585988, -0.069672115, -0.017247835, 0.0056576864, 0.04319012, 0.055003505, 0.0, 0.0, 0.0, 0.0));
    result += mul(go_0(1.0, 1.0), float4x4(0.37521392, 0.15916082, 0.059708964, 0.19046007, 0.8120325, 0.38343868, 0.3436578, 0.5287958, 0.16570656, 0.06957687, 0.014022592, 0.074799836, 0.0, 0.0, 0.0, 0.0));
    result += float4(-0.01050964, -0.00939481, 0.17684458, 0.027366742);
    return result;
}

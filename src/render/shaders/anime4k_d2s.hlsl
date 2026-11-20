// Anime4K v3.2 Upscale CNN x2 (S), https://github.com/bloc97/Anime4K (MIT License, Copyright (c) 2019-2021
// bloc97; full notice in anime4k_c1.hlsl). Pass 5: depth-to-space of the last conv (4 channels = the 2x2
// sub-pixels) added to a bilinear sample of the original, producing the 2x image. Hand port of the
// "Depth-to-Space" hook. u.xy = logical grid size, u.z = integer scale of the scene texture.
// HLSL twin of anime4k_d2s.frag (Windows/D3D12). Vertex stage: vs_mundo (mundo.hlsl).
// Both textures are only read with Load, so no samplers are declared. Resource convention: see sprite.hlsl.

[[vk::binding(0, 2)]] Texture2D<float4> conv : register(t0, space2);
[[vk::binding(1, 2)]] Texture2D<float4> scene : register(t1, space2);
[[vk::binding(0, 3)]] cbuffer Params : register(b0, space3) {
    float4 u;
};

struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 S(int2 q) {
    q = clamp(q, int2(0, 0), int2(u.xy) - 1);
    return scene.Load(int3(q * int(u.z) + int(u.z) / 2, 0));
}

float4 fs_anime4k_d2s(Salida i) : SV_Target0 {
    int2 o = int2(i.pos.xy);
    int2 i0 = o & 1;
    float c = conv.Load(int3(o >> 1, 0))[i0.y * 2 + i0.x];
    float2 q = (float2(o) + 0.5) * 0.5 - 0.5;
    float2 fl = floor(q);
    float2 f = q - fl;
    int2 q0 = int2(fl);
    float4 top = lerp(S(q0), S(q0 + int2(1, 0)), f.x);
    float4 bot = lerp(S(q0 + int2(0, 1)), S(q0 + int2(1, 1)), f.x);
    return float4(float3(c, c, c) + lerp(top, bot, f.y).rgb, 1.0);
}

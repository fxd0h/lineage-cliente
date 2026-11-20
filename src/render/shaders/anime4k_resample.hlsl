// Final resample of the Anime4K 2x image to the window-sized target (linear sampler).
// Skipped when the integer scale is already 2. HLSL twin of anime4k_resample.frag (Windows/D3D12).
// Vertex stage: vs_mundo (mundo.hlsl). Resource convention: see sprite.hlsl.

[[vk::binding(0, 2)]] Texture2D<float4> x2 : register(t0, space2);
[[vk::binding(0, 2)]] SamplerState muestreo : register(s0, space2);

struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 fs_anime4k_resample(Salida i) : SV_Target0 {
    return float4(x2.Sample(muestreo, i.uv).rgb, 1.0);
}

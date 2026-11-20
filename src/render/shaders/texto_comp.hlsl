// HD text composite: the text layer (texture 0 = premultiplied colour, texture 1 = coverage in R; both the size of the
// destination) drawn over it with the premultiplied "over" blend: out = colour + dst * (1 - coverage).
// Vertex stage: vs_mundo (mundo.hlsl, fullscreen quad). HLSL twin of texto_comp.frag (Windows/D3D12).
// Resource convention: see sprite.hlsl.

[[vk::binding(0, 2)]] Texture2D<float4> capa : register(t0, space2);
[[vk::binding(0, 2)]] SamplerState muestreo_capa : register(s0, space2);
[[vk::binding(1, 2)]] Texture2D<float4> cob : register(t1, space2);
[[vk::binding(1, 2)]] SamplerState muestreo_cob : register(s1, space2);

struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 fs_texto_comp(Salida i) : SV_Target0 {
    return float4(capa.Sample(muestreo_capa, i.uv).rgb, cob.Sample(muestreo_cob, i.uv).r);
}

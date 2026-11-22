// Composición del texto HD: la capa de texto (textura 0 = color premultiplicado, textura 1 = cobertura en R; ambas del tamaño del
// destino) dibujada sobre él con la mezcla "over" premultiplicada: out = color + dst * (1 - cobertura).
// Etapa de vértices: vs_mundo (mundo.hlsl, quad de pantalla completa). Gemelo HLSL de texto_comp.frag (Windows/D3D12).
// Convención de recursos: ver sprite.hlsl.

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

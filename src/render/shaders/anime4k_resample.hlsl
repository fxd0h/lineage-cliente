// Remuestreo final de la imagen de 2x de Anime4K al destino del tamaño de la ventana (sampler lineal).
// Se salta cuando la escala entera ya es 2. Gemelo HLSL de anime4k_resample.frag (Windows/D3D12).
// Etapa de vértices: vs_mundo (mundo.hlsl). Convención de recursos: ver sprite.hlsl.

[[vk::binding(0, 2)]] Texture2D<float4> x2 : register(t0, space2);
[[vk::binding(0, 2)]] SamplerState muestreo : register(s0, space2);

struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 fs_anime4k_resample(Salida i) : SV_Target0 {
    return float4(x2.Sample(muestreo, i.uv).rgb, 1.0);
}

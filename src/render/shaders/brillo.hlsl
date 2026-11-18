// Multiplicar el mundo: brillo (textura R8 por celda) × color por fila (textura de 1 × N) × tinte.
// Convención de recursos: ver sprite.hlsl.

[[vk::binding(0, 1)]] cbuffer Escala : register(b0, space1) {
    float4 escala;  // x, y: uv de la vista en la textura de brillo; z: v de la última fila
};

[[vk::binding(0, 2)]] Texture2D<float4> luz : register(t0, space2);
[[vk::binding(0, 2)]] SamplerState muestreo_luz : register(s0, space2);
[[vk::binding(1, 2)]] Texture2D<float4> filas : register(t1, space2);
[[vk::binding(1, 2)]] SamplerState muestreo_filas : register(s1, space2);
[[vk::binding(0, 3)]] cbuffer Tinte : register(b0, space3) {
    float4 tinte;
};

struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float2 uvf : TEXCOORD1;
};

static const float2 ESQUINAS[6] = {float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 0), float2(1, 1), float2(0, 1)};

Salida vs_brillo(uint vid : SV_VertexID) {
    float2 e = ESQUINAS[vid];
    Salida o;
    o.pos = float4(e.x * 2.0 - 1.0, 1.0 - e.y * 2.0, 0.0, 1.0);
    o.uv = e * escala.xy;
    o.uvf = float2(0.0, e.y * escala.z);
    return o;
}

float4 fs_brillo(Salida i) : SV_Target0 {
    float b = luz.Sample(muestreo_luz, i.uv).r;
    float3 f = filas.Sample(muestreo_filas, i.uvf).rgb;
    return float4(tinte.rgb * b * f, 1.0);
}

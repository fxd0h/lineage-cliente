// Reemplazar el mundo: una copia de lo ya dibujado, leída con la ola (desplazamiento por columnas;
// p.z = fase, < 0 sin ola), la borrachera (por filas) y/o la visión (p.w: 1 elfo, 2 elfo oscuro).
// Convención de recursos: ver sprite.hlsl.

[[vk::binding(0, 2)]] Texture2D<float4> escena : register(t0, space2);
[[vk::binding(0, 2)]] SamplerState muestreo : register(s0, space2);
[[vk::binding(0, 3)]] cbuffer Parametros : register(b0, space3) {
    float4 p;  // ancho, alto, fase de la ola (< 0 sin ola), modo de visión
    float4 b;  // fase de la borrachera (< 0 sin ella), amplitud
};

struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

static const float2 ESQUINAS[6] = {float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 0), float2(1, 1), float2(0, 1)};

Salida vs_mundo(uint vid : SV_VertexID) {
    float2 e = ESQUINAS[vid];
    Salida o;
    o.pos = float4(e.x * 2.0 - 1.0, 1.0 - e.y * 2.0, 0.0, 1.0);
    o.uv = e;
    return o;
}

float4 fs_mundo(Salida i) : SV_Target0 {
    float2 uv = i.uv;
    if (b.x >= 0.0) {
        float fila = floor(uv.y * p.y);
        float dx = trunc(sin((fila + b.x) * 3.14159265 * 2.0 / 100.0) * b.y);
        float x = clamp(floor(uv.x * p.x) + dx, 0.0, p.x - 1.0);
        uv.x = (x + 0.5) / p.x;
    }
    if (p.z >= 0.0) {
        float xl = floor(uv.x * p.x);
        float yl = floor(uv.y * p.y);
        float dy = trunc(sin((xl + p.z) * 3.14159265 / 50.0) * 6.0);
        float y = clamp(yl + dy, 0.0, p.y - 1.0);
        uv = float2((xl + 0.5) / p.x, (y + 0.5) / p.y);
    }
    float3 c = escena.Sample(muestreo, uv).rgb;
    if (p.w == 1.0) c = c * float3(1.0 / 16.0, 0.5, 1.0 / 16.0);
    else if (p.w == 2.0) c = c * ((2.0 * c.r + 7.0 * c.g + c.b) / 10.0);
    return float4(c, 1.0);
}

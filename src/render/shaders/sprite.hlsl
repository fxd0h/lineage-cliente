// Sprites instanciados: una llamada de dibujo para todo el frame (ver render/gpu.cpp).
// Es la fuente de SPIR-V (Linux) y DXIL (Windows); en macOS se usa todavía SHADER_MSL de gpu.cpp, que
// debe hacer lo mismo.
//
// Recursos según la convención de SDL_GPU (SDL_CreateGPUShader): en el vértice, storage buffers en
// t[n]/space0 (set 0 en SPIR-V) y uniformes en b[n]/space1 (set 1); en el fragmento, texturas y
// samplers en t[n]/s[n] de space2 (set 2) y uniformes en b[n]/space3 (set 3). [[vk::binding]] da lo
// mismo para SPIR-V; glslang une textura y sampler (--auto-sampled-textures).

struct Instancia {
    float4 destino;  // x, y, ancho, alto (píxeles lógicos)
    float4 uv;       // u0, v0, u1, v1
    uint pagina;
    uint color;      // RGBA8 multiplicativo (0xAABBGGRR)
    uint mezcla;     // 1: aditiva
    uint relleno;
};

[[vk::binding(0, 0)]] StructuredBuffer<Instancia> instancias : register(t0, space0);
[[vk::binding(0, 1)]] cbuffer Uniformes : register(b0, space1) {
    float2 pantalla;
    float2 base;  // x: primera instancia de esta llamada
};

[[vk::binding(0, 2)]] Texture2DArray<float4> atlas : register(t0, space2);
[[vk::binding(0, 2)]] SamplerState muestreo : register(s0, space2);

struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    nointerpolation uint pagina : TEXCOORD2;
    nointerpolation uint mezcla : TEXCOORD3;
};

static const float2 ESQUINAS[6] = {float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 0), float2(1, 1), float2(0, 1)};

// Igual que unpack_unorm4x8_to_float de Metal: el byte bajo es x.
float4 desempacar(uint c) {
    return float4(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, c >> 24) / 255.0;
}

Salida vs_sprite(uint vid : SV_VertexID, uint iid : SV_InstanceID) {
    Instancia i = instancias[iid + uint(base.x)];
    float2 e = ESQUINAS[vid];
    float2 p = i.destino.xy + e * i.destino.zw;
    Salida o;
    o.pos = float4(p.x / pantalla.x * 2.0 - 1.0, 1.0 - p.y / pantalla.y * 2.0, 0.0, 1.0);
    o.uv = lerp(i.uv.xy, i.uv.zw, e);
    o.color = desempacar(i.color);
    o.pagina = i.pagina;
    o.mezcla = i.mezcla;
    return o;
}

float4 fs_sprite(Salida i) : SV_Target0 {
    float4 c = atlas.Sample(muestreo, float3(i.uv, i.pagina)) * i.color;
    if (c.a <= 0.0) discard;
    // Alfa premultiplicado: con alfa 0 el color se suma al fondo sin taparlo (mezcla aditiva).
    return float4(c.rgb * c.a, i.mezcla == 1 ? 0.0 : c.a);
}

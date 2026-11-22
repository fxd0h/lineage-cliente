// Instanced sprites: one draw call for the whole frame (see render/gpu.cpp).
// HLSL source compiled to DXIL for Windows/D3D12 (cmake/shaders.cmake). Linux/Vulkan uses the GLSL twins
// (sprite.vert/sprite.frag, SPIR-V embedded by herramientas/compilar_shaders.sh) and macOS/Metal the MSL in
// gpu.cpp: keep the three in sync.
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

// En MSL (spirv-cross --msl-decoration-binding) el binding es el índice de [[buffer]], y SDL_GPU pone
// primero los uniformes: buffer(0) = Uniformes, buffer(1) = este storage buffer.
#ifdef LIN_MSL
[[vk::binding(1, 0)]]
#else
[[vk::binding(0, 0)]]
#endif
StructuredBuffer<Instancia> instancias : register(t0, space0);
[[vk::binding(0, 1)]] cbuffer Uniformes : register(b0, space1) {
    float2 pantalla;
    float2 base;  // x: primera instancia de este dibujo; y: modo de pasada (0 mezclas normales, 1 mezcla screen, 2 capa de texto HD)
};

[[vk::binding(0, 2)]] Texture2DArray<float4> atlas : register(t0, space2);
[[vk::binding(0, 2)]] SamplerState muestreo : register(s0, space2);
[[vk::binding(1, 2)]] Texture2DArray<float4> atlas_lin : register(t1, space2);  // same pages, bilinear (HD images)
[[vk::binding(1, 2)]] SamplerState muestreo_lin : register(s1, space2);

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
    // Filtro de pasada: modo 0 = mezclas normales, 1 = mezcla screen (bit 3 de mezcla), 3 = mezcla de multiplicar (bit 7), 2 = capa de texto HD (texto, más toda
    // instancia no aditiva como atenuador del texto que tiene debajo). Las instancias ocultas se mueven fuera del volumen de recorte.
    const uint modo = uint(base.y + 0.5);
    const bool texto = (i.mezcla & 64u) != 0u;
    bool oculto;
    if (modo == 2u) oculto = !texto && (i.mezcla & 137u) != 0u;
    else if (modo == 1u) oculto = texto || (i.mezcla & 8u) == 0u;
    else if (modo == 3u) oculto = texto || (i.mezcla & 128u) == 0u;
    else oculto = texto || (i.mezcla & 136u) != 0u;
    if (oculto) o.pos = float4(2.0, 2.0, 2.0, 1.0);
    o.uv = lerp(i.uv.xy, i.uv.zw, e);
    o.color = desempacar(i.color);
    o.pagina = i.pagina;
    o.mezcla = i.mezcla;
    return o;
}

float4 fs_sprite(Salida i) : SV_Target0 {
    float4 t;
    if ((i.mezcla & 4u) != 0u) {
        // Bit 2 = imagen del pack HD: texels premultiplicados, bilineal; de vuelta a alfa directo para el camino común.
        t = atlas_lin.Sample(muestreo_lin, float3(i.uv, float(i.pagina)));
        t.rgb = t.a > 0.0 ? t.rgb / t.a : float3(0.0, 0.0, 0.0);
    } else {
        t = atlas.Sample(muestreo, float3(i.uv, float(i.pagina)));
    }
    // Bit 1 de mezcla = silueta: el color del tinte con el alfa de la textura. Bit 0 = aditiva.
    float4 c = (i.mezcla & 2u) != 0u ? float4(i.color.rgb, t.a * i.color.a) : t * i.color;
    // Bit 5 = gris: la luminancia de la textura por el tinte (íconos monocromos del HUD).
    if ((i.mezcla & 32u) != 0u) c.rgb = dot(t.rgb, float3(0.299, 0.587, 0.114)) * i.color.rgb;
    if (c.a <= 0.0) discard;
    // Alfa premultiplicado: con alfa 0 el color se suma al fondo sin taparlo (mezcla aditiva).
    // Bit 7 = multiplicar: la mezcla es dst * src, así que un texel transparente debe dar blanco (deja dst sin cambios).
    if ((i.mezcla & 128u) != 0u) return float4(lerp(float3(1.0, 1.0, 1.0), c.rgb, c.a), 0.0);
    return float4(c.rgb * c.a, (i.mezcla & 9u) != 0u ? 0.0 : c.a);
}

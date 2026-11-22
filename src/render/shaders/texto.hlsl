// Texto HD: el atlas guarda campos de glifos (0,5 = contorno, ver recursos/fuente_hd.h). El contorno se dibuja con un
// smoothstep en espacio de pantalla, así el borde mide un píxel de pantalla a cualquier escala. Gemelo HLSL de texto.frag (Windows/D3D12).
// Etapa de vértices: vs_sprite (sprite.hlsl). Convención de recursos: ver sprite.hlsl.

[[vk::binding(0, 2)]] Texture2DArray<float4> atlas : register(t0, space2);
[[vk::binding(0, 2)]] SamplerState muestreo : register(s0, space2);
[[vk::binding(1, 2)]] Texture2DArray<float4> atlas_lin : register(t1, space2);
[[vk::binding(1, 2)]] SamplerState muestreo_lin : register(s1, space2);

struct Salida {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : TEXCOORD1;
    nointerpolation uint pagina : TEXCOORD2;
    nointerpolation uint mezcla : TEXCOORD3;
};

// Dos destinos (ver Gpu::pase_texto): 0 = color del texto premultiplicado, 1 = cobertura en R. Ambos usan la mezcla "over"
// premultiplicada, así una instancia atenuante (da negro, alfa = su alfa) deja dst * (1 - alfa) en ambos.
struct SalidaTexto {
    float4 salida : SV_Target0;
    float4 cobertura : SV_Target1;
};

SalidaTexto fs_texto(Salida i) {
    // Las dos muestras al principio (las derivadas necesitan control de flujo uniforme); ambas leen la misma capa.
    float4 tn = atlas.Sample(muestreo, float3(i.uv, float(i.pagina)));
    float4 tl = atlas_lin.Sample(muestreo_lin, float3(i.uv, float(i.pagina)));
    float w = max((abs(ddx(tl.r)) + abs(ddy(tl.r))) * 0.7, 0.02);  // fwidth(tl.r) * 0.7
    SalidaTexto o;
    if ((i.mezcla & 64u) != 0u) {
        float a = smoothstep(0.5 - w, 0.5 + w, tl.r) * i.color.a;
        if (a <= 0.0) discard;
        o.salida = float4(i.color.rgb * a, a);
        o.cobertura = float4(a, 0.0, 0.0, a);
    } else {
        // Bit 2 = imagen del pack HD (bilineal, premultiplicada); todo otro tipo tiene alfa = alfa del texel * alfa del tinte.
        float a = ((i.mezcla & 4u) != 0u ? tl.a : tn.a) * i.color.a;
        if (a <= 0.0) discard;
        o.salida = float4(0.0, 0.0, 0.0, a);
        o.cobertura = float4(0.0, 0.0, 0.0, a);
    }
    return o;
}

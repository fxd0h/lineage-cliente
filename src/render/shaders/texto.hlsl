// HD text: the atlas holds glyph fields (0.5 = outline, see recursos/fuente_hd.h). The outline is rendered with a
// screen-space smoothstep, so the edge is one display pixel wide at any scale. HLSL twin of texto.frag (Windows/D3D12).
// Vertex stage: vs_sprite (sprite.hlsl). Resource convention: see sprite.hlsl.

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

// Two targets (see Gpu::pase_texto): 0 = premultiplied text colour, 1 = coverage in R. Both use the premultiplied "over"
// blend, so an attenuating instance (output black, alpha = its alpha) leaves dst * (1 - alpha) on both.
struct SalidaTexto {
    float4 salida : SV_Target0;
    float4 cobertura : SV_Target1;
};

SalidaTexto fs_texto(Salida i) {
    // Both samples up front (derivatives need uniform control flow); both read the same layer.
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
        // Bit 2 = HD pack image (bilinear, premultiplied); every other kind has alpha = texel alpha * tint alpha.
        float a = ((i.mezcla & 4u) != 0u ? tl.a : tn.a) * i.color.a;
        if (a <= 0.0) discard;
        o.salida = float4(0.0, 0.0, 0.0, a);
        o.cobertura = float4(0.0, 0.0, 0.0, a);
    }
    return o;
}

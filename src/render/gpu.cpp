#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include "render/gpu.h"
#include "render/shaders_msl_anime4k.h"
#include "render/shaders_spirv.h"
#ifdef _WIN32
#include "shaders.h"  // arreglos de bytes DXIL SHADER_<ENTRADA>, generados por cmake/shaders.cmake desde shaders/*.hlsl
#endif

namespace lin {

namespace {

// MULTIPLATAFORMA: SDL_GPU usa Metal en macOS y por eso este shader está en MSL. Para Vulkan
// (Linux) y D3D12 (Windows) hay que escribirlo en HLSL y compilarlo con SDL_shadercross a SPIR-V
// y DXIL, y elegir el formato con SDL_GetGPUShaderFormats(). Convención de recursos de SDL_GPU
// para MSL: buffer(0) = uniformes, buffer(1) = primer storage buffer; texture(0)/sampler(0).
// Tamaño inicial de la textura de brillo (celdas); crece si la vista lo pide (ver asegurar_luz).
constexpr int LUZ_TAM = 1024;
constexpr int FILAS_TAM = 1024;  // alto de la textura de color por fila
// Amplitud (píxeles) del vaivén de la borrachera por filas (S_Liquor 1). Igual que desplazamiento_borracho.
constexpr float AMPLITUD_BORRACHO = 3.5f;

const char* SHADER_MSL = R"(
#include <metal_stdlib>
using namespace metal;

struct Instancia { float4 destino; float4 uv; uint pagina; uint color; uint mezcla; uint relleno; };
struct Uniformes { float2 pantalla; float2 relleno; };
struct Salida {
    float4 pos [[position]];
    float2 uv;
    float4 color;
    uint pagina [[flat]];
    uint mezcla [[flat]];
};

vertex Salida vs_sprite(uint vid [[vertex_id]], uint iid [[instance_id]],
                        constant Uniformes& u [[buffer(0)]],
                        const device Instancia* inst [[buffer(1)]]) {
    const float2 esquinas[6] = { float2(0,0), float2(1,0), float2(0,1), float2(1,0), float2(1,1), float2(0,1) };
    Instancia i = inst[iid + uint(u.relleno.x)];
    float2 e = esquinas[vid];
    float2 p = i.destino.xy + e * i.destino.zw;
    Salida o;
    o.pos = float4(p.x / u.pantalla.x * 2.0 - 1.0, 1.0 - p.y / u.pantalla.y * 2.0, 0.0, 1.0);
    // relleno.y: 0 = pasada de las mezclas normales, 1 = pasada de la mezcla screen (bit 3 de mezcla), 3 = pasada de la de multiplicar (bit 7), 2 = capa de texto HD
    // (texto, más toda instancia no aditiva como atenuador del texto que tiene debajo).
    const uint modo = uint(u.relleno.y + 0.5);
    const bool texto = (i.mezcla & 64u) != 0u;
    bool oculto;
    if (modo == 2u) oculto = !texto && (i.mezcla & 137u) != 0u;
    else if (modo == 1u) oculto = texto || (i.mezcla & 8u) == 0u;
    else if (modo == 3u) oculto = texto || (i.mezcla & 128u) == 0u;
    else oculto = texto || (i.mezcla & 136u) != 0u;
    if (oculto) o.pos = float4(2.0, 2.0, 2.0, 1.0);
    o.uv = mix(i.uv.xy, i.uv.zw, e);
    o.color = unpack_unorm4x8_to_float(i.color);
    o.pagina = i.pagina;
    o.mezcla = i.mezcla;
    return o;
}

fragment float4 fs_sprite(Salida in [[stage_in]], texture2d_array<float> atlas [[texture(0)]],
                          texture2d_array<float> atlas_lin [[texture(1)]],
                          sampler s [[sampler(0)]], sampler s_lin [[sampler(1)]]) {
    float4 t;
    if ((in.mezcla & 4) != 0) {
        // Imagen del pack HD: texels premultiplicados, bilineal; de vuelta a alfa directo para el camino común.
        t = atlas_lin.sample(s_lin, in.uv, in.pagina);
        t.rgb = t.a > 0.0 ? t.rgb / t.a : float3(0.0);
    } else {
        t = atlas.sample(s, in.uv, in.pagina);
    }
    // Bit 1 de mezcla = silueta: el color del tinte con el alfa de la textura. Bit 0 = aditiva.
    float4 c = (in.mezcla & 2) != 0 ? float4(in.color.rgb, t.a * in.color.a) : t * in.color;
    // Bit 5 = gris: la luminancia de la textura por el tinte (íconos monocromos del HUD).
    if ((in.mezcla & 32) != 0) c.rgb = float3(dot(t.rgb, float3(0.299, 0.587, 0.114))) * in.color.rgb;
    if (c.a <= 0.0) discard_fragment();
    // Alfa premultiplicado: con alfa 0 el color se suma al fondo sin taparlo (mezcla aditiva).
    // Bit 7 = multiply: the blend is dst * src, so a transparent texel must output white (leaves dst unchanged).
    if ((in.mezcla & 128) != 0) return float4(mix(float3(1.0), c.rgb, c.a), 0.0);
    return float4(c.rgb * c.a, (in.mezcla & 9) != 0 ? 0.0 : c.a);
}

// Capa de texto HD: el atlas guarda campos de glifos (0,5 = contorno, ver recursos/fuente_hd.h), umbralizados con un
// smoothstep en espacio de pantalla para que el borde mida un píxel de pantalla a cualquier escala. Toda otra instancia solo atenúa la
// capa (negro, alfa = su alfa; la mezcla "over" premultiplicada deja dst * (1 - alfa)). Dos destinos: 0 = color del
// texto (premultiplicado), 1 = cobertura en R. Gemelo de shaders/texto.frag.
struct SalidaTexto {
    float4 color [[color(0)]];
    float4 cobertura [[color(1)]];
};
fragment SalidaTexto fs_texto(Salida in [[stage_in]], texture2d_array<float> atlas [[texture(0)]],
                              texture2d_array<float> atlas_lin [[texture(1)]],
                              sampler s [[sampler(0)]], sampler s_lin [[sampler(1)]]) {
    float4 tn = atlas.sample(s, in.uv, in.pagina);
    float4 tl = atlas_lin.sample(s_lin, in.uv, in.pagina);
    float w = max(fwidth(tl.r) * 0.7, 0.02);
    SalidaTexto o;
    if ((in.mezcla & 64u) != 0u) {
        float a = smoothstep(0.5 - w, 0.5 + w, tl.r) * in.color.a;
        if (a <= 0.0) discard_fragment();
        o.color = float4(in.color.rgb * a, a);
        o.cobertura = float4(a, 0.0, 0.0, a);
    } else {
        float a = ((in.mezcla & 4u) != 0u ? tl.a : tn.a) * in.color.a;
        if (a <= 0.0) discard_fragment();
        o.color = float4(0.0, 0.0, 0.0, a);
        o.cobertura = float4(0.0, 0.0, 0.0, a);
    }
    return o;
}

// Multiplicar el mundo: brillo (textura R8 por celda) × color por fila (textura de 1 × N) × tinte.
struct BrilloSalida {
    float4 pos [[position]];
    float2 uv;
    float2 uvf;
};

vertex BrilloSalida vs_brillo(uint vid [[vertex_id]], constant float4& escala [[buffer(0)]]) {
    const float2 esquinas[6] = { float2(0,0), float2(1,0), float2(0,1), float2(1,0), float2(1,1), float2(0,1) };
    float2 e = esquinas[vid];
    BrilloSalida o;
    o.pos = float4(e.x * 2.0 - 1.0, 1.0 - e.y * 2.0, 0.0, 1.0);
    o.uv = e * escala.xy;
    o.uvf = float2(0.0, e.y * escala.z);
    return o;
}

fragment float4 fs_brillo(BrilloSalida in [[stage_in]], texture2d<float> luz [[texture(0)]],
                          texture2d<float> filas [[texture(1)]], sampler s0 [[sampler(0)]],
                          sampler s1 [[sampler(1)]], constant float4& tinte [[buffer(0)]]) {
    float b = luz.sample(s0, in.uv).r;
    float3 f = filas.sample(s1, in.uvf).rgb;
    return float4(tinte.rgb * b * f, 1.0);
}

// Reemplazar el mundo: una copia de lo ya dibujado, leída con la ola (desplazamiento por columnas;
// p.z = fase, < 0 sin ola) y/o la visión (p.w: 1 elfo, 2 elfo oscuro).
struct MundoSalida {
    float4 pos [[position]];
    float2 uv;
};

vertex MundoSalida vs_mundo(uint vid [[vertex_id]]) {
    const float2 esquinas[6] = { float2(0,0), float2(1,0), float2(0,1), float2(1,0), float2(1,1), float2(0,1) };
    float2 e = esquinas[vid];
    MundoSalida o;
    o.pos = float4(e.x * 2.0 - 1.0, 1.0 - e.y * 2.0, 0.0, 1.0);
    o.uv = e;
    return o;
}

fragment float4 fs_mundo(MundoSalida in [[stage_in]], texture2d<float> escena [[texture(0)]],
                         sampler s [[sampler(0)]], constant float4* u [[buffer(0)]]) {
    float4 p = u[0];  // ancho, alto, fase de la ola (< 0 sin ola), modo de visión
    float4 b = u[1];  // fase de la borrachera (< 0 sin ella), amplitud
    float2 uv = in.uv;
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
    float3 c = escena.sample(s, uv).rgb;
    if (p.w == 1.0) c = c * float3(1.0 / 16.0, 0.5, 1.0 / 16.0);
    else if (p.w == 2.0) c = c * ((2.0 * c.r + 7.0 * c.g + c.b) / 10.0);
    return float4(c, 1.0);
}

// Composición del texto HD: capa de color (textura 0) y capa de cobertura (textura 1, R) sobre el destino con la
// mezcla "over" premultiplicada. Etapa de vértices vs_mundo. Gemelo de shaders/texto_comp.frag.
fragment float4 fs_texto_comp(MundoSalida in [[stage_in]], texture2d<float> capa [[texture(0)]],
                              texture2d<float> cob [[texture(1)]], sampler s [[sampler(0)]],
                              sampler s2 [[sampler(1)]]) {
    return float4(capa.sample(s, in.uv).rgb, cob.sample(s2, in.uv).r);
}

// Escaladores de la presentación final (gemelos de src/render/shaders/escalado_*.frag; la etapa de vértices es vs_mundo).
// u.xy = tamaño de origen, u.zw = tamaño de salida. SIN PROBAR en Metal (no había Mac): solo plausible de compilar.
float3 esc_P(texture2d<float> t, sampler s, float4 u, float2 p) {
    return t.sample(s, (clamp(p, float2(0.0), u.xy - 1.0) + 0.5) / u.xy).rgb;
}
float esc_d(float3 a, float3 b) { return dot(abs(a - b), float3(0.299, 0.587, 0.114)); }

fragment float4 fs_escalado_xbr(MundoSalida in [[stage_in]], texture2d<float> escena [[texture(0)]],
                                sampler sm [[sampler(0)]], constant float4& u [[buffer(0)]]) {
    float2 sp = in.uv * u.xy;
    float2 c = floor(sp);
    float2 f = sp - c - 0.5;
    float2 s = float2(f.x < 0.0 ? -1.0 : 1.0, f.y < 0.0 ? -1.0 : 1.0);
    float3 E = esc_P(escena, sm, u, c);
    float3 H = esc_P(escena, sm, u, c + float2(s.x, 0.0));
    float3 V = esc_P(escena, sm, u, c + float2(0.0, s.y));
    float3 I = esc_P(escena, sm, u, c + s);
    float3 Hb = esc_P(escena, sm, u, c + float2(-s.x, 0.0));
    float3 Vb = esc_P(escena, sm, u, c + float2(0.0, -s.y));
    float3 C = esc_P(escena, sm, u, c + float2(s.x, -s.y));
    float3 G = esc_P(escena, sm, u, c + float2(-s.x, s.y));
    float3 H2 = esc_P(escena, sm, u, c + float2(2.0 * s.x, s.y));
    float3 V2 = esc_P(escena, sm, u, c + float2(s.x, 2.0 * s.y));
    float e = esc_d(E, C) + esc_d(E, G) + esc_d(I, H2) + esc_d(I, V2) + 4.0 * esc_d(V, H);
    float i = esc_d(V, Hb) + esc_d(V, V2) + esc_d(H, H2) + esc_d(H, Vb) + 4.0 * esc_d(E, I);
    float3 col = E;
    if (e < i && esc_d(E, I) > 0.02) {
        float3 n = esc_d(E, H) <= esc_d(E, V) ? H : V;
        float t = abs(f.x) + abs(f.y);
        float w = 0.5 * length(u.xy / u.zw) * 1.5;
        float a = smoothstep(0.5 - w, 0.5 + w, t);
        col = mix(E, n, a);
    }
    return float4(col, 1.0);
}

// FSR 1.0 EASU, Copyright (c) 2021 Advanced Micro Devices, Inc., MIT License (see escalado_fsr.frag).
float esc_L(float3 c) { return c.b * 0.5 + (c.r * 0.5 + c.g); }
void esc_tap(thread float3& aC, thread float& aW, float2 off, float2 dir, float2 len, float lob, float clp, float3 c) {
    float2 v = float2(off.x * dir.x + off.y * dir.y, off.x * (-dir.y) + off.y * dir.x);
    v *= len;
    float d2 = min(v.x * v.x + v.y * v.y, clp);
    float wB = 2.0 / 5.0 * d2 - 1.0;
    float wA = lob * d2 - 1.0;
    wB *= wB;
    wA *= wA;
    wB = 25.0 / 16.0 * wB - (25.0 / 16.0 - 1.0);
    float w = wB * wA;
    aC += c * w;
    aW += w;
}
void esc_set(thread float2& dir, thread float& len, float w, float lA, float lB, float lC, float lD, float lE) {
    float lenX = max(abs(lD - lC), abs(lC - lB));
    float dirX = lD - lB;
    dir.x += dirX * w;
    lenX = clamp(abs(dirX) / lenX, 0.0, 1.0);
    len += lenX * lenX * w;
    float lenY = max(abs(lE - lC), abs(lC - lA));
    float dirY = lE - lA;
    dir.y += dirY * w;
    lenY = clamp(abs(dirY) / lenY, 0.0, 1.0);
    len += lenY * lenY * w;
}
fragment float4 fs_escalado_fsr(MundoSalida in [[stage_in]], texture2d<float> escena [[texture(0)]],
                                sampler sm [[sampler(0)]], constant float4& u [[buffer(0)]]) {
    float2 pp = in.uv * u.xy - 0.5;
    float2 fp = floor(pp);
    pp -= fp;
    float3 b = esc_P(escena, sm, u, fp + float2(0, -1)), c = esc_P(escena, sm, u, fp + float2(1, -1));
    float3 e = esc_P(escena, sm, u, fp + float2(-1, 0)), f = esc_P(escena, sm, u, fp);
    float3 g = esc_P(escena, sm, u, fp + float2(1, 0)), h = esc_P(escena, sm, u, fp + float2(2, 0));
    float3 i = esc_P(escena, sm, u, fp + float2(-1, 1)), j = esc_P(escena, sm, u, fp + float2(0, 1));
    float3 k = esc_P(escena, sm, u, fp + float2(1, 1)), l = esc_P(escena, sm, u, fp + float2(2, 1));
    float3 n = esc_P(escena, sm, u, fp + float2(0, 2)), o = esc_P(escena, sm, u, fp + float2(1, 2));
    float bL = esc_L(b), cL = esc_L(c), eL = esc_L(e), fL = esc_L(f), gL = esc_L(g), hL = esc_L(h);
    float iL = esc_L(i), jL = esc_L(j), kL = esc_L(k), lL = esc_L(l), nL = esc_L(n), oL = esc_L(o);
    float2 dir = float2(0.0);
    float len = 0.0;
    esc_set(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bL, eL, fL, gL, jL);
    esc_set(dir, len, pp.x * (1.0 - pp.y), cL, fL, gL, hL, kL);
    esc_set(dir, len, (1.0 - pp.x) * pp.y, fL, iL, jL, kL, nL);
    esc_set(dir, len, pp.x * pp.y, gL, jL, kL, lL, oL);
    float2 dir2 = dir * dir;
    float dirR = dir2.x + dir2.y;
    bool zro = dirR < (1.0 / 32768.0);
    dirR = rsqrt(dirR);
    dirR = zro ? 1.0 : dirR;
    dir.x = zro ? 1.0 : dir.x;
    dir *= float2(dirR);
    len = len * 0.5;
    len *= len;
    float stretch = (dir.x * dir.x + dir.y * dir.y) / max(abs(dir.x), abs(dir.y));
    float2 len2 = float2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
    float clp = 1.0 / lob;
    float3 min4 = min(min(f, g), min(j, k));
    float3 max4 = max(max(f, g), max(j, k));
    float3 aC = float3(0.0);
    float aW = 0.0;
    esc_tap(aC, aW, float2(0, -1) - pp, dir, len2, lob, clp, b);
    esc_tap(aC, aW, float2(1, -1) - pp, dir, len2, lob, clp, c);
    esc_tap(aC, aW, float2(-1, 1) - pp, dir, len2, lob, clp, i);
    esc_tap(aC, aW, float2(0, 1) - pp, dir, len2, lob, clp, j);
    esc_tap(aC, aW, float2(0, 0) - pp, dir, len2, lob, clp, f);
    esc_tap(aC, aW, float2(-1, 0) - pp, dir, len2, lob, clp, e);
    esc_tap(aC, aW, float2(1, 1) - pp, dir, len2, lob, clp, k);
    esc_tap(aC, aW, float2(2, 1) - pp, dir, len2, lob, clp, l);
    esc_tap(aC, aW, float2(2, 0) - pp, dir, len2, lob, clp, h);
    esc_tap(aC, aW, float2(1, 0) - pp, dir, len2, lob, clp, g);
    esc_tap(aC, aW, float2(1, 2) - pp, dir, len2, lob, clp, o);
    esc_tap(aC, aW, float2(0, 2) - pp, dir, len2, lob, clp, n);
    return float4(min(max4, max(min4, aC / aW)), 1.0);
}

fragment float4 fs_escalado_crt(MundoSalida in [[stage_in]], texture2d<float> escena [[texture(0)]],
                                sampler sm [[sampler(0)]], constant float4& u [[buffer(0)]]) {
    float2 sp = in.uv * u.xy;
    float2 c = floor(sp);
    float2 f = sp - c;
    float nx = f.x < 0.5 ? -1.0 : 1.0;
    float k = abs(f.x - 0.5);
    float3 col = mix(esc_P(escena, sm, u, c), esc_P(escena, sm, u, c + float2(nx, 0.0)), smoothstep(0.0, 1.0, k * 0.8));
    float scan = 0.82 + 0.18 * cos(6.2831853 * (f.y - 0.25));
    int ox = int(in.uv.x * u.z);
    float3 mask = float3(0.8);
    mask[ox % 3] = 1.0;
    return float4(clamp(col * scan * mask * 1.12, 0.0, 1.0), 1.0);
}
)";

}  // namespace

bool Gpu::iniciar(SDL_Window* ventana, int ancho_logico, int alto_logico) {
    ventana_ = ventana;
    ancho_ = ancho_min_ = ancho_logico;
    alto_ = alto_min_ = alto_logico;
    grid_w_ = ancho_logico;
    grid_h_ = alto_logico;
    // MSL en macOS (Metal), SPIR-V en Linux (Vulkan), DXIL en Windows (D3D12; ahí el SPIR-V se sigue embebiendo para Vulkan).
    SDL_GPUShaderFormat pedidos = SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV;
#ifdef _WIN32
    pedidos |= SDL_GPU_SHADERFORMAT_DXIL;
#endif
    dev_ = SDL_CreateGPUDevice(pedidos, false, nullptr);
    if (!dev_) {
        LIN_LOG("SDL_CreateGPUDevice: %s", SDL_GetError());
        return false;
    }
    {
        const SDL_GPUShaderFormat f = SDL_GetGPUShaderFormats(dev_);
        usar_spirv_ = !(f & SDL_GPU_SHADERFORMAT_MSL);
        // D3D12 informa solo DXIL; un dispositivo Vulkan en Windows conserva SPIR-V.
        usar_dxil_ = usar_spirv_ && !(f & SDL_GPU_SHADERFORMAT_SPIRV) && (f & SDL_GPU_SHADERFORMAT_DXIL);
        if (usar_dxil_) usar_spirv_ = false;
    }
    {
        // Qué adaptador y driver eligió SDL (en D3D12, "Microsoft Basic Render Driver" es WARP, por software).
        const SDL_PropertiesID pr = SDL_GetGPUDeviceProperties(dev_);
        LIN_LOG("GPU: %s (%s), %s (driver %s %s)", SDL_GetGPUDeviceDriver(dev_), usar_dxil_ ? "DXIL" : usar_spirv_ ? "SPIR-V" : "MSL",
                SDL_GetStringProperty(pr, SDL_PROP_GPU_DEVICE_NAME_STRING, "?"),
                SDL_GetStringProperty(pr, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, "?"),
                SDL_GetStringProperty(pr, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, ""));
    }
    if (!SDL_ClaimWindowForGPUDevice(dev_, ventana_)) {
        LIN_LOG("SDL_ClaimWindowForGPUDevice: %s", SDL_GetError());
        return false;
    }
    // Se prefiere MAILBOX (sin tearing, pero nunca bloquea): con VSYNC (FIFO) en Wayland el bucle de cuadros se traba
    // mientras la ventana está oculta o tapada, lo que también traba el socket de control en vivo.
    // LIN_VSYNC=0 prefiere IMMEDIATE (sin esperar el refresco) para medir lo que da la GPU sin el tope del monitor.
    SDL_GPUPresentMode modo = SDL_WindowSupportsGPUPresentMode(dev_, ventana_, SDL_GPU_PRESENTMODE_MAILBOX)
                                  ? SDL_GPU_PRESENTMODE_MAILBOX
                                  : SDL_GPU_PRESENTMODE_VSYNC;
    if (const char* v = std::getenv("LIN_VSYNC"); v && v[0] == '0') {
        if (SDL_WindowSupportsGPUPresentMode(dev_, ventana_, SDL_GPU_PRESENTMODE_IMMEDIATE)) modo = SDL_GPU_PRESENTMODE_IMMEDIATE;
        LIN_LOG("LIN_VSYNC=0: present mode %s", modo == SDL_GPU_PRESENTMODE_IMMEDIATE ? "immediate" : "unchanged (no immediate)");
    }
    SDL_SetGPUSwapchainParameters(dev_, ventana_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, modo);
    mailbox_ = modo == SDL_GPU_PRESENTMODE_MAILBOX;
    if (!crear_pipeline()) return false;

    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = TAM_PAGINA;
    ti.height = TAM_PAGINA;
    ti.layer_count_or_depth = PAGINAS_ATLAS;
    ti.num_levels = 1;
    atlas_ = SDL_CreateGPUTexture(dev_, &ti);

    ajustar_vista(false, 0);
    crear_destino();

    SDL_GPUSamplerCreateInfo si{};
    si.min_filter = SDL_GPU_FILTER_NEAREST;
    si.mag_filter = SDL_GPU_FILTER_NEAREST;
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    muestreo_ = SDL_CreateGPUSampler(dev_, &si);
    si.min_filter = si.mag_filter = SDL_GPU_FILTER_LINEAR;
    muestreo_lineal_ = SDL_CreateGPUSampler(dev_, &si);

    SDL_GPUBufferCreateInfo bi{};
    bi.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    bi.size = MAX_INSTANCIAS * sizeof(Instancia);
    instancias_gpu_ = SDL_CreateGPUBuffer(dev_, &bi);

    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = MAX_INSTANCIAS * sizeof(Instancia);
    subida_inst_ = SDL_CreateGPUTransferBuffer(dev_, &tb);
    tb.size = TAM_SUBIDA_ATLAS;
    subida_atlas_ = SDL_CreateGPUTransferBuffer(dev_, &tb);
    SDL_GPUTextureCreateInfo tl{};
    tl.type = SDL_GPU_TEXTURETYPE_2D;
    tl.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    tl.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tl.width = tl.height = LUZ_TAM;
    tl.layer_count_or_depth = 1;
    tl.num_levels = 1;
    luz_ = SDL_CreateGPUTexture(dev_, &tl);
    tb.size = LUZ_TAM * LUZ_TAM;
    subida_luz_ = SDL_CreateGPUTransferBuffer(dev_, &tb);
    luz_w_ = luz_h_ = LUZ_TAM;
    brillo_buf_.reserve(LUZ_TAM * LUZ_TAM);
    brillo_filas_.reserve(FILAS_TAM);
    tl.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;  // color por fila: RGBA, no R8 como la de brillo
    tl.width = 1;
    tl.height = FILAS_TAM;
    filas_tex_ = SDL_CreateGPUTexture(dev_, &tl);
    tb.size = FILAS_TAM * 4;
    subida_filas_ = SDL_CreateGPUTransferBuffer(dev_, &tb);
    if (!atlas_ || !destino_ || !muestreo_ || !muestreo_lineal_ || !instancias_gpu_ || !subida_inst_ || !subida_atlas_ || !luz_ ||
        !subida_luz_ || !filas_tex_ || !subida_filas_) {
        LIN_LOG("Recursos GPU: %s", SDL_GetError());
        return false;
    }

    // Para probar el reemplazo: menos páginas, o solo un rincón de cada una.
    const char* paginas = std::getenv("LIN_ATLAS_PAGINAS");
    const char* tam = std::getenv("LIN_ATLAS_TAM");
    if (paginas || tam)
        empaquetado_ = EmpaquetadoAtlas(static_cast<u16>(tam ? std::clamp(std::atoi(tam), 256, TAM_PAGINA) : TAM_PAGINA),
                                        static_cast<u16>(paginas ? std::clamp(std::atoi(paginas), 2, PAGINAS_ATLAS) : PAGINAS_ATLAS));
    inst_.reserve(MAX_INSTANCIAS);
    claves_.reserve(MAX_INSTANCIAS);
    orden_.reserve(MAX_INSTANCIAS);
    orden_tmp_.reserve(MAX_INSTANCIAS);
    inst_ordenadas_.reserve(MAX_INSTANCIAS);
    staging_atlas_.reserve(TAM_SUBIDA_ATLAS);

    ImagenRGBA b;
    b.preparar(2, 2);
    for (u32& p : b.px) p = BLANCO;
    subir(b, blanco_);
    return true;
}

bool Gpu::crear_pipeline() {
    // Con SPIR-V cada etapa es un módulo propio (punto de entrada "main"); con MSL todas comparten
    // SHADER_MSL y se elige por nombre.
    auto crear = [this](SDL_GPUShaderCreateInfo ci) -> SDL_GPUShader* {
#ifdef _WIN32
        if (usar_dxil_) {
            struct Modulo { const char* nombre; const unsigned char* code; size_t size; };
#define LIN_DXIL(n, A) {n, A, sizeof A}
            static const Modulo modulos[] = {
                LIN_DXIL("vs_sprite", SHADER_VS_SPRITE),
                LIN_DXIL("fs_sprite", SHADER_FS_SPRITE),
                LIN_DXIL("fs_texto", SHADER_FS_TEXTO),
                LIN_DXIL("fs_texto_comp", SHADER_FS_TEXTO_COMP),
                LIN_DXIL("vs_brillo", SHADER_VS_BRILLO),
                LIN_DXIL("fs_brillo", SHADER_FS_BRILLO),
                LIN_DXIL("vs_mundo", SHADER_VS_MUNDO),
                LIN_DXIL("fs_mundo", SHADER_FS_MUNDO),
                LIN_DXIL("fs_escalado_xbr", SHADER_FS_ESCALADO_XBR),
                LIN_DXIL("fs_escalado_fsr", SHADER_FS_ESCALADO_FSR),
                LIN_DXIL("fs_escalado_crt", SHADER_FS_ESCALADO_CRT),
                LIN_DXIL("fs_anime4k_c1", SHADER_FS_ANIME4K_C1),
                LIN_DXIL("fs_anime4k_c2", SHADER_FS_ANIME4K_C2),
                LIN_DXIL("fs_anime4k_c3", SHADER_FS_ANIME4K_C3),
                LIN_DXIL("fs_anime4k_c4", SHADER_FS_ANIME4K_C4),
                LIN_DXIL("fs_anime4k_d2s", SHADER_FS_ANIME4K_D2S),
                LIN_DXIL("fs_anime4k_resample", SHADER_FS_ANIME4K_RESAMPLE),
            };
#undef LIN_DXIL
            const Modulo* m = nullptr;
            for (const Modulo& x : modulos)
                if (std::strcmp(x.nombre, ci.entrypoint) == 0) m = &x;
            if (!m) return nullptr;
            ci.code = m->code;
            ci.code_size = m->size;
            ci.format = SDL_GPU_SHADERFORMAT_DXIL;  // el nombre del punto de entrada se mantiene: dxc se corrió con -E <nombre>
            return SDL_CreateGPUShader(dev_, &ci);
        }
#endif
        if (usar_spirv_) {
            struct Modulo { const char* nombre; const unsigned char* code; size_t size; };
            static const Modulo modulos[] = {
                {"vs_sprite", spirv::sprite_vert, sizeof spirv::sprite_vert},
                {"fs_sprite", spirv::sprite_frag, sizeof spirv::sprite_frag},
                {"fs_texto", spirv::texto_frag, sizeof spirv::texto_frag},
                {"fs_texto_comp", spirv::texto_comp_frag, sizeof spirv::texto_comp_frag},
                {"vs_brillo", spirv::brillo_vert, sizeof spirv::brillo_vert},
                {"fs_brillo", spirv::brillo_frag, sizeof spirv::brillo_frag},
                {"vs_mundo", spirv::mundo_vert, sizeof spirv::mundo_vert},
                {"fs_mundo", spirv::mundo_frag, sizeof spirv::mundo_frag},
                {"fs_escalado_xbr", spirv::escalado_xbr_frag, sizeof spirv::escalado_xbr_frag},
                {"fs_escalado_fsr", spirv::escalado_fsr_frag, sizeof spirv::escalado_fsr_frag},
                {"fs_escalado_crt", spirv::escalado_crt_frag, sizeof spirv::escalado_crt_frag},
                {"fs_anime4k_c1", spirv::anime4k_c1_frag, sizeof spirv::anime4k_c1_frag},
                {"fs_anime4k_c2", spirv::anime4k_c2_frag, sizeof spirv::anime4k_c2_frag},
                {"fs_anime4k_c3", spirv::anime4k_c3_frag, sizeof spirv::anime4k_c3_frag},
                {"fs_anime4k_c4", spirv::anime4k_c4_frag, sizeof spirv::anime4k_c4_frag},
                {"fs_anime4k_d2s", spirv::anime4k_d2s_frag, sizeof spirv::anime4k_d2s_frag},
                {"fs_anime4k_resample", spirv::anime4k_resample_frag, sizeof spirv::anime4k_resample_frag},
            };
            const Modulo* m = nullptr;
            for (const Modulo& x : modulos)
                if (std::strcmp(x.nombre, ci.entrypoint) == 0) m = &x;
            if (!m) return nullptr;
            ci.code = m->code;
            ci.code_size = m->size;
            ci.entrypoint = "main";
            ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
        }
        return SDL_CreateGPUShader(dev_, &ci);
    };
    SDL_GPUShaderCreateInfo vs{};
    vs.code = reinterpret_cast<const Uint8*>(SHADER_MSL);
    vs.code_size = std::strlen(SHADER_MSL);
    vs.entrypoint = "vs_sprite";
    vs.format = SDL_GPU_SHADERFORMAT_MSL;
    vs.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vs.num_uniform_buffers = 1;
    vs.num_storage_buffers = 1;
    SDL_GPUShader* v = crear(vs);
    SDL_GPUShaderCreateInfo fs = vs;
    fs.entrypoint = "fs_sprite";
    fs.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fs.num_uniform_buffers = 0;
    fs.num_storage_buffers = 0;
    fs.num_samplers = 2;  // atlas nearest + atlas linear (HD images)
    SDL_GPUShader* f = crear(fs);
    if (!v || !f) {
        LIN_LOG("SDL_CreateGPUShader: %s", SDL_GetError());
        return false;
    }
    SDL_GPUColorTargetDescription ct{};
    ct.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ct.blend_state.enable_blend = true;
    // Alfa premultiplicado (el shader multiplica): una sola mezcla sirve para la normal y la
    // aditiva, y todo sigue en una llamada de dibujo.
    ct.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    ct.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    ct.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    ct.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    SDL_GPUGraphicsPipelineCreateInfo pi{};
    pi.vertex_shader = v;
    pi.fragment_shader = f;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.target_info.color_target_descriptions = &ct;
    pi.target_info.num_color_targets = 1;
    pipeline_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
    {
        SDL_GPUShaderCreateInfo tf = fs;
        tf.entrypoint = "fs_texto";
        if (SDL_GPUShader* ft = crear(tf)) {
            crear_pipelines_texto(pi, ft);
            SDL_ReleaseGPUShader(dev_, ft);
        }
    }
    if (!pipeline_) LIN_LOG("SDL_CreateGPUGraphicsPipeline: %s", SDL_GetError());
    // Mezcla screen: src + dst * (1 - src), por canal (src = el color premultiplicado del shader).
    ct.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
    pipeline_pantalla_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
    if (!pipeline_pantalla_) LIN_LOG("SDL_CreateGPUGraphicsPipeline (pantalla): %s", SDL_GetError());
    // Mezcla de multiplicar: dst * src (el shader da blanco para los texels transparentes); el alfa del destino no se toca.
    ct.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
    ct.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    ct.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    ct.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    pipeline_mult_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
    if (!pipeline_mult_) LIN_LOG("SDL_CreateGPUGraphicsPipeline (multiplicar): %s", SDL_GetError());
    // Se liberan recién ahora: `pi` todavía apunta a ellos para el pipeline de la mezcla screen (Metal libera la función de inmediato).
    SDL_ReleaseGPUShader(dev_, v);
    SDL_ReleaseGPUShader(dev_, f);

    SDL_GPUShaderCreateInfo bv = vs;
    bv.entrypoint = "vs_brillo";
    bv.num_storage_buffers = 0;
    SDL_GPUShaderCreateInfo bf = fs;
    bf.entrypoint = "fs_brillo";
    bf.num_uniform_buffers = 1;
    bf.num_samplers = 2;
    SDL_GPUShader* bvs = crear(bv);
    SDL_GPUShader* bfs = crear(bf);
    if (!bvs || !bfs) {
        LIN_LOG("SDL_CreateGPUShader (brillo): %s", SDL_GetError());
        return false;
    }
    SDL_GPUColorTargetDescription mt{};
    mt.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    mt.blend_state.enable_blend = true;
    // destino = destino * fuente: multiplicar, sin cambiar el alfa del destino.
    mt.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    mt.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_COLOR;
    mt.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    mt.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    mt.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    mt.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    SDL_GPUGraphicsPipelineCreateInfo mi{};
    mi.vertex_shader = bvs;
    mi.fragment_shader = bfs;
    mi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    mi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    mi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    mi.target_info.color_target_descriptions = &mt;
    mi.target_info.num_color_targets = 1;
    pipeline_brillo_ = SDL_CreateGPUGraphicsPipeline(dev_, &mi);
    SDL_ReleaseGPUShader(dev_, bvs);
    SDL_ReleaseGPUShader(dev_, bfs);
    if (!pipeline_brillo_) LIN_LOG("SDL_CreateGPUGraphicsPipeline (brillo): %s", SDL_GetError());
    // Reemplazar el mundo (ola y visión): sin mezcla, escribe la copia transformada.
    SDL_GPUShaderCreateInfo mv = vs;
    mv.entrypoint = "vs_mundo";
    mv.num_uniform_buffers = 0;
    mv.num_storage_buffers = 0;
    SDL_GPUShaderCreateInfo mf = fs;
    mf.entrypoint = "fs_mundo";
    mf.num_uniform_buffers = 1;
    mf.num_samplers = 1;
    mf.num_storage_buffers = 0;
    SDL_GPUShader* mvs = crear(mv);
    SDL_GPUShader* mfs = crear(mf);
    if (!mvs || !mfs) {
        LIN_LOG("SDL_CreateGPUShader (mundo): %s", SDL_GetError());
        return false;
    }
    SDL_GPUColorTargetDescription ctm{};
    ctm.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUGraphicsPipelineCreateInfo mpi{};
    mpi.vertex_shader = mvs;
    mpi.fragment_shader = mfs;
    mpi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    mpi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    mpi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    mpi.target_info.color_target_descriptions = &ctm;
    mpi.target_info.num_color_targets = 1;
    pipeline_mundo_ = SDL_CreateGPUGraphicsPipeline(dev_, &mpi);
    SDL_ReleaseGPUShader(dev_, mvs);
    SDL_ReleaseGPUShader(dev_, mfs);
    if (!pipeline_mundo_) LIN_LOG("SDL_CreateGPUGraphicsPipeline (mundo): %s", SDL_GetError());
    // Composición del texto HD (quad de vs_mundo + fs_texto_comp): la capa de texto sobre un destino, premultiplicada. [0] para las
    // texturas RGBA8, [1] para el formato del swapchain.
    {
        SDL_GPUShaderCreateInfo cf = mf;
        cf.entrypoint = "fs_texto_comp";
        cf.num_uniform_buffers = 0;
        cf.num_samplers = 2;
        SDL_GPUShader* cvs = crear(mv);
        SDL_GPUShader* cfs = crear(cf);
        if (cvs && cfs) {
            const SDL_GPUTextureFormat formatos[2] = {SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                                       SDL_GetGPUSwapchainTextureFormat(dev_, ventana_)};
            for (int k = 0; k < 2; ++k) {
                SDL_GPUColorTargetDescription cc{};
                cc.format = formatos[k];
                cc.blend_state.enable_blend = true;
                cc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                cc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
                cc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
                cc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                cc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                cc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
                SDL_GPUGraphicsPipelineCreateInfo cpi = mpi;
                cpi.vertex_shader = cvs;
                cpi.fragment_shader = cfs;
                cpi.target_info.color_target_descriptions = &cc;
                pipe_comp_[k] = SDL_CreateGPUGraphicsPipeline(dev_, &cpi);
                if (!pipe_comp_[k]) LIN_LOG("HD text composite pipeline: %s", SDL_GetError());
            }
        }
        SDL_ReleaseGPUShader(dev_, cvs);
        SDL_ReleaseGPUShader(dev_, cfs);
    }
    // Filtros de la presentación final: vs_mundo (quad de pantalla completa) + un fragment shader cada uno, sin mezcla.
    for (int k = 1; k < static_cast<int>(Escalado::ANIME4K); ++k) {
        const char* nombres[N_ESCALADOS] = {"", "fs_escalado_xbr", "fs_escalado_fsr", "fs_escalado_crt", ""};
        SDL_GPUShaderCreateInfo ev = mv;
        SDL_GPUShaderCreateInfo ef = mf;
        ef.entrypoint = nombres[k];
        SDL_GPUShader* evs = crear(ev);
        SDL_GPUShader* efs = crear(ef);
        if (evs && efs) {
            SDL_GPUGraphicsPipelineCreateInfo epi = mpi;
            epi.vertex_shader = evs;
            epi.fragment_shader = efs;
            pipeline_esc_[k] = SDL_CreateGPUGraphicsPipeline(dev_, &epi);
        }
        if (!pipeline_esc_[k]) LIN_LOG("Escalado %s: %s", nombres[k], SDL_GetError());
        SDL_ReleaseGPUShader(dev_, evs);
        SDL_ReleaseGPUShader(dev_, efs);
    }
    // Pasadas de Anime4K: conv1..4 escriben intermedios RGBA16F, depth-to-space y resample escriben RGBA8.
    {
        static const char* nombres[A4K_PASES] = {"fs_anime4k_c1", "fs_anime4k_c2", "fs_anime4k_c3",
                                                 "fs_anime4k_c4", "fs_anime4k_d2s", "fs_anime4k_resample"};
        static const int muestras[A4K_PASES] = {1, 1, 1, 1, 2, 1};
        for (int k = 0; k < A4K_PASES; ++k) {
            SDL_GPUShaderCreateInfo ev = mv;
            SDL_GPUShaderCreateInfo ef = mf;
            ef.entrypoint = nombres[k];
            ef.code = reinterpret_cast<const Uint8*>(SHADER_MSL_ANIME4K);
            ef.code_size = std::strlen(SHADER_MSL_ANIME4K);
            ef.num_samplers = static_cast<Uint32>(muestras[k]);
            ef.num_uniform_buffers = k == 5 ? 0 : 1;
            SDL_GPUShader* evs = crear(ev);
            SDL_GPUShader* efs = crear(ef);
            if (evs && efs) {
                SDL_GPUColorTargetDescription cta = ctm;
                if (k < 4) cta.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
                SDL_GPUGraphicsPipelineCreateInfo epi = mpi;
                epi.target_info.color_target_descriptions = &cta;
                epi.vertex_shader = evs;
                epi.fragment_shader = efs;
                a4k_pipe_[k] = SDL_CreateGPUGraphicsPipeline(dev_, &epi);
            }
            if (!a4k_pipe_[k]) LIN_LOG("Anime4K %s: %s", nombres[k], SDL_GetError());
            SDL_ReleaseGPUShader(dev_, evs);
            SDL_ReleaseGPUShader(dev_, efs);
        }
    }
    return pipeline_ != nullptr && pipeline_brillo_ != nullptr && pipeline_mundo_ != nullptr;
}

void Gpu::crear_pipelines_texto(SDL_GPUGraphicsPipelineCreateInfo pi, SDL_GPUShader* fs_texto) {
    // Pasada de capa, dos destinos con la mezcla "over" premultiplicada del pipeline de sprites: [0] RGBA8 = color del texto
    // premultiplicado, [1] R8 = cobertura del texto. Una instancia atenuante da (0, 0, 0, alfa), que la mezcla convierte en
    // dst * (1 - alfa) en ambos (un "over" directo con origen negro no puede conservar la cobertura en el canal alfa).
    SDL_GPUColorTargetDescription cts[2] = {pi.target_info.color_target_descriptions[0],
                                            pi.target_info.color_target_descriptions[0]};
    cts[1].format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    pi.target_info.color_target_descriptions = cts;
    pi.target_info.num_color_targets = 2;
    pi.fragment_shader = fs_texto;
    pipe_texto_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
    if (!pipe_texto_) LIN_LOG("HD text pipeline: %s", SDL_GetError());
}

void Gpu::pase_texto(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* destino, int w, int h, int formato, const RectVista* rect) {
    const u32 n = static_cast<u32>(inst_ordenadas_.size());
    if (!texto_hd_ || primer_texto_ >= n || !pipe_texto_ || !pipe_comp_[formato] || w < 1 || h < 1) return;
    if (!capa_texto_ || !capa_cob_ || capa_w_ != w || capa_h_ != h) {
        SDL_ReleaseGPUTexture(dev_, capa_texto_);
        SDL_ReleaseGPUTexture(dev_, capa_cob_);
        SDL_GPUTextureCreateInfo td{};
        td.type = SDL_GPU_TEXTURETYPE_2D;
        td.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        td.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        td.width = static_cast<Uint32>(w);
        td.height = static_cast<Uint32>(h);
        td.layer_count_or_depth = 1;
        td.num_levels = 1;
        capa_texto_ = SDL_CreateGPUTexture(dev_, &td);
        td.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        capa_cob_ = SDL_CreateGPUTexture(dev_, &td);
        capa_w_ = w;
        capa_h_ = h;
        if (!capa_texto_ || !capa_cob_) return;
    }
    // Pasada 1: la capa de texto. Todo lo ordenado desde la primera instancia de texto en adelante va en un solo dibujo: el texto suma su
    // cobertura, toda otra instancia no aditiva (una ventana, un panel) atenúa lo que ya está en la capa según su
    // alfa, así el texto bajo una ventana translúcida queda atenuado por esa ventana y bajo una opaca desaparece, como en el
    // camino de píxeles (donde la ventana se mezcla sobre el texto). Orden = el orden de instancias ordenado.
    SDL_GPUColorTargetInfo ct[2]{};
    ct[0].texture = capa_texto_;
    ct[1].texture = capa_cob_;
    for (SDL_GPUColorTargetInfo& c : ct) {
        c.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};
        c.load_op = SDL_GPU_LOADOP_CLEAR;
        c.store_op = SDL_GPU_STOREOP_STORE;
    }
    SDL_GPURenderPass* p = SDL_BeginGPURenderPass(cmd, ct, 2, nullptr);
    if (!p) return;
    if (rect) {
        SDL_GPUViewport vp{static_cast<float>(rect->x), static_cast<float>(rect->y), static_cast<float>(rect->w),
                           static_cast<float>(rect->h), 0.0f, 1.0f};
        SDL_SetGPUViewport(p, &vp);
    }
    SDL_BindGPUVertexStorageBuffers(p, 0, &instancias_gpu_, 1);
    const SDL_GPUTextureSamplerBinding tsb[2] = {{atlas_, muestreo_}, {atlas_, muestreo_lineal_}};
    SDL_BindGPUFragmentSamplers(p, 0, tsb, 2);
    const float uni[4] = {static_cast<float>(grid_w_), static_cast<float>(grid_h_), static_cast<float>(primer_texto_), 2};
    SDL_BindGPUGraphicsPipeline(p, pipe_texto_);
    SDL_PushGPUVertexUniformData(cmd, 0, uni, sizeof uni);
    SDL_DrawGPUPrimitives(p, 6, n - primer_texto_, 0, 0);
    SDL_EndGPURenderPass(p);
    // Pasada 2: la capa sobre el destino (premultiplicada).
    SDL_GPUColorTargetInfo cd{};
    cd.texture = destino;
    cd.load_op = SDL_GPU_LOADOP_LOAD;
    cd.store_op = SDL_GPU_STOREOP_STORE;
    p = SDL_BeginGPURenderPass(cmd, &cd, 1, nullptr);
    if (!p) return;
    SDL_BindGPUGraphicsPipeline(p, pipe_comp_[formato]);
    const SDL_GPUTextureSamplerBinding csb[2] = {{capa_texto_, muestreo_}, {capa_cob_, muestreo_}};
    SDL_BindGPUFragmentSamplers(p, 0, csb, 2);
    SDL_DrawGPUPrimitives(p, 6, 1, 0, 0);
    llamadas_ += 2;
    SDL_EndGPURenderPass(p);
}

void Gpu::cerrar() {
    if (!dev_) return;
    SDL_WaitForGPUIdle(dev_);
    SDL_ReleaseGPUTransferBuffer(dev_, subida_atlas_);
    SDL_ReleaseGPUTransferBuffer(dev_, subida_inst_);
    SDL_ReleaseGPUBuffer(dev_, instancias_gpu_);
    SDL_ReleaseGPUSampler(dev_, muestreo_);
    SDL_ReleaseGPUSampler(dev_, muestreo_lineal_);
    SDL_ReleaseGPUTexture(dev_, destino_);
    SDL_ReleaseGPUTexture(dev_, atlas_);
    SDL_ReleaseGPUGraphicsPipeline(dev_, pipeline_);
    SDL_ReleaseGPUGraphicsPipeline(dev_, pipeline_pantalla_);
    SDL_ReleaseGPUGraphicsPipeline(dev_, pipeline_mult_);
    SDL_ReleaseGPUGraphicsPipeline(dev_, pipeline_brillo_);
    SDL_ReleaseGPUGraphicsPipeline(dev_, pipe_texto_);
    for (SDL_GPUGraphicsPipeline* pc : pipe_comp_) SDL_ReleaseGPUGraphicsPipeline(dev_, pc);
    SDL_ReleaseGPUTexture(dev_, capa_texto_);
    SDL_ReleaseGPUTexture(dev_, capa_cob_);
    SDL_ReleaseGPUTexture(dev_, luz_);
    SDL_ReleaseGPUTransferBuffer(dev_, subida_luz_);
    SDL_ReleaseGPUTexture(dev_, copia_);
    SDL_ReleaseGPUTexture(dev_, salida_);
    for (SDL_GPUGraphicsPipeline* p : a4k_pipe_) SDL_ReleaseGPUGraphicsPipeline(dev_, p);
    SDL_ReleaseGPUTexture(dev_, a4k_a_);
    SDL_ReleaseGPUTexture(dev_, a4k_b_);
    SDL_ReleaseGPUTexture(dev_, a4k_x2_);
    SDL_ReleaseGPUSampler(dev_, a4k_lineal_);
    SDL_ReleaseGPUTexture(dev_, filas_tex_);
    SDL_ReleaseGPUTransferBuffer(dev_, subida_filas_);
    SDL_ReleaseGPUGraphicsPipeline(dev_, pipeline_mundo_);
    SDL_ReleaseWindowFromGPUDevice(dev_, ventana_);
    SDL_DestroyGPUDevice(dev_);
    dev_ = nullptr;
}

bool Gpu::subir(const ImagenRGBA& img, RegionAtlas& r, u32 escala_hd) {
    if (img.vacia()) return false;
    size_t bytes = img.px.size() * 4;
    if (staging_atlas_.size() + bytes > TAM_SUBIDA_ATLAS) return false;  // reintentar el próximo frame
    if (!empaquetado_.reservar(img.ancho, img.alto, frame_, r)) {
        if (!atlas_lleno_avisado_) LIN_LOG("Atlas: no hay lugar para %ux%u", img.ancho, img.alto);
        atlas_lleno_avisado_ = true;
        return false;
    }
    if (escala_hd < 1) escala_hd = 1;
    r.w = static_cast<u16>(img.ancho / escala_hd);  // logical size
    r.h = static_cast<u16>(img.alto / escala_hd);
    r.hd = escala_hd > 1 ? static_cast<u8>(escala_hd) : 0;
    r.ox = img.ox;
    r.oy = img.oy;
    SubidaPendiente p{r, static_cast<u32>(staging_atlas_.size())};
    const u8* src = reinterpret_cast<const u8*>(img.px.data());
    staging_atlas_.insert(staging_atlas_.end(), src, src + bytes);
    pendientes_.push_back(p);
    return true;
}

void Gpu::dibujar_escalado(const RegionAtlas& r, float x, float y, float w, float h, u64 clave, u32 color, Mezcla mezcla) {
    if (!r.valida()) return;
    const u64 capa_pedida = clave >> 56;
    const u32 tope = capa_pedida >= CAPA_UI ? MAX_INSTANCIAS
                     : capa_pedida >= CAPA_MUNDO ? MAX_INSTANCIAS - RESERVA_CAPAS
                                                 : MAX_INSTANCIAS - 2 * RESERVA_CAPAS;
    if (inst_.size() >= tope) {
        // Un aviso cada ~10 s mientras siga pasando (no uno solo por proceso).
        if (frame_ >= proximo_aviso_instancias_) {
            LIN_LOG("Gpu: lista de dibujo llena (%zu instancias): se descarta la capa %llu", inst_.size(),
                    static_cast<unsigned long long>(capa_pedida));
            proximo_aviso_instancias_ = frame_ + 600;
        }
        return;
    }
    if (capa_pedida < CAPA_UI && escala_mundo_ != escala_ && !sin_zoom_mundo_) {  // el mundo, con su propio zoom
        const float z = zoom_mundo();
        x *= z;
        y *= z;
        w *= z;
        h *= z;
    }
    // El contenido va desplazado por su origen; se recorta contra la cuadrícula de la ventana.
    x += origen_x_;
    y += origen_y_;
    if (x >= grid_w_ || y >= grid_h_ || x + w <= 0 || y + h <= 0) return;  // fuera de pantalla
    empaquetado_.usar(r.pagina, frame_);
    // Al píxel real más cercano: con muestreo "nearest" el arte no se deforma en posiciones
    // fraccionarias (cada texel ocupa siempre escala × escala píxeles). Con un filtro de escalado la escena va a la
    // grilla lógica, así que al píxel lógico: el filtro recibe la imagen entera de cada cuadro, sin bordes corridos.
    const float e = static_cast<float>(escala_escena() > 0 ? escala_escena() : 1);
    x = std::round(x * e) / e;
    y = std::round(y * e) / e;
    constexpr float inv = 1.0f / TAM_PAGINA;
    const float s = static_cast<float>(r.escala_texel());
    u32 modo = static_cast<u32>(mezcla);
    // Las imágenes HD se filtran en forma lineal: se queda medio texel dentro de la región para que el margen nunca se cuele.
    const float inset = r.hd ? 0.5f : 0.0f;
    if (r.hd) modo |= 4;
    if (mezcla == Mezcla::PANTALLA) ++n_pantalla_;
    if (mezcla == Mezcla::MULTIPLICAR) ++n_mult_;
    inst_.push_back({x, y, w, h, (r.x + inset) * inv, (r.y + inset) * inv, (r.x + r.w * s - inset) * inv,
                     (r.y + r.h * s - inset) * inv, r.pagina, color, modo, 0});
    const u64 capa = clave >> 56;
    if (nivel_ventana_ && (capa == CAPA_UI || capa == CAPA_UI_TEXTO)) {
        // Profundidad: nivel de la ventana (5 bits), texto sobre lo demás (1 bit) y la profundidad
        // original (26 bits; la UI usa valores chicos).
        const u64 prof = std::min<u64>((clave >> 24) & 0xFFFFFFFFu, (1u << 26) - 1);
        clave = (static_cast<u64>(CAPA_VENTANAS) << 56) | (static_cast<u64>(nivel_ventana_ & 31) << 51) |
                ((capa - CAPA_UI) << 50) | (prof << 24);
    }
    claves_.push_back(clave);
}

void Gpu::dibujar(const RegionAtlas& r, int x, int y, u64 clave, u32 color) {
    dibujar_escalado(r, static_cast<float>(x + r.ox), static_cast<float>(y + r.oy), r.w, r.h, clave, color);
}

void Gpu::dibujar_en(const RegionAtlas& r, float x, float y, u64 clave, u32 color, Mezcla mezcla) {
    dibujar_escalado(r, x + r.ox, y + r.oy, r.w, r.h, clave, color, mezcla);
}

void Gpu::rectangulo(int x, int y, int w, int h, u64 clave, u32 color, Mezcla mezcla) {
    // Muestrea solo el centro del blanco de 2×2 para no tocar el margen del atlas.
    RegionAtlas c = blanco_;
    c.w = c.h = 1;
    dibujar_escalado(c, static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h), clave, color, mezcla);
}

// Radix sort LSD estable sobre los 5 bytes altos de la clave (los 3 bajos son siempre 0).
void Gpu::ordenar() {
    size_t n = inst_.size();
    orden_.resize(n);
    orden_tmp_.resize(n);
    for (u32 i = 0; i < n; ++i) orden_[i] = i;
    for (int byte = 3; byte < 8; ++byte) {
        u32 cuenta[257] = {};
        int desp = byte * 8;
        for (u32 i : orden_) ++cuenta[((claves_[i] >> desp) & 0xFF) + 1];
        if (cuenta[1 + ((claves_[orden_[0]] >> desp) & 0xFF)] == n) continue;  // byte igual en todas
        for (int k = 0; k < 256; ++k) cuenta[k + 1] += cuenta[k];
        for (u32 i : orden_) orden_tmp_[cuenta[(claves_[i] >> desp) & 0xFF]++] = i;
        orden_.swap(orden_tmp_);
    }
    inst_ordenadas_.resize(n);
    for (size_t i = 0; i < n; ++i) inst_ordenadas_[i] = inst_[orden_[i]];
    // Como la capa es el byte alto de la clave, lo que va antes de CAPA_EFECTOS es un prefijo.
    n_mundo_ = 0;
    while (n_mundo_ < n && (claves_[orden_[n_mundo_]] >> 56) < CAPA_EFECTOS) ++n_mundo_;
    primer_texto_ = 0;
    while (primer_texto_ < n && !(inst_ordenadas_[primer_texto_].mezcla & 64u)) ++primer_texto_;
}

// Una vista lógica grande (poca escala en una pantalla de muchos píxeles) puede pedir más celdas que
// la textura. Se agranda a la potencia de 2 siguiente: solo pasa al cambiar el tamaño de la vista.
bool Gpu::asegurar_luz(int bw, int bh) {
    if (bw <= luz_w_ && bh <= luz_h_) return true;
    int w = luz_w_, h = luz_h_;
    while (w < bw) w *= 2;
    while (h < bh) h *= 2;
    SDL_GPUTextureCreateInfo tl{};
    tl.type = SDL_GPU_TEXTURETYPE_2D;
    tl.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    tl.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tl.width = static_cast<Uint32>(w);
    tl.height = static_cast<Uint32>(h);
    tl.layer_count_or_depth = 1;
    tl.num_levels = 1;
    SDL_GPUTexture* tex = SDL_CreateGPUTexture(dev_, &tl);
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = static_cast<Uint32>(w) * static_cast<Uint32>(h);
    SDL_GPUTransferBuffer* subida = SDL_CreateGPUTransferBuffer(dev_, &tb);
    if (!tex || !subida) {
        LIN_LOG("Textura de brillo de %dx%d: %s", w, h, SDL_GetError());
        if (tex) SDL_ReleaseGPUTexture(dev_, tex);
        if (subida) SDL_ReleaseGPUTransferBuffer(dev_, subida);
        return false;
    }
    SDL_ReleaseGPUTexture(dev_, luz_);  // SDL la libera cuando la GPU termina de usarla
    SDL_ReleaseGPUTransferBuffer(dev_, subida_luz_);
    luz_ = tex;
    subida_luz_ = subida;
    luz_w_ = w;
    luz_h_ = h;
    brillo_buf_.reserve(static_cast<size_t>(w) * h);
    return true;
}

void Gpu::multiplicar_mundo(const u8* brillo, int bw, int bh, int celda, u32 tinte, const u32* filas, int n_filas) {
    if (!brillo || bw < 1 || bh < 1 || celda < 1 || !asegurar_luz(bw, bh)) return;
    brillo_buf_.assign(brillo, brillo + static_cast<size_t>(bw) * bh);
    if (filas && n_filas >= 1 && n_filas <= FILAS_TAM) brillo_filas_.assign(filas, filas + n_filas);
    else brillo_filas_.assign(1, BLANCO);
    brillo_w_ = bw;
    brillo_h_ = bh;
    brillo_celda_ = celda;
    brillo_tinte_ = tinte;
    brillo_activo_ = true;
}

void Gpu::reemplazar_mundo(int fase_ola, int modo_vision, int fase_borracho) {
    reemplazo_fase_ = fase_ola;
    reemplazo_modo_ = modo_vision;
    reemplazo_fase_b_ = fase_borracho;
    reemplazo_activo_ = fase_ola >= 0 || modo_vision != 0 || fase_borracho >= 0;
}

int Gpu::escala_maxima(int franja) const {
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(ventana_, &pw, &ph);
    // La franja (barra de la ventana) no cuenta para la escala: si no entra, tapa unas filas de arriba
    // del mundo. Así una pantalla de 1080 (ventana de ~1010 px) llega a ×2, como 640×480 del original.
    (void)franja;
    return std::max(1, std::min(pw / ancho_min_, ph / alto_min_));
}

void Gpu::ajustar_vista(bool ampliable, int zoom, int franja, bool solo_mundo) {
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(ventana_, &pw, &ph);
    if (vista_fija_) {
        // Vista lógica constante, como el cliente original; la ventana solo cambia la escala final.
        const bool entero = escalado_ == Escalado::ENTERO;
        ancho_ = grid_w_ = ancho_min_;
        alto_ = alto_min_;
        grid_h_ = alto_min_ + franja;
        origen_x_ = 0;
        origen_y_ = franja;
        ventana_pw_ = std::max(1, pw);
        ventana_ph_ = std::max(1, ph);
        const float fit = factor_ajuste(ventana_pw_, ventana_ph_, grid_w_, grid_h_);
        escala_ = escala_interna(fit, entero);
        // El zoom es solo del mapa, hasta la escala interna: la grilla de 640×480 (y la interfaz) no cambia.
        escala_mundo_ = ampliable && zoom > 0 ? std::min(zoom, escala_) : escala_;
        rect_ = rect_vista(ventana_pw_, ventana_ph_, grid_w_, grid_h_, entero);
        return;
    }
    const int maxima = escala_maxima(franja);
    escala_ = ampliable && zoom > 0 ? std::min(zoom, maxima) : maxima;
    escala_mundo_ = escala_;
    if (ampliable && solo_mundo) escala_ = maxima;
    grid_w_ = std::max(ancho_min_, pw / escala_);
    grid_h_ = std::max(alto_min_, ph / escala_);
    if (ampliable) {
        // El contenido llena la cuadrícula, bajo la franja.
        ancho_ = grid_w_;
        alto_ = grid_h_ - franja;
        origen_x_ = 0;
        origen_y_ = franja;
    } else {
        // Pantallas de 640×480: centradas bajo la franja; lo que sobra de la cuadrícula queda vacío.
        ancho_ = ancho_min_;
        alto_ = alto_min_;
        origen_x_ = (grid_w_ - ancho_) / 2;
        origen_y_ = franja + (grid_h_ - franja - alto_) / 2;
    }
}

bool Gpu::crear_destino() {
    // Con un filtro de escalado la escena se dibuja a la grilla lógica: el filtro lee un punto por píxel lógico y hace el
    // escalado él mismo (dibujarla a escala × escala sería trabajo que el filtro no usa).
    salida_w_ = grid_w_ * escala_;
    salida_h_ = grid_h_ * escala_;
    if (escalado_ != Escalado::ENTERO && (!salida_ || salida_tw_ != salida_w_ || salida_th_ != salida_h_)) {
        SDL_ReleaseGPUTexture(dev_, salida_);  // SDL la libera cuando la GPU termina de usarla
        SDL_GPUTextureCreateInfo ts{};
        ts.type = SDL_GPU_TEXTURETYPE_2D;
        ts.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        ts.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        ts.width = static_cast<Uint32>(salida_w_);
        ts.height = static_cast<Uint32>(salida_h_);
        ts.layer_count_or_depth = 1;
        ts.num_levels = 1;
        salida_ = SDL_CreateGPUTexture(dev_, &ts);
        salida_tw_ = salida_ ? salida_w_ : 0;
        salida_th_ = salida_ ? salida_h_ : 0;
        if (!salida_) LIN_LOG("Textura de salida de %dx%d: %s", salida_w_, salida_h_, SDL_GetError());
    }
    int w = grid_w_ * escala_escena(), h = grid_h_ * escala_escena();
    if (destino_ && w == destino_w_ && h == destino_h_) return true;
    if (destino_) SDL_ReleaseGPUTexture(dev_, destino_);  // SDL la libera cuando la GPU termina de usarla
    SDL_GPUTextureCreateInfo td{};
    td.type = SDL_GPU_TEXTURETYPE_2D;
    td.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    td.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    td.width = static_cast<Uint32>(w);
    td.height = static_cast<Uint32>(h);
    td.layer_count_or_depth = 1;
    td.num_levels = 1;
    destino_ = SDL_CreateGPUTexture(dev_, &td);
    if (copia_) SDL_ReleaseGPUTexture(dev_, copia_);  // copia del mundo para la ola y la visión
    copia_ = SDL_CreateGPUTexture(dev_, &td);
    if (!destino_ || !copia_) {
        LIN_LOG("Textura de destino de %dx%d: %s", w, h, SDL_GetError());
        destino_w_ = destino_h_ = 0;  // se reintenta el próximo frame
        return false;
    }
    destino_w_ = w;
    destino_h_ = h;
    return true;
}

void Gpu::terminar_frame(u32 n) {
    empaquetado_.fin_de_frame();  // si faltó lugar, deja una página libre para el frame siguiente
    instancias_previas_ = n;
    inst_.clear();
    claves_.clear();
    n_pantalla_ = 0;
    n_mult_ = 0;
    brillo_activo_ = false;
    reemplazo_activo_ = false;
    reemplazo_fase_ = -1;
    reemplazo_fase_b_ = -1;
    reemplazo_modo_ = 0;
    ++frame_;
}

void Gpu::preparar_frame() {
    if (!inst_.empty()) ordenar();
    else {
        inst_ordenadas_.clear();
        n_mundo_ = 0;
        primer_texto_ = 0;
    }
    preparado_ = true;
}

void Gpu::presentar() {
    if (!preparado_) preparar_frame();
    preparado_ = false;
    // Sin destino o sin comando no se dibuja este frame; las subidas del atlas quedan para el siguiente.
    SDL_GPUCommandBuffer* cmd = crear_destino() ? SDL_AcquireGPUCommandBuffer(dev_) : nullptr;
    if (!cmd) return terminar_frame(0);

    // 1. Subidas: píxeles nuevos del atlas e instancias del frame.
    SDL_GPUCopyPass* copia = SDL_BeginGPUCopyPass(cmd);
    void* p_atlas = pendientes_.empty() ? nullptr : SDL_MapGPUTransferBuffer(dev_, subida_atlas_, true);
    if (p_atlas) {
        std::memcpy(p_atlas, staging_atlas_.data(), staging_atlas_.size());
        SDL_UnmapGPUTransferBuffer(dev_, subida_atlas_);
        for (const SubidaPendiente& s : pendientes_) {
            SDL_GPUTextureTransferInfo src{};
            src.transfer_buffer = subida_atlas_;
            src.offset = s.offset;
            const u32 tw = s.r.w * s.r.escala_texel(), th = s.r.h * s.r.escala_texel();  // texels (r.w/h son lógicos)
            src.pixels_per_row = tw;
            src.rows_per_layer = th;
            SDL_GPUTextureRegion dst{};
            dst.texture = atlas_;
            dst.layer = s.r.pagina;
            dst.x = s.r.x;
            dst.y = s.r.y;
            dst.w = tw;
            dst.h = th;
            dst.d = 1;
            SDL_UploadToGPUTexture(copia, &src, &dst, false);
        }
        pendientes_.clear();
        staging_atlas_.clear();
    }
    void* p_luz = brillo_activo_ ? SDL_MapGPUTransferBuffer(dev_, subida_luz_, true) : nullptr;
    void* p_filas = p_luz ? SDL_MapGPUTransferBuffer(dev_, subida_filas_, true) : nullptr;
    if (p_luz && !p_filas) SDL_UnmapGPUTransferBuffer(dev_, subida_luz_);
    brillo_activo_ = p_filas != nullptr;  // sin subir el brillo, este frame va sin luz
    if (brillo_activo_) {
        std::memcpy(p_luz, brillo_buf_.data(), brillo_buf_.size());
        SDL_UnmapGPUTransferBuffer(dev_, subida_luz_);
        SDL_GPUTextureTransferInfo src{};
        src.transfer_buffer = subida_luz_;
        src.pixels_per_row = static_cast<Uint32>(brillo_w_);
        src.rows_per_layer = static_cast<Uint32>(brillo_h_);
        SDL_GPUTextureRegion dst{};
        dst.texture = luz_;
        dst.w = static_cast<Uint32>(brillo_w_);
        dst.h = static_cast<Uint32>(brillo_h_);
        dst.d = 1;
        SDL_UploadToGPUTexture(copia, &src, &dst, true);
        std::memcpy(p_filas, brillo_filas_.data(), brillo_filas_.size() * sizeof(u32));
        SDL_UnmapGPUTransferBuffer(dev_, subida_filas_);
        SDL_GPUTextureTransferInfo fsrc{};
        fsrc.transfer_buffer = subida_filas_;
        fsrc.pixels_per_row = 1;
        fsrc.rows_per_layer = static_cast<Uint32>(brillo_filas_.size());
        SDL_GPUTextureRegion fdst{};
        fdst.texture = filas_tex_;
        fdst.w = 1;
        fdst.h = static_cast<Uint32>(brillo_filas_.size());
        fdst.d = 1;
        SDL_UploadToGPUTexture(copia, &fsrc, &fdst, true);
    }
    u32 n = static_cast<u32>(inst_ordenadas_.size());
    void* p_inst = n ? SDL_MapGPUTransferBuffer(dev_, subida_inst_, true) : nullptr;
    // Sin subir las instancias (el buffer tiene las de otro frame) o sin subir los píxeles nuevos del
    // atlas (sus regiones mostrarían los de antes): este frame no se dibuja.
    if (!p_inst || (!pendientes_.empty() && !p_atlas)) {
        if (p_inst) SDL_UnmapGPUTransferBuffer(dev_, subida_inst_);
        n = 0;
    }
    if (n) {
        std::memcpy(p_inst, inst_ordenadas_.data(), n * sizeof(Instancia));
        SDL_UnmapGPUTransferBuffer(dev_, subida_inst_);
        SDL_GPUTransferBufferLocation src{subida_inst_, 0};
        SDL_GPUBufferRegion dst{instancias_gpu_, 0, n * static_cast<u32>(sizeof(Instancia))};
        SDL_UploadToGPUBuffer(copia, &src, &dst, true);
    }
    SDL_EndGPUCopyPass(copia);

    // 2. Dibujo a la textura de destino (lógica × escala): una sola llamada instanciada. El
    // shader sigue trabajando en coordenadas lógicas (destino_ ya se creó al principio).
    SDL_GPUColorTargetInfo objetivo{};
    objetivo.texture = destino_;
    objetivo.clear_color = {0, 0, 0, 1};
    objetivo.load_op = SDL_GPU_LOADOP_CLEAR;
    objetivo.store_op = SDL_GPU_STOREOP_STORE;
    // Pase 1: lo que va por debajo de CAPA_EFECTOS. Con efectos sobre el mundo, solo eso; lo demás va
    // en el pase 4, encima. Sin efectos, un solo pase, como antes.
    const bool efectos = brillo_activo_ || reemplazo_activo_;
    const u32 corte = efectos ? std::min(n_mundo_, n) : n;
    llamadas_ = 0;
    auto dibujar = [&](SDL_GPURenderPass* p, u32 desde, u32 cuantas, auto&& self) -> void {
        if (!cuantas) return;
        // El mundo y las capas de encima (efectos, interfaz) quedan en llamadas separadas (barato; la partición en tramos maneja el orden).
        if ((n_pantalla_ || n_mult_) && n_mundo_ > desde && n_mundo_ < desde + cuantas) {
            self(p, desde, n_mundo_ - desde, self);
            self(p, n_mundo_, desde + cuantas - n_mundo_, self);
            return;
        }
        SDL_BindGPUVertexStorageBuffers(p, 0, &instancias_gpu_, 1);
        const SDL_GPUTextureSamplerBinding tsb[2] = {{atlas_, muestreo_}, {atlas_, muestreo_lineal_}};
        SDL_BindGPUFragmentSamplers(p, 0, tsb, 2);
        // Orden del pintor: las instancias consecutivas de la misma clase de mezcla (0 normal, 1 screen, 3 multiplicar) forman un
        // dibujo; el vertex shader igual filtra por clase, así un tramo puede tener instancias de texto HD (aquí se saltan,
        // no empiezan ni parten un tramo). Sin instancias screen/multiplicar esto es un solo dibujo como antes.
        auto clase = [&](u32 m) -> int { return (m & 8u) ? 1 : (m & 128u) ? 3 : 0; };
        SDL_GPUGraphicsPipeline* const tuberias[4] = {pipeline_, pipeline_pantalla_, nullptr, pipeline_mult_};
        const bool mezclas = (n_pantalla_ || n_mult_);
        u32 i = desde;
        const u32 fin = desde + cuantas;
        while (i < fin) {
            int c = clase(inst_ordenadas_[i].mezcla);
            u32 j = i + 1;
            if (mezclas) {
                while (j < fin) {
                    const u32 m = inst_ordenadas_[j].mezcla;
                    if (!(m & 64u) && clase(m) != c) break;
                    ++j;
                }
            } else {
                j = fin;
            }
            if (tuberias[c]) {
                SDL_BindGPUGraphicsPipeline(p, tuberias[c]);
                const float uni[4] = {static_cast<float>(grid_w_), static_cast<float>(grid_h_), static_cast<float>(i),
                                      static_cast<float>(c)};
                SDL_PushGPUVertexUniformData(cmd, 0, uni, sizeof uni);
                SDL_DrawGPUPrimitives(p, 6, j - i, 0, 0);
                ++llamadas_;
            }
            i = j;
        }
    };
    SDL_GPURenderPass* pase = SDL_BeginGPURenderPass(cmd, &objetivo, 1, nullptr);
    dibujar(pase, 0, corte, dibujar);
    SDL_EndGPURenderPass(pase);
    if (reemplazo_activo_) {
        // Pase 2: copia del mundo y la pasada que lo escribe de nuevo con la ola y/o la visión.
        SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureLocation origen{};
        origen.texture = destino_;
        SDL_GPUTextureLocation copia_dst{};
        copia_dst.texture = copia_;
        SDL_CopyGPUTextureToTexture(cp, &origen, &copia_dst, static_cast<Uint32>(destino_w_), static_cast<Uint32>(destino_h_), 1, false);
        SDL_EndGPUCopyPass(cp);
        objetivo.load_op = SDL_GPU_LOADOP_LOAD;
        pase = SDL_BeginGPURenderPass(cmd, &objetivo, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(pase, pipeline_mundo_);
        SDL_GPUTextureSamplerBinding csb{copia_, muestreo_};
        SDL_BindGPUFragmentSamplers(pase, 0, &csb, 1);
        const float z = zoom_mundo();  // la ola y la borrachera van por columnas y filas del mundo
        float p[8] = {grid_w_ / z, grid_h_ / z, static_cast<float>(reemplazo_fase_),
                      static_cast<float>(reemplazo_modo_), static_cast<float>(reemplazo_fase_b_), AMPLITUD_BORRACHO, 0, 0};
        SDL_PushGPUFragmentUniformData(cmd, 0, p, sizeof p);
        SDL_DrawGPUPrimitives(pase, 6, 1, 0, 0);
        ++llamadas_;
        SDL_EndGPURenderPass(pase);
    }
    if (brillo_activo_) {
        // Pase 3: brillo y tinte (luz, mapas con tinte, agua) sobre el mundo.
        objetivo.load_op = SDL_GPU_LOADOP_LOAD;
        pase = SDL_BeginGPURenderPass(cmd, &objetivo, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(pase, pipeline_brillo_);
        SDL_GPUTextureSamplerBinding lsb[2] = {{luz_, muestreo_}, {filas_tex_, muestreo_}};
        SDL_BindGPUFragmentSamplers(pase, 0, lsb, 2);
        const float celda = static_cast<float>(brillo_celda_);
        // 0.9999: que el borde derecho e inferior caiga dentro de la última celda, no fuera.
        const float z = zoom_mundo();  // las celdas de brillo están en píxeles lógicos del mundo
        float escala[4] = {grid_w_ / z / (celda * luz_w_) * 0.9999f, grid_h_ / z / (celda * luz_h_) * 0.9999f,
                           static_cast<float>(brillo_filas_.size()) / FILAS_TAM * 0.9999f, 0};
        SDL_PushGPUVertexUniformData(cmd, 0, escala, sizeof escala);
        const u32 t = brillo_tinte_;
        float tinte[4] = {(t & 0xFF) / 255.0f, ((t >> 8) & 0xFF) / 255.0f, ((t >> 16) & 0xFF) / 255.0f, 1.0f};
        SDL_PushGPUFragmentUniformData(cmd, 0, tinte, sizeof tinte);
        SDL_DrawGPUPrimitives(pase, 6, 1, 0, 0);
        ++llamadas_;
        SDL_EndGPURenderPass(pase);
    }
    if (efectos) {
        // Pase 4: lo de encima del mundo (efectos, interfaz, ventanas), sin tocar.
        objetivo.load_op = SDL_GPU_LOADOP_LOAD;
        pase = SDL_BeginGPURenderPass(cmd, &objetivo, 1, nullptr);
        dibujar(pase, corte, n - corte, dibujar);
        SDL_EndGPURenderPass(pase);
    }
    if (!ruta_captura_.empty()) cmd = guardar_captura(cmd, destino_, destino_w_, destino_h_, ruta_captura_, true);
    if (!cmd) return terminar_frame(n);  // el comando que sigue a la captura no se pudo obtener

    // 2b. Filtro de escalado: destino_ (la escena a la grilla lógica) -> salida_ (grilla × escala). La captura final la lee de vuelta.
    SDL_GPUTexture* final_tex = destino_;
    const bool hay_texto = texto_hd_ && primer_texto_ < n;
    if (escalado_ == Escalado::ANIME4K) {
        if (SDL_GPUTexture* t = pase_anime4k(cmd)) final_tex = t;
    } else if (escalado_ != Escalado::ENTERO && pipeline_esc_[static_cast<int>(escalado_)] && salida_) {
        SDL_GPUColorTargetInfo ot{};
        ot.texture = salida_;
        ot.load_op = SDL_GPU_LOADOP_DONT_CARE;
        ot.store_op = SDL_GPU_STOREOP_STORE;
        if (SDL_GPURenderPass* fp = SDL_BeginGPURenderPass(cmd, &ot, 1, nullptr)) {
            SDL_BindGPUGraphicsPipeline(fp, pipeline_esc_[static_cast<int>(escalado_)]);
            SDL_GPUTextureSamplerBinding sb{destino_, muestreo_};
            SDL_BindGPUFragmentSamplers(fp, 0, &sb, 1);
            const float u[4] = {static_cast<float>(grid_w_), static_cast<float>(grid_h_),
                                static_cast<float>(salida_w_), static_cast<float>(salida_h_)};
            SDL_PushGPUFragmentUniformData(cmd, 0, u, sizeof u);
            SDL_DrawGPUPrimitives(fp, 6, 1, 0, 0);
            SDL_EndGPURenderPass(fp);
            final_tex = salida_;
        }
    }
    // Si el filtro no se pudo aplicar (sin su pipeline o sin texturas), la escena a la grilla lógica se agranda por vecino.
    if (final_tex == destino_ && escala_escena() != escala_ && salida_) {
        SDL_GPUBlitInfo b{};
        b.source.texture = destino_;
        b.source.w = static_cast<Uint32>(destino_w_);
        b.source.h = static_cast<Uint32>(destino_h_);
        b.destination.texture = salida_;
        b.destination.w = static_cast<Uint32>(salida_w_);
        b.destination.h = static_cast<Uint32>(salida_h_);
        b.load_op = SDL_GPU_LOADOP_DONT_CARE;
        b.filter = SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(cmd, &b);
        final_tex = salida_;
    }
    if (hay_texto && !vista_fija_) pase_texto(cmd, final_tex, salida_w_, salida_h_, 0, nullptr);
    if (!ruta_final_.empty()) {
        SDL_GPUTexture* ventana_tex = nullptr;
        if (vista_fija_) {
            // Imagen fuera de pantalla del tamaño de la ventana con el mismo blit con bandas negras que recibe el swapchain.
            SDL_GPUTextureCreateInfo td{};
            td.type = SDL_GPU_TEXTURETYPE_2D;
            td.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            td.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
            td.width = static_cast<Uint32>(ventana_pw_);
            td.height = static_cast<Uint32>(ventana_ph_);
            td.layer_count_or_depth = 1;
            td.num_levels = 1;
            ventana_tex = SDL_CreateGPUTexture(dev_, &td);
            if (ventana_tex) {
                blit_vista(cmd, final_tex, ventana_tex);
                if (hay_texto) pase_texto(cmd, ventana_tex, ventana_pw_, ventana_ph_, 0, &rect_);
            }
        }
        if (ventana_tex) {
            cmd = guardar_captura(cmd, ventana_tex, ventana_pw_, ventana_ph_, ruta_final_, false);
            SDL_ReleaseGPUTexture(dev_, ventana_tex);
        } else {
            cmd = guardar_captura(cmd, final_tex, salida_w_, salida_h_, ruta_final_, false);
        }
    }
    if (!cmd) return terminar_frame(n);

    // 3. Copia 1:1 a la ventana, centrada (la textura ya tiene el tamaño escalado).
    SDL_GPUTexture* swap = nullptr;
    Uint32 sw = 0, sh = 0;
    if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, ventana_, &swap, &sw, &sh) && swap) {
        if (vista_fija_) {
            // Se usa el tamaño real del swapchain para que la correspondencia del mouse siempre coincida con lo que se muestra.
            ventana_pw_ = static_cast<int>(sw);
            ventana_ph_ = static_cast<int>(sh);
            rect_ = rect_vista(ventana_pw_, ventana_ph_, grid_w_, grid_h_, escalado_ == Escalado::ENTERO);
            blit_vista(cmd, final_tex, swap);
            if (hay_texto) pase_texto(cmd, swap, static_cast<int>(sw), static_cast<int>(sh), 1, &rect_);
        } else {
            Uint32 dw = static_cast<Uint32>(salida_w_), dh = static_cast<Uint32>(salida_h_);
            SDL_GPUBlitInfo b{};
            b.source.texture = final_tex;
            b.destination.texture = swap;
            b.destination.x = sw > dw ? (sw - dw) / 2 : 0;
            b.destination.y = sh > dh ? (sh - dh) / 2 : 0;
            b.destination.w = std::min(dw, sw);
            b.destination.h = std::min(dh, sh);
            b.source.w = b.destination.w;
            b.source.h = b.destination.h;
            b.load_op = SDL_GPU_LOADOP_CLEAR;
            b.clear_color = {0, 0, 0, 1};
            b.filter = SDL_GPU_FILTER_NEAREST;
            SDL_BlitGPUTexture(cmd, &b);
        }
    }
    if (SDL_GPUFence* f = medir_gpu_ ? SDL_SubmitGPUCommandBufferAndAcquireFence(cmd) : nullptr) {
        const Uint64 t0 = SDL_GetTicksNS();
        SDL_WaitForGPUFences(dev_, true, &f, 1);
        SDL_ReleaseGPUFence(dev_, f);
        ms_gpu_ += static_cast<double>(SDL_GetTicksNS() - t0) / 1e6;
        ++cuadros_gpu_;
    } else if (!medir_gpu_) {
        SDL_SubmitGPUCommandBuffer(cmd);
    }
    terminar_frame(n);
    pace_frame();
}

// Vista fija: imagen final -> `dst` (swapchain o fuera de pantalla), con bandas negras. Los ajustes enteros son 1:1 por vecino más cercano;
// todo lo demás se remuestrea con un filtro lineal desde la imagen interna (como mucho 2x más grande).
void Gpu::blit_vista(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* src, SDL_GPUTexture* dst) {
    SDL_GPUBlitInfo b{};
    b.source.texture = src;
    b.source.w = static_cast<Uint32>(salida_w_);
    b.source.h = static_cast<Uint32>(salida_h_);
    b.destination.texture = dst;
    b.destination.x = static_cast<Uint32>(rect_.x);
    b.destination.y = static_cast<Uint32>(rect_.y);
    b.destination.w = static_cast<Uint32>(rect_.w);
    b.destination.h = static_cast<Uint32>(rect_.h);
    b.load_op = SDL_GPU_LOADOP_CLEAR;
    b.clear_color = {0, 0, 0, 1};
    const bool uno_a_uno = rect_.w == salida_w_ && rect_.h == salida_h_;
    b.filter = uno_a_uno ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(cmd, &b);
}

// MAILBOX no espera a la pantalla, así que el bucle se limita a ~60 fps (con VSYNC ya va a ese ritmo).
void Gpu::pace_frame() {
    if (!mailbox_) return;
    constexpr Uint64 FRAME_NS = 1'000'000'000ull / 60;
    const Uint64 now = SDL_GetTicksNS();
    if (next_frame_ns_ > now) SDL_DelayPrecise(next_frame_ns_ - now);
    next_frame_ns_ = std::max(next_frame_ns_ + FRAME_NS, SDL_GetTicksNS());
}

SDL_GPUCommandBuffer* Gpu::guardar_captura(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* tex, int w, int h, std::string& ruta, bool logica) {
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    tb.size = static_cast<Uint32>(w * h * 4);
    SDL_GPUTransferBuffer* bajada = SDL_CreateGPUTransferBuffer(dev_, &tb);
    if (!bajada) {
        LIN_LOG("Captura: sin buffer de %dx%d (%s)", w, h, SDL_GetError());
        ruta.clear();
        return cmd;  // el frame sigue en el mismo comando
    }
    SDL_GPUCopyPass* copia = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion src{};
    src.texture = tex;
    src.w = static_cast<Uint32>(w);
    src.h = static_cast<Uint32>(h);
    src.d = 1;
    SDL_GPUTextureTransferInfo dst{};
    dst.transfer_buffer = bajada;
    SDL_DownloadFromGPUTexture(copia, &src, &dst);
    SDL_EndGPUCopyPass(copia);
    // Se envía este comando y se espera a la GPU para poder leer los píxeles.
    SDL_GPUFence* f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!f) {  // el envío falló: no hay píxeles que leer
        LIN_LOG("Captura: no se pudo enviar el comando (%s)", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(dev_, bajada);
        ruta.clear();
        return SDL_AcquireGPUCommandBuffer(dev_);
    }
    SDL_WaitForGPUFences(dev_, true, &f, 1);
    SDL_ReleaseGPUFence(dev_, f);
    // La captura se guarda a la resolución lógica (un píxel de cada bloque de escala × escala).
    if (const u32* px = static_cast<const u32*>(SDL_MapGPUTransferBuffer(dev_, bajada, false))) {
        // `logica`: un píxel por bloque de escala × escala; si no, la imagen entera tal cual.
        std::vector<u32> img;
        const u32* datos = px;
        int iw = w, ih = h;
        if (logica) {
            iw = grid_w_;
            ih = grid_h_;
            img.resize(static_cast<size_t>(iw) * ih);
            for (int y = 0; y < ih; ++y)
                for (int x = 0; x < iw; ++x) img[static_cast<size_t>(y) * iw + x] = px[static_cast<size_t>(y * escala_escena()) * w + x * escala_escena()];
            datos = img.data();
        }
        SDL_Surface* s = SDL_CreateSurfaceFrom(iw, ih, SDL_PIXELFORMAT_ABGR8888, const_cast<u32*>(datos), iw * 4);
        if (s && SDL_SaveBMP(s, ruta.c_str())) LIN_LOG("Captura guardada en %s", ruta.c_str());
        SDL_DestroySurface(s);
        SDL_UnmapGPUTransferBuffer(dev_, bajada);
    }
    SDL_ReleaseGPUTransferBuffer(dev_, bajada);
    ruta.clear();
    return SDL_AcquireGPUCommandBuffer(dev_);  // el resto del frame va en un comando nuevo
}

void Gpu::a_grilla(float wx, float wy, int& gx, int& gy) const {
    int pw = 0, ph = 0, ww = 0, wh = 0;
    SDL_GetWindowSizeInPixels(ventana_, &pw, &ph);
    SDL_GetWindowSize(ventana_, &ww, &wh);
    const float densidad = ww > 0 ? static_cast<float>(pw) / ww : 1.0f;
    if (vista_fija_) {
        pixel_a_grilla(wx * densidad, wy * densidad, rect_, grid_w_, grid_h_, gx, gy);
        return;
    }
    float x = wx * densidad - (pw - grid_w_ * escala_) / 2.0f;
    float y = wy * densidad - (ph - grid_h_ * escala_) / 2.0f;
    gx = static_cast<int>(x / escala_);
    gy = static_cast<int>(y / escala_);
}

void Gpu::a_logico(float wx, float wy, int& lx, int& ly) const {
    int gx = 0, gy = 0;
    a_grilla(wx, wy, gx, gy);
    lx = gx - origen_x_;
    ly = gy - origen_y_;
}

}  // namespace lin

#include "recursos/fuente_hd.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace lin {
namespace {

// Sigma gaussiana (píxeles lógicos) del suavizado, y el nivel de la máscara suavizada que pasa a ser el contorno.
// Un trazo de 1 px llega a ~0,8 tras el suavizado, así que un nivel bajo 0,5 evita que se achiquen los extremos y esquinas de los trazos;
// es la perilla que "conserva las esquinas": más bajo = esquinas más cuadradas y trazos más gruesos, más alto = más redondas y finas.
constexpr float SIGMA = 0.42f;
constexpr float NIVEL = 0.44f;

// Gaussiana separable sobre `src` (w*h floats) hacia `dst`; `tmp` es espacio de trabajo.
void desenfocar(const std::vector<float>& src, std::vector<float>& tmp, std::vector<float>& dst, int w, int h) {
    const float sigma = SIGMA * HD_ESCALA;
    const int radio = static_cast<int>(std::ceil(sigma * 3.0f));
    float k[64] = {};
    float suma = 0;
    for (int i = -radio; i <= radio; ++i) suma += (k[i + radio] = std::exp(-0.5f * i * i / (sigma * sigma)));
    for (int i = 0; i <= 2 * radio; ++i) k[i] /= suma;
    tmp.assign(src.size(), 0.0f);
    dst.assign(src.size(), 0.0f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float a = 0;
            for (int i = -radio; i <= radio; ++i) {
                const int xx = x + i;
                if (xx >= 0 && xx < w) a += k[i + radio] * src[static_cast<size_t>(y) * w + xx];
            }
            tmp[static_cast<size_t>(y) * w + x] = a;
        }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float a = 0;
            for (int i = -radio; i <= radio; ++i) {
                const int yy = y + i;
                if (yy >= 0 && yy < h) a += k[i + radio] * tmp[static_cast<size_t>(yy) * w + x];
            }
            dst[static_cast<size_t>(y) * w + x] = a;
        }
}

}  // namespace

bool campo_de_mascara(const ImagenRGBA& m, ImagenRGBA& s) {
    const int w = m.ancho, h = m.alto;
    if (w <= 0 || h <= 0) return false;
    const int W = (w + 2 * HD_BORDE) * HD_ESCALA, H = (h + 2 * HD_BORDE) * HD_ESCALA;
    static thread_local std::vector<float> a, b, c;  // reused: no allocation per glyph once warm
    a.assign(static_cast<size_t>(W) * H, 0.0f);
    bool tinta = false;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if ((m.px[static_cast<size_t>(y) * w + x] >> 24) == 0) continue;
            tinta = true;
            for (int j = 0; j < HD_ESCALA; ++j)
                for (int i = 0; i < HD_ESCALA; ++i)
                    a[static_cast<size_t>((y + HD_BORDE) * HD_ESCALA + j) * W + (x + HD_BORDE) * HD_ESCALA + i] = 1.0f;
        }
    if (!tinta) return false;
    desenfocar(a, b, c, W, H);
    s.preparar(static_cast<u16>(W), static_cast<u16>(H));
    s.ox = s.oy = -HD_BORDE;
    for (size_t i = 0; i < c.size(); ++i) {  // el nivel del contorno pasa a ser 0,5
        const u32 v = static_cast<u32>(std::clamp(c[i] - NIVEL + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
        s.px[i] = v * 0x01010101u;
    }
    return true;
}

bool campo_de_cobertura(const u8* cob, int w, int h, int x0, int y0, int cw, int ch, ImagenRGBA& s) {
    const int W = (cw + 2 * HD_BORDE) * HD_ESCALA, H = (ch + 2 * HD_BORDE) * HD_ESCALA;
    s.preparar(static_cast<u16>(W), static_cast<u16>(H));
    s.ox = s.oy = -HD_BORDE;
    bool tinta = false;
    for (int y = 0; y < h; ++y) {
        const int dy = y0 + y + HD_BORDE * HD_ESCALA;
        if (dy < 0 || dy >= H) continue;
        for (int x = 0; x < w; ++x) {
            const int dx = x0 + x + HD_BORDE * HD_ESCALA;
            const u32 v = cob[static_cast<size_t>(y) * w + x];
            if (dx < 0 || dx >= W || v == 0) continue;
            tinta = true;
            s.px[static_cast<size_t>(dy) * W + dx] = v * 0x01010101u;
        }
    }
    return tinta;
}

}  // namespace lin

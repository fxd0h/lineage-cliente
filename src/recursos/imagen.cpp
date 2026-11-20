#include <algorithm>
#include <cmath>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>
#include "recursos/imagen.h"

namespace lin {

bool decodificar_img(Bytes datos, ImagenRGBA& salida) {
    LectorBinario l{datos};
    u16 w = l.u16_(), h = l.u16_();
    l.u16_();
    u16 clave = l.u16_();
    if (l.error || w == 0 || h == 0 || !l.quedan(static_cast<size_t>(w) * h * 2)) return false;
    salida.preparar(w, h);
    for (size_t i = 0; i < salida.px.size(); ++i) {
        u16 c = l.u16_();
        salida.px[i] = (c == clave) ? 0 : rgb555(c);
    }
    return true;
}

// Fondo negro -> transparente. Las animaciones de la selección de personaje traen, sobre negro,
// resplandores y sombras al pie en degradé (de 0x01 a ~0x40 junto a la figura): luz que el
// original sumaba al fondo. Desde el negro puro se recorren los píxeles oscuros (canal máximo <
// UMBRAL_RESPLANDOR) unidos a él y cada uno pasa a alfa = máximo / UMBRAL con el color dividido
// por ese alfa: dibujado encima queda color + (1 - alfa) × fondo, casi lo mismo que sumarlo, y el
// negro desaparece. Lo oscuro que no toca el fondo (dentro de una figura) queda opaco.
constexpr u32 UMBRAL_RESPLANDOR = 0x40;
void quitar_fondo_negro(ImagenRGBA& img) {
    auto maximo = [](u32 c) { return std::max({c & 0xFFu, (c >> 8) & 0xFFu, (c >> 16) & 0xFFu}); };
    const int w = img.ancho, h = img.alto;
    std::vector<u8> visto(img.px.size(), 0);
    std::vector<u32> pila;
    for (int i = 0; i < w * h; ++i)
        if ((img.px[i] & 0x00FFFFFFu) == 0) {
            img.px[i] = 0;
            visto[i] = 1;
            pila.push_back(static_cast<u32>(i));
        }
    while (!pila.empty()) {
        int i = static_cast<int>(pila.back());
        pila.pop_back();
        int x = i % w, y = i / w;
        const int vecinos[4] = {x > 0 ? i - 1 : -1, x + 1 < w ? i + 1 : -1, y > 0 ? i - w : -1, y + 1 < h ? i + w : -1};
        for (int v : vecinos) {
            if (v < 0 || visto[v]) continue;
            u32 c = img.px[v], m = maximo(c);
            if (m >= UMBRAL_RESPLANDOR) continue;
            visto[v] = 1;
            pila.push_back(static_cast<u32>(v));
            u32 r = (c & 0xFF) * UMBRAL_RESPLANDOR / m, g = ((c >> 8) & 0xFF) * UMBRAL_RESPLANDOR / m,
                b = ((c >> 16) & 0xFF) * UMBRAL_RESPLANDOR / m, a = m * 255 / UMBRAL_RESPLANDOR;
            img.px[v] = std::min(r, 255u) | (std::min(g, 255u) << 8) | (std::min(b, 255u) << 16) | (a << 24);
        }
    }
}

bool decodificar_png(Bytes datos, ImagenRGBA& salida, bool fondo_negro) {
    if (datos.size() < 8) return false;
    std::vector<u8> copia;
    const u8* p = datos.data();
    if (datos[3] == 0x58) {  // 'X': PNG ofuscado (check_fix_png de l1j-client)
        copia.assign(datos.begin(), datos.end());
        copia[3] = 0x47;
        for (size_t i = 1; i + 5 <= copia.size(); ++i) {
            size_t k = copia.size() - i;
            copia[k] ^= copia[k - 1];
            copia[k] ^= 0x52;
        }
        p = copia.data();
    }
    int w, h, canales;
    u8* rgba = stbi_load_from_memory(p, static_cast<int>(datos.size()), &w, &h, &canales, 4);
    if (!rgba) return false;
    salida.preparar(static_cast<u16>(w), static_cast<u16>(h));
    std::memcpy(salida.px.data(), rgba, salida.px.size() * 4);
    stbi_image_free(rgba);
    if (fondo_negro) quitar_fondo_negro(salida);
    return true;
}

void ampliar_glifo_suave(const ImagenRGBA& e, int k, ImagenRGBA& s) {
    s.preparar(static_cast<u16>(e.ancho * k), static_cast<u16>(e.alto * k));
    s.ox = static_cast<i16>(e.ox * k);
    s.oy = static_cast<i16>(e.oy * k);
    auto alfa = [&](int x, int y) {
        return x < 0 || y < 0 || x >= e.ancho || y >= e.alto ? 0.0f : (e.px[static_cast<size_t>(y) * e.ancho + x] >> 24) / 255.0f;
    };
    for (int y = 0; y < s.alto; ++y) {
        const float v = (y + 0.5f) / k - 0.5f;
        const int y0 = static_cast<int>(std::floor(v));
        const float fy = v - y0;
        for (int x = 0; x < s.ancho; ++x) {
            const float u = (x + 0.5f) / k - 0.5f;
            const int x0 = static_cast<int>(std::floor(u));
            const float fx = u - x0;
            const float val = (alfa(x0, y0) * (1 - fx) + alfa(x0 + 1, y0) * fx) * (1 - fy) +
                              (alfa(x0, y0 + 1) * (1 - fx) + alfa(x0 + 1, y0 + 1) * fx) * fy;
            // El valor cambia 1/k por píxel de salida a través de un borde: el corte en 0,5 con esa pendiente
            // deja un píxel de transición (antialias).
            const float a = std::clamp((val - 0.5f) * k + 0.5f, 0.0f, 1.0f);
            const u32 b = static_cast<u32>(a * 255.0f + 0.5f);
            s.px[static_cast<size_t>(y) * s.ancho + x] = b | (b << 8) | (b << 16) | (b << 24);
        }
    }
}

}  // namespace lin

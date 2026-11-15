#include "render/icon_aura.h"

namespace lin {

namespace {
constexpr u32 ALPHA_OUTER = 85;   // 255 / 3
constexpr u32 ALPHA_INNER = 142;  // 255 * 5 / 9

// Is the silhouette dilated by d px set at icon coordinates (x, y)?
bool in_dilation(const ImagenRGBA& icon, int x, int y, int d) {
    auto opaque = [&](int ix, int iy) {
        return ix >= 0 && iy >= 0 && ix < icon.ancho && iy < icon.alto &&
               (icon.px[static_cast<size_t>(iy) * icon.ancho + ix] >> 24) != 0;
    };
    return opaque(x, y) || opaque(x - d, y) || opaque(x + d, y) || opaque(x, y - d) || opaque(x, y + d);
}
}  // namespace

void build_bless_aura(const ImagenRGBA& icon, ImagenRGBA& aura) {
    const int w = icon.ancho, h = icon.alto;
    aura.preparar(static_cast<u16>(w + AURA_PAD), static_cast<u16>(h + AURA_PAD));
    for (int y = 0; y < h + AURA_PAD; ++y)
        for (int x = 0; x < w + AURA_PAD; ++x) {
            const int ix = x - AURA_PAD / 2, iy = y - AURA_PAD / 2;
            u32 a = 0;
            if (in_dilation(icon, ix, iy, 1)) a = ALPHA_INNER;
            else if (in_dilation(icon, ix, iy, 2)) a = ALPHA_OUTER;
            aura.px[static_cast<size_t>(y) * aura.ancho + x] = 0x00FFFFFFu | (a << 24);
        }
}

void agregar_aura(ImagenRGBA& img) {
    if (img.vacia()) return;
    const int w = img.ancho, h = img.alto;
    ImagenRGBA aura;
    build_bless_aura(img, aura);
    ImagenRGBA out;
    out.preparar(static_cast<u16>(2 * w + AURA_PAD), static_cast<u16>(h + AURA_PAD));
    out.ox = img.ox;
    out.oy = img.oy;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) out.px[static_cast<size_t>(y) * out.ancho + x] = img.px[static_cast<size_t>(y) * w + x];
    for (int y = 0; y < aura.alto; ++y)
        for (int x = 0; x < aura.ancho; ++x)
            out.px[static_cast<size_t>(y) * out.ancho + w + x] = aura.px[static_cast<size_t>(y) * aura.ancho + x];
    img = static_cast<ImagenRGBA&&>(out);
}

}  // namespace lin

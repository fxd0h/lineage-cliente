#include "app/item_icon.h"
#include "render/icon_aura.h"

namespace lin {

void draw_item_icon(Gpu& gpu, const RegionAtlas& r, int x, int y, float w, float h, u64 key, u8 bless,
                    bool identified) {
    const u8 kind = identified ? bless_kind(bless) : static_cast<u8>(BLESS_NORMAL);
    const float fx = static_cast<float>(x + r.ox), fy = static_cast<float>(y + r.oy);
    const u64 over = key + 1;  // low 24 bits of the key are free: above the base icon, below the next depth
    if (kind == BLESS_CURSED) {
        // Original: pixel = icon / 2 + 0x3800 (565), i.e. half brightness plus a constant (58, 0, 0).
        gpu.dibujar_escalado(r, fx, fy, w, h, key, rgba(128, 128, 128));
        gpu.dibujar_escalado(r, fx, fy, w, h, over, rgba(58, 0, 0), Mezcla::SILUETA_ADITIVA);
        return;
    }
    gpu.dibujar_escalado(r, fx, fy, w, h, key, BLANCO);
    if (kind == BLESS_BLESSED && r.aura_dx != 0 && r.w != 0) {
        // Blessed, as in the 2009 client (FUN_00445ac0, LUT 0x8f173c: v' = (2v + max) / 3): the silhouette
        // dilated by 2 px and by 1 px each lighten the screen one third toward white, so the icon gets
        // lighter and a 1-2 px whitish halo surrounds it. The aura image holds both passes (alpha 1/3 on
        // the outer ring, 5/9 inside) and is drawn over the icon.
        RegionAtlas aura = r;
        aura.x = static_cast<u16>(r.x + r.aura_dx);
        aura.w = static_cast<u16>(r.w + AURA_PAD);
        aura.h = static_cast<u16>(r.h + AURA_PAD);
        aura.ox = aura.oy = aura.aura_dx = 0;
        const float sx = w / static_cast<float>(r.w), sy = h / static_cast<float>(r.h);
        const float pad = AURA_PAD / 2;
        gpu.dibujar_escalado(aura, fx - pad * sx, fy - pad * sy, static_cast<float>(aura.w) * sx,
                             static_cast<float>(aura.h) * sy, over, BLANCO, Mezcla::SILUETA);
    }
}

}  // namespace lin

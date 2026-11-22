#include "app/item_icon.h"
#include "render/icon_aura.h"

namespace lin {

void draw_item_icon(Gpu& gpu, const RegionAtlas& r, int x, int y, float w, float h, u64 key, u8 bless,
                    bool identified) {
    const u8 kind = identified ? bless_kind(bless) : static_cast<u8>(BLESS_NORMAL);
    const float fx = static_cast<float>(x + r.ox), fy = static_cast<float>(y + r.oy);
    const u64 over = key + 1;  // los 24 bits bajos de la clave están libres: encima del ícono base, debajo de la profundidad siguiente
    if (kind == BLESS_CURSED) {
        // Original: píxel = ícono / 2 + 0x3800 (565), o sea media luminosidad más una constante (58, 0, 0).
        gpu.dibujar_escalado(r, fx, fy, w, h, key, rgba(128, 128, 128));
        gpu.dibujar_escalado(r, fx, fy, w, h, over, rgba(58, 0, 0), Mezcla::SILUETA_ADITIVA);
        return;
    }
    // Bendito: el halo aclara primero lo que está debajo y el ícono se copia opaco encima (0x44638a), así que el ícono
    // mismo no se aclara: ícono sobre el aura.
    const bool aura_on = kind == BLESS_BLESSED && r.aura_dx != 0 && r.w != 0;
    gpu.dibujar_escalado(r, fx, fy, w, h, aura_on ? over : key, BLANCO);
    if (aura_on) {
        // Bendito, como en el cliente 2009 (0x445fd0, LUT 0x8f173c: v' = (2v + max) / 3): la silueta del ícono desplazada
        // +/-2 px y +/-1 px aclara cada vez la pantalla un tercio hacia el blanco, un halo blanquecino de 1-2 px alrededor del ícono.
        RegionAtlas aura = r;
        aura.x = static_cast<u16>(r.x + r.aura_dx);
        aura.w = static_cast<u16>(r.w + AURA_PAD);
        aura.h = static_cast<u16>(r.h + AURA_PAD);
        aura.ox = aura.oy = aura.aura_dx = 0;
        const float sx = w / static_cast<float>(r.w), sy = h / static_cast<float>(r.h);
        const float pad = AURA_PAD / 2;
        gpu.dibujar_escalado(aura, fx - pad * sx, fy - pad * sy, static_cast<float>(aura.w) * sx,
                             static_cast<float>(aura.h) * sy, key, BLANCO, Mezcla::SILUETA);
    }
}

}  // namespace lin

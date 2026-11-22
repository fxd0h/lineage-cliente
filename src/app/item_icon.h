// Dibujo del ícono de un objeto con el estado de bendición del cliente original (S3EP1 FUN_00445ac0 en 0x445ac0).
#pragma once
#include "mundo/inventario.h"
#include "render/gpu.h"

namespace lin {

// Dibuja un ícono de inventario de w x h píxeles lógicos en (x, y) (aquí se suma el desplazamiento de la región del atlas). `bless`
// es el valor crudo del protocolo de S_AddItem / S_InvList /
// S_ItemColor. El original muestra el estado de bendición solo en el ícono, nunca en el nombre:
//  - bendito (0): brillo blanco, un aclarado interior más un halo de 1-2 px alrededor de la silueta (aura precalculada);
//  - maldito (2): el ícono a media luminosidad más un rojo oscuro constante (58, 0, 0) (color de 16 bits 0x3800 en modo 565);
//  - normal (1) y sin identificar (3, o `identified` en false): ícono liso.
// `key` es la clave de orden de dibujo del ícono liso; las pasadas de brillo/tinte van justo encima.
void draw_item_icon(Gpu& gpu, const RegionAtlas& r, int x, int y, float w, float h, u64 key, u8 bless,
                    bool identified = true);
inline void draw_item_icon(Gpu& gpu, const RegionAtlas& r, int x, int y, u64 key, u8 bless, bool identified = true) {
    draw_item_icon(gpu, r, x, y, static_cast<float>(r.w), static_cast<float>(r.h), key, bless, identified);
}

}  // namespace lin

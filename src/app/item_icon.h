// Item icon drawing with the bless state of the original client (S3EP1 FUN_00445ac0 at 0x445ac0).
#pragma once
#include "mundo/inventario.h"
#include "render/gpu.h"

namespace lin {

// Draws an inventory icon of w x h logical pixels at (x, y) (the atlas region offset is added here). `bless`
// is the raw wire value of S_AddItem / S_InvList /
// S_ItemColor. The original shows the bless state only on the icon, never in the name:
//  - blessed (0): white glow, an interior lightening plus a 1-2 px halo around the silhouette (precomputed aura);
//  - cursed (2): the icon at half brightness plus a constant dark red (58, 0, 0) (16-bit color 0x3800 in 565 mode);
//  - normal (1) and unidentified (3, or `identified` false): plain icon.
// `key` is the draw-order key of the plain icon; the glow/tint passes go right above it.
void draw_item_icon(Gpu& gpu, const RegionAtlas& r, int x, int y, float w, float h, u64 key, u8 bless,
                    bool identified = true);
inline void draw_item_icon(Gpu& gpu, const RegionAtlas& r, int x, int y, u64 key, u8 bless, bool identified = true) {
    draw_item_icon(gpu, r, x, y, static_cast<float>(r.w), static_cast<float>(r.h), key, bless, identified);
}

}  // namespace lin

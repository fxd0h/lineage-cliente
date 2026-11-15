// Bless aura of inventory icons, as the original client builds it (S3EP1 FUN_00445ac0 at 0x445ac0).
//
// The original lightens by 1/3 toward white (x' = (2x + 255) / 3) every screen pixel under the icon's opaque
// silhouette dilated by 2 px, then again under the one dilated by 1 px. Pixels in the 1 px dilation are thus
// lightened twice, i.e. a white overlay of alpha 1 - (2/3)^2 = 5/9; the 2 px ring only once (alpha 1/3). The
// aura is that overlay precomputed as one image (white, with those alphas), drawn once with Mezcla::SILUETA.
// Dilation = the silhouette shifted by (+-d, 0) and (0, +-d), united with itself.
#pragma once
#include "recursos/imagen.h"

namespace lin {

constexpr int AURA_PAD = 4;  // the aura image is 2 px larger on every side than the icon

// Builds the (w + 4) x (h + 4) aura of `icon` (the icon sits at (2, 2)).
void build_bless_aura(const ImagenRGBA& icon, ImagenRGBA& aura);
// Replaces `img` (w x h) by an image of (2w + 4) x (h + 4): the icon at (0, 0) and its aura at (w, 0).
void agregar_aura(ImagenRGBA& img);

}  // namespace lin

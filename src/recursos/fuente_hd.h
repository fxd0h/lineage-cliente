// HD text: glyph "fields" for the post-filter text pass (see docs/fork/hd_text.md).
//
// A field is an HD_ESCALA texels-per-logical-pixel image whose value crosses 0.5 exactly on the glyph outline
// (inside > 0.5). The GPU samples it bilinearly and thresholds it with a screen-space smoothstep, so the outline is
// a resolution-independent smooth curve: sharp at any display scale. Bitmap fonts get their field by smoothing the
// binary mask (an implicit-curve vectorizer); the HTML face is rasterized from its real outlines.
#pragma once
#include "core/tipos.h"
#include "recursos/imagen.h"

namespace lin {

constexpr int HD_ESCALA = 8;  // field texels per logical pixel
constexpr int HD_BORDE = 1;   // logical pixels of empty margin around the glyph cell (the outline may bulge)

// Field of a 1-bit mask. `mascara` is w*h RGBA pixels (alpha != 0 = ink; the bitmap font cells). `salida` becomes
// (w + 2 HD_BORDE) * HD_ESCALA wide, with ox = oy = -HD_BORDE (logical), the field in R, G, B and A (premultiplied
// white, so the atlas upload needs no special case). false if the glyph has no ink.
bool campo_de_mascara(const ImagenRGBA& mascara, ImagenRGBA& salida);

// Field from an 8-bit coverage image of `w` x `h` texels placed at texel (x0, y0) of a cell of `cw` x `ch` logical
// pixels (HD_ESCALA texels each). Used for outline-rendered faces. false if empty.
bool campo_de_cobertura(const u8* cobertura, int w, int h, int x0, int y0, int cw, int ch, ImagenRGBA& salida);

}  // namespace lin

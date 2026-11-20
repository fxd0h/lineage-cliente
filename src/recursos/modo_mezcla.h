// Blend mode of a sprite or map tile, decoded from the object attribute flags like the original client
// (S3EP1U frame renderer 0x465c30, see docs/fork/RE_SPRITES_RENDER.md).
#pragma once
#include "core/tipos.h"

namespace lin {

enum class ModoMezcla : u8 {
    NORMAL,
    OCULTO,       // flag 0x2: not drawn
    PANTALLA,     // flag 0x8: screen, max + min * (1 - max)
    PROMEDIO,     // flag 0x1 / 0x100: 50% average
    MULTIPLICAR,  // flag 0x80: dst * src (white leaves the background unchanged)
    ADITIVA,      // flag 0x10: additive, clamped
};

// Sprite attribute 104 -> blend, in the exe's priority order (0x40 / 0x20 / 0x200 / 0xc00 are not implemented).
constexpr ModoMezcla modo_de_atributo(u32 flags) {
    if (flags & 0x02) return ModoMezcla::OCULTO;
    if (flags & 0x08) return ModoMezcla::PANTALLA;
    if (flags & 0x101) return ModoMezcla::PROMEDIO;
    if (flags & 0x80) return ModoMezcla::MULTIPLICAR;
    if (flags & 0x10) return ModoMezcla::ADITIVA;
    return ModoMezcla::NORMAL;
}

// Map tile type byte -> blend (the loader 0x461ff0 maps 0x10 -> 0x8, 0x04 -> 0x100, 0x20 -> 0x80, in that order).
constexpr ModoMezcla modo_de_tile(u32 tipo) {
    if (tipo & 0x10) return ModoMezcla::PANTALLA;
    if (tipo & 0x04) return ModoMezcla::PROMEDIO;
    if (tipo & 0x20) return ModoMezcla::MULTIPLICAR;
    return ModoMezcla::NORMAL;
}

}  // namespace lin

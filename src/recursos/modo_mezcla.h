// Modo de mezcla de un sprite o tile del mapa, decodificado de los flags de atributos del objeto como el cliente original
// (renderizador de cuadro de S3EP1U 0x465c30, ver docs/fork/RE_SPRITES_RENDER.md).
#pragma once
#include "core/tipos.h"

namespace lin {

enum class ModoMezcla : u8 {
    NORMAL,
    OCULTO,       // flag 0x2: no se dibuja
    PANTALLA,     // flag 0x8: screen, max + min * (1 - max)
    PROMEDIO,     // flag 0x1 / 0x100: promedio del 50%
    MULTIPLICAR,  // flag 0x80: dst * src (el blanco deja el fondo sin cambios)
    ADITIVA,      // flag 0x10: additive, clamped
};

// Atributo 104 del sprite -> mezcla, en el orden de prioridad del exe (0x40 / 0x20 / 0x200 / 0xc00 no están implementados).
constexpr ModoMezcla modo_de_atributo(u32 flags) {
    if (flags & 0x02) return ModoMezcla::OCULTO;
    if (flags & 0x08) return ModoMezcla::PANTALLA;
    if (flags & 0x101) return ModoMezcla::PROMEDIO;
    if (flags & 0x80) return ModoMezcla::MULTIPLICAR;
    if (flags & 0x10) return ModoMezcla::ADITIVA;
    return ModoMezcla::NORMAL;
}

// Byte de tipo del tile del mapa -> mezcla (el cargador 0x461ff0 convierte 0x10 -> 0x8, 0x04 -> 0x100, 0x20 -> 0x80, en ese orden).
constexpr ModoMezcla modo_de_tile(u32 tipo) {
    if (tipo & 0x10) return ModoMezcla::PANTALLA;
    if (tipo & 0x04) return ModoMezcla::PROMEDIO;
    if (tipo & 0x20) return ModoMezcla::MULTIPLICAR;
    return ModoMezcla::NORMAL;
}

}  // namespace lin

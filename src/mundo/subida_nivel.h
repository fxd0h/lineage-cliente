#pragma once
#include "core/tipos.h"

namespace lin {

// The original client plays a local effect when the level reported by the server goes up (L1J sends no
// packet for it). Sprite 2127 is the "level up" fountain of light (sound 517 comes with its frames).
constexpr i32 GFX_SUBIDA_NIVEL = 2127;

// True when `ahora` is a level increase worth showing. `previo` == 0 means no level was known yet
// (login, first status packet, world cleared), which never counts.
constexpr bool subida_de_nivel(i32 previo, i32 ahora) { return previo > 0 && ahora > previo; }

}  // namespace lin

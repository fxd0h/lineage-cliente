#pragma once
#include "core/tipos.h"

namespace lin {

// El cliente original reproduce un efecto local cuando sube el nivel informado por el servidor (L1J no manda
// paquete para eso). El sprite 2127 es la fuente de luz de "level up" (el sonido 517 viene con sus cuadros).
constexpr i32 GFX_SUBIDA_NIVEL = 2127;

// True cuando `ahora` es una subida de nivel que vale mostrar. `previo` == 0 significa que todavía no se conocía nivel
// (login, primer paquete de estado, mundo limpio), y eso nunca cuenta.
constexpr bool subida_de_nivel(i32 previo, i32 ahora) { return previo > 0 && ahora > previo; }

}  // namespace lin

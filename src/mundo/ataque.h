// Ataque sostenido con el mouse (como el original): mantener el botón sobre un monstruo lo ataca sin
// parar, y si se suelta con el puntero fuera de la ventana el ataque sigue solo (a.ataque_sostenido).
#pragma once
#include "core/tipos.h"

namespace lin {

// ¿Está el punto (x, y), en coordenadas de la ventana, fuera de una ventana de ancho × alto?
inline bool punto_fuera_de_ventana(float x, float y, int ancho, int alto) {
    return x < 0 || y < 0 || x >= static_cast<float>(ancho) || y >= static_cast<float>(alto);
}

// Al soltar el botón izquierdo: ¿el ataque queda sostenido? Sí si se lo mantenía sobre un objetivo que
// se ataca (no un NPC al que se habla ni un ítem) y el puntero salió de la ventana al mantenerlo o al soltarlo.
inline bool sostener_ataque_al_soltar(bool objetivo_fijo, bool atacable, bool puntero_fuera) {
    return objetivo_fijo && atacable && puntero_fuera;
}

}  // namespace lin

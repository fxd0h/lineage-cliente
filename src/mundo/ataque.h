// Sustained mouse attack (like the original): holding the button on a monster attacks it continuously, and
// releasing the button over the bottom panel latches the attack (a.ataque_sostenido).
#pragma once
#include "core/tipos.h"

namespace lin {

// ¿Está el punto (x, y), en coordenadas de la ventana, fuera de una ventana de ancho × alto?
inline bool punto_fuera_de_ventana(float x, float y, int ancho, int alto) {
    return x < 0 || y < 0 || x >= static_cast<float>(ancho) || y >= static_cast<float>(alto);
}

// On left-button release: does the attack latch? Yes if it was held on an attackable target (not an NPC
// being talked to nor an item) and released over the bottom panel.
inline bool sostener_ataque_al_soltar(bool objetivo_fijo, bool atacable, bool released_on_panel) {
    return objetivo_fijo && atacable && released_on_panel;
}

// Overweight: the stock client has no weight check (the weight byte at 0x8f07f3 is only read by the HUD
// gauge, 0x4b2660/0x4b3d20/0x4b424b), so it still sends C_Attack / the spell packet. The server refuses at
// weight240 >= 197 (message 110 / 316) and sends no echo, and the client animates only on the echo.
// Our local prediction must therefore be skipped at the same threshold.
inline constexpr int PESO_SOBRECARGA = 197;  // on the 0..240 scale
inline bool sobrecargado(int peso240) { return peso240 >= PESO_SOBRECARGA; }

}  // namespace lin

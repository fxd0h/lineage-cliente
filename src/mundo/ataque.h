// Ataque sostenido con el mouse (como el original): mantener el botón sobre un monstruo lo ataca sin parar, y
// soltar el botón sobre el panel inferior deja el ataque enganchado (a.ataque_sostenido).
#pragma once
#include "core/tipos.h"

namespace lin {

// ¿Está el punto (x, y), en coordenadas de la ventana, fuera de una ventana de ancho × alto?
inline bool punto_fuera_de_ventana(float x, float y, int ancho, int alto) {
    return x < 0 || y < 0 || x >= static_cast<float>(ancho) || y >= static_cast<float>(alto);
}

// Al soltar el botón izquierdo: ¿queda enganchado el ataque? Sí, si se mantuvo sobre un objetivo atacable (no un NPC
// con el que se habla ni un objeto) y se soltó sobre el panel inferior.
inline bool sostener_ataque_al_soltar(bool objetivo_fijo, bool atacable, bool released_on_panel) {
    return objetivo_fijo && atacable && released_on_panel;
}

// Sobrepeso: el cliente original no revisa el peso (el byte de peso en 0x8f07f3 solo lo lee el indicador
// del HUD, 0x4b2660/0x4b3d20/0x4b424b), así que igual manda C_Attack / el paquete del hechizo. El servidor rechaza con
// weight240 >= 197 (mensaje 110 / 316) y no manda eco, y el cliente solo anima con el eco.
// Por eso nuestra predicción local debe saltarse con el mismo umbral.
inline constexpr int PESO_SOBRECARGA = 197;  // en la escala 0..240
inline bool sobrecargado(int peso240) { return peso240 >= PESO_SOBRECARGA; }

}  // namespace lin

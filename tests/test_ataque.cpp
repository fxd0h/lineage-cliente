// Ataque sostenido al sacar el puntero de la ventana (mundo/ataque.h).
#include <doctest/doctest.h>
#include "mundo/ataque.h"

using namespace lin;

TEST_CASE("punto_fuera_de_ventana: bordes") {
    CHECK_FALSE(punto_fuera_de_ventana(0, 0, 640, 480));
    CHECK_FALSE(punto_fuera_de_ventana(639.5f, 479.5f, 640, 480));
    CHECK(punto_fuera_de_ventana(-1, 10, 640, 480));
    CHECK(punto_fuera_de_ventana(10, -0.5f, 640, 480));
    CHECK(punto_fuera_de_ventana(640, 10, 640, 480));
    CHECK(punto_fuera_de_ventana(10, 480, 640, 480));
}

TEST_CASE("sostener_ataque_al_soltar: solo con objetivo fijo atacable y puntero fuera") {
    CHECK(sostener_ataque_al_soltar(true, true, true));
    CHECK_FALSE(sostener_ataque_al_soltar(false, true, true));  // no se mantenía sobre un objetivo
    CHECK_FALSE(sostener_ataque_al_soltar(true, false, true));  // NPC o ítem
    CHECK_FALSE(sostener_ataque_al_soltar(true, true, false));  // soltado dentro: se abandona
}

TEST_CASE("sobrecargado: threshold 197 on the 0..240 scale (server weight240 >= 197)") {
    CHECK_FALSE(sobrecargado(0));
    CHECK_FALSE(sobrecargado(196));
    CHECK(sobrecargado(197));
    CHECK(sobrecargado(240));
}

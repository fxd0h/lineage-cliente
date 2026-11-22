// Aura de bendición de los íconos (render/icon_aura.h): dilatación de la silueta opaca, sin GPU.
#include <doctest/doctest.h>
#include "render/icon_aura.h"

using namespace lin;

namespace {
u32 alpha_at(const ImagenRGBA& a, int x, int y) { return a.px[static_cast<size_t>(y) * a.ancho + x] >> 24; }
}

TEST_CASE("Aura: single pixel dilates as a plus of radius 1 and 2") {
    ImagenRGBA icon;
    icon.preparar(5, 5);
    icon.px[2 * 5 + 2] = 0xFF112233u;  // centro; en el aura de 9x9 queda en (4, 4)
    ImagenRGBA aura;
    build_bless_aura(icon, aura);
    REQUIRE(aura.ancho == 9);
    REQUIRE(aura.alto == 9);
    // exe 0x446099..: corrimientos en cruz de +/-2 y +/-1 px, sin centro; cada pasada es un tercio hacia el blanco.
    CHECK(alpha_at(aura, 4, 4) == 0);  // bajo el ícono, que se dibuja encima
    CHECK(alpha_at(aura, 5, 4) == 85);
    CHECK(alpha_at(aura, 4, 3) == 85);
    CHECK(alpha_at(aura, 6, 4) == 85);
    CHECK(alpha_at(aura, 4, 2) == 85);
    CHECK(alpha_at(aura, 5, 5) == 0);  // los vecinos diagonales quedan fuera de una dilatación en cruz
    CHECK(alpha_at(aura, 7, 4) == 0);
    CHECK(aura.px[4 * 9 + 6] == (0x00FFFFFFu | (85u << 24)));  // white
}

TEST_CASE("Aura: transparent icon gives an empty aura, composite keeps the icon") {
    ImagenRGBA icon;
    icon.preparar(4, 3);
    ImagenRGBA aura;
    build_bless_aura(icon, aura);
    for (u32 p : aura.px) CHECK((p >> 24) == 0u);
    icon.px[0] = 0xFF0000FFu;
    agregar_aura(icon);
    CHECK(icon.ancho == 12);
    CHECK(icon.alto == 7);
    CHECK(icon.px[0] == 0xFF0000FFu);
    CHECK(alpha_at(icon, 4 + 2 + 1, 2) == 85);  // la imagen del aura empieza en x = w = 4; 1 px a la derecha del píxel
}

TEST_CASE("Aura: both passes overlap at 5/9") {
    ImagenRGBA icon;
    icon.preparar(5, 5);
    icon.px[2 * 5 + 1] = icon.px[2 * 5 + 2] = icon.px[2 * 5 + 3] = 0xFF112233u;  // una fila de 3 px; aura (3..5, 4)
    ImagenRGBA aura;
    build_bless_aura(icon, aura);
    CHECK(alpha_at(aura, 4, 3) == 85);    // arriba del medio: solo la pasada de +/-1 (+/-2 no toca nada arriba)
    CHECK(alpha_at(aura, 4, 4) == 85);    // píxel del medio (bajo el ícono): solo los vecinos a 1 px son opacos
    CHECK(alpha_at(aura, 2, 4) == 142);   // extremo izquierdo de la fila del halo: a 1 px de (3,4), a 2 px de (4,4)
}

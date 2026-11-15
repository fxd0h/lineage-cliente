// Bless aura of icons (render/icon_aura.h): dilation of the opaque silhouette, without a GPU.
#include <doctest/doctest.h>
#include "render/icon_aura.h"

using namespace lin;

namespace {
u32 alpha_at(const ImagenRGBA& a, int x, int y) { return a.px[static_cast<size_t>(y) * a.ancho + x] >> 24; }
}

TEST_CASE("Aura: single pixel dilates as a plus of radius 1 and 2") {
    ImagenRGBA icon;
    icon.preparar(5, 5);
    icon.px[2 * 5 + 2] = 0xFF112233u;  // centre; in the 9x9 aura it is at (4, 4)
    ImagenRGBA aura;
    build_bless_aura(icon, aura);
    REQUIRE(aura.ancho == 9);
    REQUIRE(aura.alto == 9);
    CHECK(alpha_at(aura, 4, 4) == 142);
    CHECK(alpha_at(aura, 5, 4) == 142);
    CHECK(alpha_at(aura, 4, 3) == 142);
    CHECK(alpha_at(aura, 6, 4) == 85);
    CHECK(alpha_at(aura, 4, 2) == 85);
    CHECK(alpha_at(aura, 5, 5) == 0);  // diagonal neighbours are outside a plus-shaped dilation
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
    CHECK(alpha_at(icon, 4 + 2, 2) == 142);  // the aura image starts at x = w = 4
}

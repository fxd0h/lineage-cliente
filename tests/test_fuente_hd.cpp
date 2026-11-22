#include <doctest/doctest.h>
#include "recursos/fuente_hd.h"
#include "recursos/fuente_ttf.h"

using namespace lin;

namespace {
int campo(const ImagenRGBA& f, int x, int y) { return static_cast<int>(f.px[static_cast<size_t>(y) * f.ancho + x] & 0xFF); }
}  // namespace

TEST_CASE("HD field of a bitmap mask: size, margin, outline level") {
    ImagenRGBA m;
    m.preparar(4, 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 1; x < 3; ++x) m.px[y * 4 + x] = 0xFFFFFFFFu;  // 2 px wide vertical bar
    ImagenRGBA f;
    REQUIRE(campo_de_mascara(m, f));
    CHECK(f.ancho == (4 + 2 * HD_BORDE) * HD_ESCALA);
    CHECK(f.alto == (4 + 2 * HD_BORDE) * HD_ESCALA);
    CHECK(f.ox == -HD_BORDE);
    const int y = f.alto / 2;
    CHECK(campo(f, 0, y) < 32);  // empty margin
    CHECK(campo(f, f.ancho - 1, y) < 32);
    CHECK(campo(f, f.ancho / 2, y) > 200);  // dentro de la barra
    // El contorno (nivel 128) queda a menos de un texel del borde del píxel en x = 1 (+ margen).
    const int borde = (1 + HD_BORDE) * HD_ESCALA;
    CHECK(campo(f, borde - 2, y) < 128);
    CHECK(campo(f, borde + 2, y) > 128);
}

TEST_CASE("HD field: blank mask has no ink") {
    ImagenRGBA m, f;
    m.preparar(6, 12);
    CHECK_FALSE(campo_de_mascara(m, f));
}

TEST_CASE("HD field of the HTML face comes from real outlines") {
    ImagenRGBA f;
    REQUIRE(rasterizar_html_hd('M', f));
    CHECK(f.ancho == (ancho_html('M') + 2 * HD_BORDE) * HD_ESCALA);
    bool dentro = false;
    for (u32 p : f.px) dentro |= (p & 0xFF) == 0xFF;
    CHECK(dentro);
    CHECK_FALSE(rasterizar_html_hd(' ', f));
}

#include <doctest/doctest.h>
#include "render/vista.h"

using namespace lin;

TEST_CASE("vista fija: factor de ajuste y escala interna") {
    CHECK(factor_ajuste(1280, 960, 640, 480) == doctest::Approx(2.0f));
    CHECK(factor_ajuste(1000, 700, 640, 480) == doctest::Approx(1.4583f).epsilon(0.001));
    CHECK(escala_interna(1.4583f, true) == 1);
    CHECK(escala_interna(1.4583f, false) == 2);
    CHECK(escala_interna(2.0f, true) == 2);
    CHECK(escala_interna(2.0f, false) == 2);  // exact fit does not round up
    CHECK(escala_interna(0.7f, false) == 1);
    CHECK(escala_interna(0.7f, true) == 1);
    CHECK(escala_interna(9.0f, false) == 4);
}

TEST_CASE("vista fija: rectángulo con letterbox") {
    // 16:9 window, 4:3 grid: pillarbox, height-limited.
    RectVista r = rect_vista(1920, 1080, 640, 480, false);
    CHECK(r.h == 1080);
    CHECK(r.w == 1440);
    CHECK(r.x == 240);
    CHECK(r.y == 0);
    // Integer mode: largest integer multiple, centered.
    r = rect_vista(1920, 1080, 640, 480, true);
    CHECK(r.w == 1280);
    CHECK(r.h == 960);
    CHECK(r.x == 320);
    CHECK(r.y == 60);
    // Tall window: letterbox on top and bottom.
    r = rect_vista(800, 1000, 640, 480, false);
    CHECK(r.w == 800);
    CHECK(r.h == 600);
    CHECK(r.y == 200);
    // Smaller than the grid: integer mode falls back to the fractional fit.
    r = rect_vista(320, 240, 640, 480, true);
    CHECK(r.w == 320);
    CHECK(r.h == 240);
    // With the title strip the grid is 640 x 504.
    r = rect_vista(1000, 700, 640, 504, false);
    CHECK(r.h == 700);
    CHECK(r.w == 889);
    CHECK(r.x == 55);
}

TEST_CASE("vista fija: ventana -> cuadrícula") {
    const RectVista r = rect_vista(1920, 1080, 640, 480, false);  // 1440x1080 at x=240
    int gx = 0, gy = 0;
    pixel_a_grilla(240.0f, 0.0f, r, 640, 480, gx, gy);
    CHECK(gx == 0);
    CHECK(gy == 0);
    pixel_a_grilla(240.0f + 1439.0f, 1079.0f, r, 640, 480, gx, gy);
    CHECK(gx == 639);
    CHECK(gy == 479);
    pixel_a_grilla(960.0f, 540.0f, r, 640, 480, gx, gy);  // window centre = grid centre
    CHECK(gx == 320);
    CHECK(gy == 240);
    pixel_a_grilla(10.0f, 10.0f, r, 640, 480, gx, gy);  // over the black bar: outside the grid
    CHECK(gx < 0);
    pixel_a_grilla(1900.0f, 10.0f, r, 640, 480, gx, gy);
    CHECK(gx >= 640);
}

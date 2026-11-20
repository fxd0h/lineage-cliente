// La GPU de verdad: crea el dispositivo con los shaders de esta plataforma (MSL, DXIL o SPIR-V), dibuja
// con los tres pipelines (sprites normal y aditivo, multiplicar y reemplazar el mundo) y lee la captura.
// Sin GPU ni ventana (CI sin pantalla, sin Vulkan) se salta. En Linux sin GPU: xvfb-run con lavapipe.
#include <doctest/doctest.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "render/gpu.h"

using namespace lin;

namespace {

struct Color {
    u8 r, g, b;
};

Color pixel(const std::string& bmp, int x, int y) {
    Color c{0, 0, 0};
    if (SDL_Surface* s = SDL_LoadBMP(bmp.c_str())) {
        u8 a = 0;
        SDL_ReadSurfacePixel(s, x, y, &c.r, &c.g, &c.b, &a);
        SDL_DestroySurface(s);
    }
    return c;
}

std::string ruta_temporal(const char* nombre) {
    const char* tmp = std::getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = std::getenv("TEMP");  // Windows
    std::string r = tmp && *tmp ? tmp : "/tmp";
    if (r.back() != '/' && r.back() != '\\') r += '/';
    return r + nombre;
}

ImagenRGBA lisa(u32 color) {
    ImagenRGBA i;
    i.preparar(8, 8);
    for (u32& p : i.px) p = color;
    return i;
}

}  // namespace

TEST_CASE("gpu: los shaders de la plataforma dibujan lo esperado") {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        MESSAGE("sin video (" << std::string(SDL_GetError()) << "): se salta");
        return;
    }
    SDL_Window* v = SDL_CreateWindow("lin_tests", 320, 240, SDL_WINDOW_HIDDEN);
    Gpu g;
    if (!v || !g.iniciar(v, 320, 240)) {
        MESSAGE("sin GPU (" << std::string(SDL_GetError()) << "): se salta");
        g.cerrar();
        if (v) SDL_DestroyWindow(v);
        SDL_Quit();
        return;
    }
    RegionAtlas verde, azul;
    REQUIRE(g.subir(lisa(rgba(0, 255, 0)), verde));
    REQUIRE(g.subir(lisa(rgba(0, 0, 255)), azul));
    const std::string bmp = ruta_temporal("lin-test-gpu.bmp");

    // Un frame con: un rectángulo de la interfaz, un sprite del mundo y otro aditivo encima.
    auto frame = [&](auto antes_de_presentar) {
        g.ajustar_vista(false, 1);
        g.rectangulo(10, 10, 20, 20, clave_orden(CAPA_UI, 0), rgba(255, 0, 0));
        g.dibujar_en(verde, 100, 10, clave_orden(CAPA_MUNDO, 0));
        g.dibujar_en(azul, 104, 10, clave_orden(CAPA_MUNDO, 1), BLANCO, Mezcla::ADITIVA);
        antes_de_presentar();
        g.pedir_captura(bmp);
        g.presentar();
    };

    SUBCASE("sprites: normal, aditivo y fondo") {
        frame([] {});
        const Color rojo = pixel(bmp, 15, 15), solo_verde = pixel(bmp, 101, 11), suma = pixel(bmp, 106, 11);
        CHECK(rojo.r == 255);
        CHECK(rojo.g == 0);
        CHECK(solo_verde.g == 255);
        CHECK(solo_verde.b == 0);
        CHECK(suma.g == 255);  // aditivo: el azul se suma al verde sin taparlo
        CHECK(suma.b == 255);
        const Color fondo = pixel(bmp, 200, 200);
        CHECK(fondo.r + fondo.g + fondo.b == 0);
    }
    SUBCASE("multiplicar el mundo: tinte y brillo, sin tocar la interfaz") {
        const u8 brillo = 128;
        frame([&] { g.multiplicar_mundo(&brillo, 1, 1, 4096, rgba(255, 255, 255)); });
        const Color mundo = pixel(bmp, 101, 11), ui = pixel(bmp, 15, 15);
        CHECK(mundo.g >= 126);
        CHECK(mundo.g <= 130);
        CHECK(ui.r == 255);
    }
    SUBCASE("reemplazar el mundo: visión de elfo") {
        frame([&] { g.reemplazar_mundo(-1, 1); });
        const Color mundo = pixel(bmp, 101, 11), ui = pixel(bmp, 15, 15);
        CHECK(mundo.g >= 126);  // × 0,5 en verde
        CHECK(mundo.g <= 129);
        CHECK(ui.r == 255);
    }
    std::remove(bmp.c_str());
    g.cerrar();
    SDL_DestroyWindow(v);
    SDL_Quit();
}

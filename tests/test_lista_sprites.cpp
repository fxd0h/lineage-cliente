// Tabla de sprites del cliente en texto plano (TW13032701.txt, list.spz): lectura, fusión con
// sprite_table.txt y validación contra los .pak. Los que usan la tabla real se saltan sin ella:
// LISTA_SPRITES=<archivo>, o LINEAGE_DIR con TW13032701.txt / list.spz. (Adaptado del fork fxd0h.)
#include <doctest/doctest.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "comun.h"
#include "recursos/pak.h"
#include "recursos/sprites.h"
#include "recursos/tabla_sprites.h"

using namespace lin;

namespace {

std::string ruta_lista() {
    if (const char* e = std::getenv("LISTA_SPRITES"); e && *e) return e;
    const std::string j = carpeta_juego();
    if (j.empty()) return {};
    for (const char* rel : {"TW13032701.txt", "list.spz", "spritelist/list.spz"})
        if (std::filesystem::exists(j + rel)) return j + rel;
    return {};
}

bool cargar_tabla(TablaSprites& t) {
    return t.cargar_archivo("datos/sprite_table.txt") || t.cargar_archivo("../../datos/sprite_table.txt");
}

ContadorFrames contador_de(const Recursos& rec) {
    return [&rec](GfxId g, u16 sub) {
        char n[32];
        std::snprintf(n, sizeof n, "%u-%u.spr", g, sub);
        return contar_frames_spr(rec.sprite(n));
    };
}

}  // namespace

TEST_CASE("tabla del cliente: acciones, atributos, sufijos pegados y entradas en una línea") {
    TablaSprites t;
    ResumenLista r;
    const char* txt =
        "12414 0 41211\n"
        "#7 8=5 primero\n"
        "\t0.walk(1 2,0.0:2 0.1:3<478<479)\n"
        "\t3.breath(1 10.8.9:4 8.0:4)\n"  // en algunas filas la coma es un '.'
        "\t8.death(1 3,40.2:5 40.0:5[191 40.1:5)\n"
        "\t101.shadow(9)\n\t102.type(10)\n\t113.morph(5000)\n"
        "#8 1 misma linea 0.walk(1 1,0.0:2!) 110.framerate(12) 1.attack(1 1,8.0:3]77)\n"
        "#9 1 solo atributos\n\t102.type(0)\n\t100.see(1 0 -2 4 0)\n";
    CHECK(t.fusionar_lista(txt, r));
    CHECK(r.nuevos == 2);
    CHECK(r.atributos == 1);  // #9: solo el tipo, porque la tabla vacía no lo tenía
    CHECK(t.tipo(9) == 0);
    // Atributo 100: x en medias celdas (0..4 -> celdas 0..2), y en celdas (-2..0), relativo al objeto.
    CHECK(t.dentro_transparencia(9, 0, -2));
    CHECK(t.dentro_transparencia(9, 2, 0));
    CHECK_FALSE(t.dentro_transparencia(9, 3, 0));
    CHECK_FALSE(t.dentro_transparencia(9, 0, 1));
    CHECK_FALSE(t.tiene_transparencia(7));
    CHECK(t.gfx_archivos(7) == 5);
    CHECK(t.sombra(7) == 9);
    CHECK(t.tipo(7) == 10);
    CHECK(t.morfo(7) == 5000);
    CHECK(t.morfo(8) == 0);
    const RangoAccion* w = t.accion(7, ACC_CAMINAR);
    REQUIRE(w);
    CHECK(w->cantidad == 2);
    CHECK(t.frame(w->primer + 1).n_sonidos == 2);  // "<478<479": se conservan los dos, se elige uno al azar en cada muestra
    CHECK(t.frame(w->primer + 1).sonido == 478);
    CHECK(t.frame(w->primer + 1).sonido_mas[0] == 479);
    CHECK(t.frame(w->primer + 1).modo_sonido[0] == SONIDO_SOLAPA);
    const RangoAccion* b = t.accion(7, ACC_QUIETO);
    REQUIRE(b);
    CHECK(b->cantidad == 2);
    CHECK(t.frame(b->primer).sub == 8);
    CHECK(t.frame(b->primer).frame == 9);
    const RangoAccion* m = t.accion(7, ACC_MORIR);
    REQUIRE(m);
    CHECK(t.frame(m->primer).frame == 0);  // la muerte cae de 0 a n-1 y termina en el cadáver
    CHECK(t.frame(m->primer + 2).frame == 2);
    REQUIRE(t.accion(8, ACC_CAMINAR));
    CHECK(t.frame(t.accion(8, ACC_CAMINAR)->primer).impacto);
    REQUIRE(t.accion(8, ACC_ATACAR));
    CHECK(t.accion(8, ACC_ATACAR)->velocidad == 12);
    CHECK(t.accion(9, ACC_CAMINAR) == nullptr);
}

TEST_CASE("tabla del cliente: con un contador, se quitan los frames y alias que no están en los .pak") {
    TablaSprites t;
    ResumenLista r;
    // gfx 20 apunta a los archivos de 30, que no existen: vuelve a los propios. Su acción usa los frames 0 y 5,
    // y 20-0.spr tiene 3 imágenes: queda solo el 0. gfx 21 no tiene archivos: se omite.
    const char* txt = "#20 1=30 a\n\t0.walk(0 2,0.0:2 0.5:2)\n#21 1 b\n\t0.walk(0 1,0.0:2)\n";
    const ContadorFrames contador = [](GfxId g, u16 sub) { return g == 20 && sub == 0 ? 3u : 0u; };
    CHECK(t.fusionar_lista(txt, r, &contador));
    CHECK(r.nuevos == 1);
    CHECK(r.recortados == 1);
    CHECK(t.gfx_archivos(20) == 20);
    REQUIRE(t.accion(20, ACC_CAMINAR));
    CHECK(t.accion(20, ACC_CAMINAR)->cantidad == 1);
    CHECK(t.accion(21, ACC_CAMINAR) == nullptr);
}

TEST_CASE("tabla del cliente: los gfx que ya estaban en sprite_table.txt conservan sus filas" *
          doctest::skip(ruta_lista().empty())) {
    TablaSprites antes, despues;
    REQUIRE(cargar_tabla(antes));
    REQUIRE(cargar_tabla(despues));
    ResumenLista r;
    REQUIRE(despues.fusionar_lista_archivo(ruta_lista(), r));
    MESSAGE("tabla del cliente: " << r.nuevos << " nuevos, " << r.atributos << " solo atributos, " << r.reemplazados
                                  << " ya estaban, " << r.omitidos << " omitidos");
    for (GfxId gfx : {GfxId{1020}, GfxId{61}, GfxId{1112}}) {
        CAPTURE(gfx);
        for (u32 a = 0; a < 256; ++a) {
            const RangoAccion *x = antes.accion(gfx, static_cast<u8>(a)), *y = despues.accion(gfx, static_cast<u8>(a));
            REQUIRE((x == nullptr) == (y == nullptr));
            if (x) CHECK(x->cantidad == y->cantidad);
        }
    }
}

TEST_CASE("tabla del cliente: validada contra los .pak, cada frame nuevo existe" *
          doctest::skip(ruta_lista().empty() || carpeta_juego().empty())) {
    Recursos rec;
    REQUIRE(rec.abrir(carpeta_juego()));
    const ContadorFrames contador = contador_de(rec);
    TablaSprites t;
    REQUIRE(cargar_tabla(t));
    const size_t antes = t.cantidad_gfx();
    ResumenLista r;
    REQUIRE(t.fusionar_lista_archivo(ruta_lista(), r, &contador));
    MESSAGE("nuevos " << r.nuevos << ", sin archivo o redirigidos " << r.sin_archivo << ", con frames quitados " << r.recortados);
    CHECK(r.nuevos > 1000);
    // Young gilian (8913): no estaba en sprite_table.txt y ahora camina.
    const RangoAccion* w = t.accion(8913, ACC_CAMINAR);
    REQUIRE(w);
    CHECK(w->cantidad > 1);
    u32 malos = 0;
    for (GfxId g = static_cast<GfxId>(antes); g < t.cantidad_gfx(); ++g)
        for (u32 id = 0; id < 256; ++id)
            if (const RangoAccion* x = t.accion(g, static_cast<u8>(id)))
                for (u32 i = 0; i < x->cantidad; ++i) {
                    const FrameAccion& f = t.frame(x->primer + i);
                    const u32 c = contador(t.gfx_archivos(g), f.sub);
                    if (c > 0 && f.frame >= c) ++malos;
                }
    CHECK(malos == 0);
}

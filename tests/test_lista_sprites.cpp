// list.spz (plaintext sprite list, user-supplied and never shipped): parsing, merge into
// sprite_table.txt, and that common gfx resolve the same actions. Skipped when the file is absent.
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

// LISTA_SPRITES=<file>, else list.spz / spritelist/list.spz in (or next to) the game folder.
std::string ruta_lista() {
    if (const char* e = std::getenv("LISTA_SPRITES"); e && *e) return e;
    const std::string j = carpeta_juego();
    if (j.empty()) return {};
    for (const char* rel : {"list.spz", "spritelist/list.spz", "../spritelist/list.spz"})
        if (std::filesystem::exists(j + rel)) return j + rel;
    return {};
}

bool cargar_tabla(TablaSprites& t) {
    return t.cargar_archivo("datos/sprite_table.txt") || t.cargar_archivo("../../datos/sprite_table.txt");
}

bool iguales(const TablaSprites& a, const RangoAccion& ra, const TablaSprites& b, const RangoAccion& rb) {
    if (ra.cantidad != rb.cantidad || ra.direccional != rb.direccional || ra.velocidad != rb.velocidad) return false;
    for (u32 i = 0; i < ra.cantidad; ++i) {
        const FrameAccion &x = a.frame(ra.primer + i), &y = b.frame(rb.primer + i);
        if (x.sub != y.sub || x.frame != y.frame || x.duracion != y.duracion || x.impacto != y.impacto ||
            x.sonido != y.sonido || x.efecto != y.efecto)
            return false;
    }
    return true;
}

}  // namespace

TEST_CASE("list.spz parser: actions, attributes, glued suffixes and entries sharing a line") {
    TablaSprites t;
    ResumenLista r;
    const char* txt =
        "S3 0 0\n"
        "#7 8=5 first\n"
        "\t0.walk(1 2,0.0:2 0.1:3<478<479)\n"
        "\t3.breath(1 10.8.9:4 8.0:4)\n"  // the comma is a '.' in some rows
        "\t8.death(1 3,40.2:5 40.0:5[191 40.1:5)\n"
        "\t101.shadow(9)\n\t102.type(10)\n"
        "#8 1 same line 0.walk(1 1,0.0:2!) 110.framerate(12) 1.attack(1 1,8.0:3]77)\n"
        "#9 1 only attributes\n\t102.type(0)\n";
    CHECK(t.fusionar_lista(txt, r));
    CHECK(r.nuevos == 2);
    CHECK(r.omitidos == 1);
    CHECK(t.gfx_archivos(7) == 5);
    CHECK(t.sombra(7) == 9);
    CHECK(t.tipo(7) == 10);
    const RangoAccion* w = t.accion(7, 0);
    REQUIRE(w);
    CHECK(w->cantidad == 2);
    CHECK(t.frame(w->primer + 1).sonido == 479);
    const RangoAccion* b = t.accion(7, 3);
    REQUIRE(b);
    CHECK(b->cantidad == 2);
    CHECK(t.frame(b->primer).sub == 8);
    CHECK(t.frame(b->primer).frame == 9);
    const RangoAccion* m = t.accion(7, ACC_MORIR);
    REQUIRE(m);
    CHECK(t.frame(m->primer).frame == 0);  // rotated like sprite_table.txt death rows
    CHECK(t.frame(m->primer + 2).frame == 2);
    REQUIRE(t.accion(8, 0));
    CHECK(t.frame(t.accion(8, 0)->primer).impacto);
    REQUIRE(t.accion(8, 1));
    CHECK(t.accion(8, 1)->velocidad == 12);
    CHECK(t.frame(t.accion(8, 1)->primer).efecto == 77);
    CHECK(t.accion(9, 0) == nullptr);
}

TEST_CASE("list.spz: gfx 8913 (young gilian) walks with 8 frames" * doctest::skip(ruta_lista().empty())) {
    TablaSprites t;
    REQUIRE(cargar_tabla(t));
    CHECK(t.accion(8913, ACC_CAMINAR) == nullptr);
    ResumenLista r;
    REQUIRE(t.fusionar_lista_archivo(ruta_lista(), r));
    MESSAGE("sprite_table.txt: " << t.gfx_de_tabla() << " gfx; list.spz: " << r.nuevos << " new, " << r.reemplazados
                                 << " replaced, " << r.omitidos << " skipped");
    const RangoAccion* w = t.accion(8913, ACC_CAMINAR);
    REQUIRE(w);
    CHECK(w->cantidad == 8);
    CHECK(t.duracion_ms(8913, ACC_CAMINAR) > 0);
    CHECK(t.accion(8913, ACC_MORIR) != nullptr);
}

TEST_CASE("list.spz: gfx already in sprite_table.txt keep their rows" * doctest::skip(ruta_lista().empty())) {
    TablaSprites antes, despues;
    REQUIRE(cargar_tabla(antes));
    REQUIRE(cargar_tabla(despues));
    ResumenLista r;
    REQUIRE(despues.fusionar_lista_archivo(ruta_lista(), r));
    for (GfxId gfx : {GfxId{1020}, GfxId{61}, GfxId{1112}}) {
        CAPTURE(gfx);
        CHECK(antes.gfx_archivos(gfx) == despues.gfx_archivos(gfx));
        for (u32 a = 0; a < 256; ++a) {
            const RangoAccion* x = antes.accion(gfx, static_cast<u8>(a));
            const RangoAccion* y = despues.accion(gfx, static_cast<u8>(a));
            CAPTURE(a);
            REQUIRE((x == nullptr) == (y == nullptr));
            if (x) CHECK(iguales(antes, *x, despues, *y));
        }
    }
}

namespace {

// Image count of "<gfx>-<sub>.spr" in the user's paks.
ContadorFrames contador_de(const Recursos& rec) {
    return [&rec](GfxId g, u16 sub) {
        char n[32];
        std::snprintf(n, sizeof n, "%u-%u.spr", g, sub);
        return contar_frames_spr(rec.sprite(n));
    };
}

}  // namespace

TEST_CASE("list.spz: validated against our paks, every drawn frame exists" *
          doctest::skip(ruta_lista().empty() || carpeta_juego().empty())) {
    Recursos rec;
    REQUIRE(rec.abrir(carpeta_juego()));
    const ContadorFrames contador = contador_de(rec);
    TablaSprites t;
    REQUIRE(cargar_tabla(t));
    ResumenLista r;
    REQUIRE(t.fusionar_lista_archivo(ruta_lista(), r, &contador));
    MESSAGE("list.spz gfx with a missing/redirected file: " << r.sin_archivo << ", with frames dropped: " << r.recortados
                                                            << ", new: " << r.nuevos);

    // Young gilian: the walk (action 0) resolves to an existing image in all 8 headings.
    const RangoAccion* w = t.accion(8913, ACC_CAMINAR);
    REQUIRE(w);
    CHECK(w->cantidad > 0);
    for (u32 i = 0; i < w->cantidad; ++i)
        for (u16 d = 0; d < 8; ++d) {
            const FrameAccion& f = t.frame(w->primer + i);
            CAPTURE(i);
            CAPTURE(d);
            CHECK(f.frame < contador(t.gfx_archivos(8913), static_cast<u16>(f.sub + d)));
        }

    // Storage^Nodim: list.spz points 10669 at a file we lack; it must not stay redirected there.
    const GfxId a = t.gfx_archivos(10669);
    CHECK((a == 10669 || contador(a, 0) > 0));
    if (const RangoAccion* q = t.accion(10669, ACC_QUIETO))
        CHECK(t.frame(q->primer).frame < contador(a, t.frame(q->primer).sub));

    // Every action of every gfx list.spz added: all its frames exist (any heading).
    u32 malos = 0;
    for (GfxId g = 0; g < t.cantidad_gfx(); ++g)
        for (u32 id = 0; id < 256; ++id) {
            const RangoAccion* x = t.accion(g, static_cast<u8>(id));
            if (!x) continue;
            for (u32 i = 0; i < x->cantidad; ++i) {
                const FrameAccion& f = t.frame(x->primer + i);
                const u32 c = contador(t.gfx_archivos(g), f.sub);
                if (c > 0 && f.frame >= c && t.gfx_de_tabla() == 0) ++malos;
            }
        }
    CHECK(malos == 0);
}

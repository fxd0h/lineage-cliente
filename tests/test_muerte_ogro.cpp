// Ogre (gfx 1020; Contaminated Ogre 5687 es alias suyo): la acción de muerte debe resolverse y los cuadros de muerte
// de cada rumbo deben existir en la instalación. Se salta sin LINEAGE_DIR.
#include <doctest/doctest.h>
#include <cstdio>
#include "comun.h"
#include "recursos/pak.h"
#include "recursos/sprites.h"
#include "recursos/tabla_sprites.h"

using namespace lin;

TEST_CASE("Gfx 1020 (Ogre): la acción morir resuelve y sus frames existen" * doctest::skip(carpeta_juego().empty())) {
    TablaSprites t;
    if (!t.cargar_archivo("datos/sprite_table.txt") && !t.cargar_archivo("../../datos/sprite_table.txt")) return;
    Recursos r;
    REQUIRE(r.abrir(carpeta_juego()));
    for (GfxId gfx : {GfxId{1020}, GfxId{5687}}) {
        CAPTURE(gfx);
        const RangoAccion* m = t.accion(gfx, ACC_MORIR);
        REQUIRE(m);
        CHECK(m->cantidad == 7);  
        // La caída recorre los cuadros del archivo en orden y se queda en el último (acostado).
        for (u32 i = 0; i < m->cantidad; ++i) {
            CHECK(t.frame(m->primer + i).sub == 40);
            CHECK(t.frame(m->primer + i).frame == i);
        }
        const RangoAccion* dano = t.accion(gfx, ACC_DANADO);
        REQUIRE(dano);
        CHECK(dano->cantidad == 3);
        CHECK(t.accion(gfx, ACC_QUIETO) != nullptr);
        CHECK(t.duracion_ms(gfx, ACC_MORIR) > 0);
        for (u8 d = 0; d < 8; ++d)
            for (u32 i = 0; i < m->cantidad; ++i) {
                const FrameAccion& f = t.frame(m->primer + i);
                char n[48];
                std::snprintf(n, sizeof n, "%u-%u.spr", static_cast<unsigned>(t.gfx_archivos(gfx)),
                              static_cast<unsigned>(f.sub + (m->direccional ? d : 0)));
                CAPTURE(n);
                ArchivoSpr spr;
                REQUIRE(spr.abrir(r.sprite(n)));
                CAPTURE(f.frame);
                REQUIRE(f.frame < spr.frames.size());
                CHECK(spr.frames[f.frame].ntiles > 0);
            }
    }
}

TEST_CASE("Tabla de sprites: la fila morir gira el frame inicial al final") {
    // Las filas de la tabla listan primero el último cuadro del archivo ("8 1 4 40 3 6 40 0 5 40 1 4 40 2 4"): reproducido tal cual
    // el cadáver se vuelve a levantar. La caminata conserva la rotación (es un bucle).
    const char reg[] = "?48 0 1 4 0 0 3 0 1 3 0 2 3 0 3 3 8 1 4 40 3 6 40 0 5 40 1 4 40 2 4 102 10";
    std::vector<u8> t(reg, reg + sizeof reg - 1);
    TablaSprites s;
    REQUIRE(s.cargar(t));
    const RangoAccion* m = s.accion(0, ACC_MORIR);
    REQUIRE(m);
    REQUIRE(m->cantidad == 4);
    for (u32 i = 0; i < 4; ++i) CHECK(s.frame(m->primer + i).frame == i);
    CHECK(s.frame(m->primer + 3).duracion == 6);
    const RangoAccion* c = s.accion(0, ACC_CAMINAR);
    REQUIRE(c);
    CHECK(s.frame(c->primer).frame == 0);
}

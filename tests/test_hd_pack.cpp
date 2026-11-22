// HD UI pack (recursos/hd_pack.h): manifest parsing, logical/texel mapping, premultiplied alpha, HD aura.
#include <doctest/doctest.h>
#include "recursos/hd_pack.h"
#include "render/atlas.h"
#include "render/icon_aura.h"

using namespace lin;

TEST_CASE("HdPack: manifest entries, duplicates and variants") {
    HdPack p;
    p.leer_manifest(
        "name\tsha\tw\th\tox\toy\tduplicate_of\n"
        "img_7\taa\t10\t20\t-3\t4\t\n"
        "tbt_9\tbb\t32\t32\t0\t0\timg_7\n"
        "img_7h\tcc\t5\t5\t0\t0\t\n");
    CHECK(p.cantidad() == 2);  // "img_7h" no es direccionable
    u16 w, h;
    i16 ox, oy;
    REQUIRE(p.info(HdPack::Tipo::IMG, 7, w, h, ox, oy));
    CHECK(w == 10);
    CHECK(h == 20);
    CHECK(ox == -3);
    CHECK(oy == 4);
    CHECK(p.info(HdPack::Tipo::TBT, 9, w, h, ox, oy));
    CHECK_FALSE(p.info(HdPack::Tipo::TBT, 7, w, h, ox, oy));
    ImagenRGBA out;
    CHECK_FALSE(p.cargar(HdPack::Tipo::IMG, 7, out, 100, 100));  // no files: falls back cleanly
    CHECK_FALSE(p.cargar(HdPack::Tipo::IMG, 7, out, 39, 100));   // 40 texels wide does not fit
}

TEST_CASE("HdPack: premultiplied alpha") {
    ImagenRGBA img;
    img.preparar(2, 1);
    img.px[0] = 0x80FFFFFFu;  // a = 128, blanco
    img.px[1] = 0xFF102030u;  // opaco: sin cambios
    premultiplicar(img);
    CHECK((img.px[0] >> 24) == 0x80u);
    CHECK((img.px[0] & 255) == 128);
    CHECK(img.px[1] == 0xFF102030u);
}

TEST_CASE("HD region: logical size, texel mapping of clipped width") {
    RegionAtlas r;
    r.x = 100;
    r.w = 50;  // logical
    r.hd = 4;
    CHECK(r.escala_texel() == 4);
    CHECK(r.texel_w(30) == 120);  // una barra recortada a 30 px lógicos muestrea 120 texels
    CHECK(hd_texels(r.w, 4) == 200);
    RegionAtlas n;
    CHECK(n.escala_texel() == 1);
    CHECK(n.texel_w(30) == 30);
}

TEST_CASE("Aura at HD scale: padding and radius scale") {
    ImagenRGBA icon;
    icon.preparar(8, 8);
    icon.px[4 * 8 + 4] = 0xFF000000u;
    ImagenRGBA aura;
    build_bless_aura(icon, aura, 4);
    CHECK(aura.ancho == 8 + 16);
    CHECK((aura.px[(4 + 8) * 24 + 4 + 8] >> 24) == 0u);          // centro: bajo el ícono
    CHECK((aura.px[(4 + 8) * 24 + 4 + 8 + 4] >> 24) == 85u);     // a 1 px lógico
    CHECK((aura.px[(4 + 8) * 24 + 4 + 8 + 8] >> 24) == 85u);     // 2 logical px away
    agregar_aura(icon, 4);
    CHECK(icon.ancho == 2 * 8 + 16);
    CHECK(icon.alto == 8 + 16);
}

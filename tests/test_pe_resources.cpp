// PE resource reader: a synthetic PE built in the test, plus the real cursors if LINEAGE_DIR is set.
#include <doctest/doctest.h>
#include <cstring>
#include "comun.h"
#include "plataforma/archivo_mapeado.h"
#include "recursos/pe_resources.h"

using namespace lin;

namespace {

void put16(std::vector<u8>& b, size_t o, u32 v) { b[o] = v & 255; b[o + 1] = (v >> 8) & 255; }
void put32(std::vector<u8>& b, size_t o, u32 v) { put16(b, o, v & 0xFFFF); put16(b, o + 2, v >> 16); }

// One section ".rsrc" (rva 0x1000, raw 0x200) with type 1 / id 5 / lang 0x409 -> a 8x2 1-bpp cursor.
std::vector<u8> make_pe() {
    std::vector<u8> b(0x200 + 0x200, 0);
    b[0] = 'M'; b[1] = 'Z';
    put32(b, 0x3C, 0x80);
    put32(b, 0x80, 0x4550);
    put16(b, 0x86, 1);        // sections
    put16(b, 0x94, 0xE0);     // optional header size
    const size_t opt = 0x98;
    put16(b, opt, 0x10B);
    put32(b, opt + 96 + 16, 0x1000);  // resource directory RVA
    const size_t sec = opt + 0xE0;
    put32(b, sec + 8, 0x200); put32(b, sec + 12, 0x1000); put32(b, sec + 16, 0x200); put32(b, sec + 20, 0x200);
    const size_t r = 0x200;
    put16(b, r + 14, 1); put32(b, r + 16, 1); put32(b, r + 20, 0x80000018);          // type 1 -> dir @0x18
    put16(b, r + 0x18 + 14, 1); put32(b, r + 0x18 + 16, 5); put32(b, r + 0x18 + 20, 0x80000030);  // id 5
    put16(b, r + 0x30 + 14, 1); put32(b, r + 0x30 + 16, 0x409); put32(b, r + 0x30 + 20, 0x48);    // lang
    put32(b, r + 0x48, 0x1100); put32(b, r + 0x4C, 4 + 40 + 8 + 16);                            // data entry
    const size_t d = 0x300;  // raw of RVA 0x1100
    put16(b, d, 1); put16(b, d + 2, 1);  // hotspot
    put32(b, d + 4, 40); put32(b, d + 8, 8); put32(b, d + 12, 2 * 2);  // w=8, h=2*2 (XOR+AND)
    put16(b, d + 16, 1); put16(b, d + 18, 1);
    put32(b, d + 4 + 40 + 4, 0x00FFFFFF);  // palette: black, white
    const size_t xr = d + 4 + 40 + 8, an = xr + 8;
    // Bottom-up rows: row 0 (bottom) = XOR 0xF0 AND 0x0F; row 1 (top) = XOR 0xFF AND 0xFF.
    b[xr] = 0xF0;     b[an] = 0x0F;
    b[xr + 4] = 0xFF; b[an + 4] = 0xFF;
    return b;
}

}  // namespace

TEST_CASE("PE resources: synthetic cursor") {
    std::vector<u8> pe = make_pe();
    PeCursor c;
    REQUIRE(pe_load_cursor(pe, 5, c));
    CHECK(c.width == 8);
    CHECK(c.height == 2);
    CHECK(c.hot_x == 1);
    CHECK(c.hot_y == 1);
    CHECK(c.pixels[0] == PePixel::INVERT);       // top row: xor 1, and 1
    CHECK(c.pixels[8] == PePixel::WHITE);        // bottom row: xor 1, and 0
    CHECK(c.pixels[12] == PePixel::TRANSPARENT);  // xor 0, and 1
    CHECK(!pe_load_cursor(pe, 6, c));
    CHECK(pe_find_resource(pe, 2, 5).empty());
    // Truncated or corrupt inputs fail without crashing.
    for (size_t n : {size_t{0}, size_t{10}, size_t{0x90}, size_t{0x210}, size_t{0x2A0}, size_t{0x30C}}) {
        std::vector<u8> cut(pe.begin(), pe.begin() + static_cast<long>(n));
        CHECK(!pe_load_cursor(cut, 5, c));
    }
}

TEST_CASE("PE resources: original cursors" * doctest::skip(carpeta_juego().empty())) {
    ArchivoMapeado f;
    REQUIRE(f.abrir(ruta_real(carpeta_juego() + "S3EP1U.bin")));
    for (u32 id = 1; id <= 17; ++id) {
        PeCursor c;
        REQUIRE(pe_load_cursor(f.datos(), id, c));
        CHECK(c.width == 32);
        CHECK(c.height == 32);
        CHECK(c.hot_x < 32);
        CHECK(c.hot_y < 32);
    }
}

#include "recursos/big5.h"
#include "core/tipos.h"

namespace lin {

namespace {
constexpr u16 TABLA[] = {
#include "recursos/big5_tabla.inc"
};
constexpr int ANCHO_FILA = 0xFF - 0x40;

void agregar(std::string& s, u32 cp) {
    if (cp < 0x80) s += static_cast<char>(cp);
    else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
    else {
        s += static_cast<char>(0xE0 | (cp >> 12));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
}
}  // namespace

void big5_a_utf8(std::string_view e, std::string& salida) {
    salida.clear();
    for (size_t i = 0; i < e.size(); ++i) {
        const u8 c = static_cast<u8>(e[i]);
        if (c < 0x80) { salida += static_cast<char>(c); continue; }
        if (c >= 0x81 && c <= 0xFE && i + 1 < e.size()) {
            const u8 t = static_cast<u8>(e[i + 1]);
            if (t >= 0x40) {
                const u16 cp = TABLA[(c - 0x81) * ANCHO_FILA + (t - 0x40)];
                if (cp) { agregar(salida, cp); ++i; continue; }
            }
        }
        salida += '?';
    }
}

}  // namespace lin

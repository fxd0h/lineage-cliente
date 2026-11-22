#include <algorithm>
#include <iterator>
#include <utility>
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
            if (t >= 0x40 && t <= 0xFE) {  // 191 por fila: 0xFF se saldría de la tabla
                const u16 cp = TABLA[(c - 0x81) * ANCHO_FILA + (t - 0x40)];
                if (cp) { agregar(salida, cp); ++i; continue; }
            }
        }
        salida += '?';
    }
}

void utf8_a_big5(std::string_view e, std::vector<u8>& salida) {
    // Tabla inversa (código Unicode -> Big5), ordenada, armada una vez.
    static const std::vector<std::pair<u16, u16>> inversa = [] {
        std::vector<std::pair<u16, u16>> v;
        for (size_t i = 0; i < std::size(TABLA); ++i)
            if (TABLA[i]) v.emplace_back(TABLA[i], static_cast<u16>(((0x81 + i / ANCHO_FILA) << 8) | (0x40 + i % ANCHO_FILA)));
        std::sort(v.begin(), v.end());
        return v;
    }();
    for (size_t i = 0; i < e.size();) {
        const u8 c = static_cast<u8>(e[i]);
        if (c < 0x80) {
            salida.push_back(c);
            ++i;
            continue;
        }
        const size_t largo = (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        u32 cp = largo == 2 ? (c & 0x1Fu) : largo == 3 ? (c & 0x0Fu) : (c & 0x07u);
        bool ok = largo > 1 && i + largo <= e.size();
        for (size_t k = 1; ok && k < largo; ++k) {
            const u8 b = static_cast<u8>(e[i + k]);
            ok = (b & 0xC0) == 0x80;
            cp = (cp << 6) | (b & 0x3Fu);
        }
        i += ok ? largo : 1;
        const auto it = ok && cp <= 0xFFFF ? std::lower_bound(inversa.begin(), inversa.end(), std::pair<u16, u16>{static_cast<u16>(cp), 0})
                                           : inversa.end();
        if (it != inversa.end() && it->first == cp) {
            salida.push_back(static_cast<u8>(it->second >> 8));
            salida.push_back(static_cast<u8>(it->second));
        } else {
            salida.push_back('?');
        }
    }
}

}  // namespace lin

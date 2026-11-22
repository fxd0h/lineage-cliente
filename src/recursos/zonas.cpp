#include "recursos/zonas.h"
#include <charconv>

namespace lin {
namespace {
// Siguiente entero (separado por espacios) de t; avanza t. false si no hay.
bool entero(std::string_view& t, i32& v) {
    while (!t.empty() && t.front() == ' ') t.remove_prefix(1);
    auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
    if (ec != std::errc{}) return false;
    t.remove_prefix(static_cast<size_t>(p - t.data()));
    return true;
}
}  // namespace

bool TablaZonas::cargar(const Recursos& r) {
    zonas_.clear();
    if (!tabla_.cargar(r, "zone3-e.tbl")) return false;
    for (size_t i = 0; i < tabla_.cantidad(); ++i) {
        std::string_view l = tabla_.linea(i);
        if (i >= 1 && i <= 6) {  // "R G B ; comentario"
            i32 c[3];
            if (entero(l, c[0]) && entero(l, c[1]) && entero(l, c[2]))
                colores_[i - 1] = static_cast<u32>(c[0] << 16 | c[1] << 8 | c[2]);
            continue;
        }
        if (l.empty() || l.front() != '"') continue;
        const size_t fin = l.find('"', 1);
        if (fin == std::string_view::npos) continue;
        ZonaNombre z;
        z.nombre = l.substr(1, fin - 1);
        l.remove_prefix(fin + 1);
        i32 v[6];
        for (i32& x : v)
            if (!entero(l, x)) goto siguiente;
        z.clase = static_cast<u8>(v[0] < 0 || v[0] > 5 ? 0 : v[0]);
        z.mapa = static_cast<u16>(v[1]);
        z.x1 = v[2], z.y1 = v[3], z.x2 = v[4], z.y2 = v[5];
        zonas_.push_back(z);
    siguiente:;
    }
    return !zonas_.empty();
}

const ZonaNombre* TablaZonas::buscar(u16 mapa, i32 x, i32 y) const {
    const ZonaNombre* entera = nullptr;
    for (const ZonaNombre& z : zonas_) {
        if (z.mapa != mapa) continue;
        if (z.x2 == 0 && z.y2 == 0) {
            if (!entera) entera = &z;
        } else if (x >= z.x1 && x <= z.x2 && y >= z.y1 && y <= z.y2) {
            return &z;
        }
    }
    return entera;
}

}  // namespace lin

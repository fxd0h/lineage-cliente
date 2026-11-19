#include <cstring>
#include "mundo/inventario.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <iterator>

namespace lin {

namespace {

TextoRef guardar(Inventario& inv, std::string_view t) {
    if (inv.textos.size() > MAX_ARENA_TEXTOS)
        compactar_textos(inv.textos, [&](auto&& f) {
            for (Slot s = 0; s < inv.items.tamano(); ++s) {
                f(inv.items.col<I_NOMBRE>()[s]);
                f(inv.items.col<I_ATRIBUTOS>()[s]);
            }
        });
    TextoRef r{static_cast<u32>(inv.textos.size()), static_cast<u16>(t.size() > 0xFFFF ? 0xFFFF : t.size())};
    inv.textos.insert(inv.textos.end(), t.begin(), t.begin() + r.len);
    return r;
}

}  // namespace

std::string_view nombre_item(const Inventario& inv, Slot s) {
    TextoRef r = inv.items.col<I_NOMBRE>()[s];
    return {inv.textos.data() + r.ofs, r.len};
}

namespace {

// Agrega líneas con formato separadas por '\n' a un buffer fijo (se corta si no entra).
struct Lineas {
    char* salida;
    size_t capacidad;
    size_t n = 0;
    [[gnu::format(printf, 2, 3)]] void operator()(const char* fmt, ...) {
        if (capacidad == 0 || n + 1 >= capacidad) return;
        if (n > 0) salida[n++] = '\n';
        va_list va;
        va_start(va, fmt);
        const int w = std::vsnprintf(salida + n, capacidad - n, fmt, va);
        va_end(va);
        if (w > 0) n = std::min(capacidad - 1, n + static_cast<size_t>(w));
    }
};

}  // namespace

std::string_view atributos_item(const Inventario& inv, Slot s) {
    TextoRef r = inv.items.col<I_ATRIBUTOS>()[s];
    return {inv.textos.data() + r.ofs, r.len};
}

namespace {
// Greedy wrap at spaces to `ancho` columns (a longer word is cut). Returns false when the output is full.
void envolver(std::string_view t, size_t ancho, LineasTip& o, size_t max_lineas) {
    while (!t.empty() && o.n < max_lineas) {
        while (t.starts_with(' ')) t.remove_prefix(1);
        if (t.empty()) break;
        size_t corte = std::min(t.size(), ancho);
        if (t.size() > ancho) {
            const size_t sp = t.rfind(' ', ancho);
            if (sp != std::string_view::npos && sp > 0) corte = sp;
        }
        std::memcpy(o.texto[o.n], t.data(), corte);
        o.texto[o.n][corte] = 0;
        ++o.n;
        t.remove_prefix(corte);
    }
}
}  // namespace

void armar_tooltip_item(std::string_view nombre, std::string_view detalles, LineasTip& salida) {
    salida.n = salida.nombre = 0;
    if (nombre.size() <= 23) {
        std::memcpy(salida.texto[0], nombre.data(), nombre.size());
        salida.texto[0][nombre.size()] = 0;
        salida.n = 1;
    } else {
        envolver(nombre, 23, salida, 2);  // the exe splits once; the rest of a very long name is cut
    }
    salida.nombre = salida.n;
    while (!detalles.empty() && salida.n < LineasTip::MAX_LINEAS) {
        const size_t fin = std::min(detalles.find('\n'), detalles.size());
        const std::string_view linea = detalles.substr(0, fin);
        if (linea.empty()) salida.texto[salida.n][0] = 0, ++salida.n;
        else envolver(linea, 22, salida, LineasTip::MAX_LINEAS);
        detalles.remove_prefix(std::min(fin + 1, detalles.size()));
    }
}

size_t describir_atributos(std::string_view bytes, char* salida, size_t capacidad) {
    static constexpr const char* MATERIALES[] = {
        "none", "liquid", "web", "vegetation", "animal matter", "paper", "cloth", "leather", "wood", "bone",
        "dragon scale", "iron", "steel", "copper", "silver", "gold", "platinum", "mithril", "black mithril",
        "glass", "gemstone", "mineral", "oriharukon"};
    static constexpr const char* CLASES[] = {"Royal", "Knight", "Elf", "Wizard", "Dark Elf", "Dragon Knight", "Illusionist"};
    static constexpr const char* STATS[] = {"STR", "DEX", "CON", "WIS", "INT", "CHA"};
    static constexpr const char* ELEMENTOS[] = {"Fire", "Water", "Wind", "Earth"};
    static constexpr const char* RESISTENCIAS[] = {"Freeze", "Stone", "Sleep", "Blind", "Stun", "Hold"};
    LectorBinario l{Bytes(reinterpret_cast<const u8*>(bytes.data()), bytes.size())};
    Lineas linea{salida, capacidad};
    size_t& n = linea.n;
    auto material_y_peso = [&] {
        const u8 m = l.u8_();
        const u32 peso = l.u32_();
        if (m < std::size(MATERIALES) && m != 0) linea("Material: %s", MATERIALES[m]);
        linea("Weight %u", peso);
    };
    // Formato de L1ItemInstance.getStatusBytes: c etiqueta + su valor. Algunas etiquetas (1, 19,
    // 21, 22, 23) llevan detrás, sin etiqueta propia, el material y el peso.
    while (l.quedan(1) && !l.error) {
        const u8 et = l.u8_();
        switch (et) {
        case 1: {
            const u8 chico = l.u8_(), grande = l.u8_();
            linea("Damage %u (small) / %u (large)", chico, grande);
            material_y_peso();
            break;
        }
        case 19: {
            linea("AC %u", l.u8_());
            const u8 m = l.u8_();
            l.u8_();  // grado de los accesorios (o 0xFF)
            const u32 peso = l.u32_();
            if (m < std::size(MATERIALES) && m != 0) linea("Material: %s", MATERIALES[m]);
            linea("Weight %u", peso);
            break;
        }
        case 21: linea("Satiety %u", l.u16_()); material_y_peso(); break;
        case 22: linea("Light radius %u", l.u16_()); material_y_peso(); break;
        case 23: material_y_peso(); break;
        case 2: linea("Enchant %+d", l.i8_()); break;
        case 3: linea("Damaged %u", l.u8_()); break;
        case 4: linea("Two-handed"); break;
        case 5: linea("Hit %+d", l.i8_()); break;
        case 6: linea("Extra damage %+d", l.i8_()); break;
        case 7: {
            const u8 bits = l.u8_();
            if ((bits & 0x7F) == 0x7F) {
                linea("Usable by: all classes");
                break;
            }
            char clases[128];
            size_t m = 0;
            for (int i = 0; i < 7; ++i)
                if (bits & (1 << i)) {
                    const int w = std::snprintf(clases + m, sizeof clases - m, "%s%s", m ? ", " : "", CLASES[i]);
                    if (w > 0) m = std::min(sizeof clases - 1, m + static_cast<size_t>(w));
                }
            if (m) linea("Usable by: %s", clases);
            break;
        }
        case 8: case 9: case 10: case 11: case 12: case 13: linea("%s %+d", STATS[et - 8], l.i8_()); break;
        case 14: linea("HP %+d", l.i16_()); break;
        case 32: linea("MP %+d", l.i8_()); break;
        case 17: linea("SP %+d", l.i8_()); break;
        case 18: linea("Haste"); break;
        case 16: linea("Steals MP"); break;
        case 34: linea("Steals HP"); break;
        case 24: linea("Bow hit %+d", l.i8_()); break;
        case 35: linea("Bow damage %+d", l.i8_()); break;
        case 27: case 28: case 29: case 30: linea("%s resistance %+d", ELEMENTOS[et - 27], l.i8_()); break;
        case 15: {
            // MR, o, seguido de 33 y el tipo, resistencia a un estado.
            const i16 v = l.i16_();
            if (l.quedan(2) && l.datos[l.pos] == 33) {
                l.u8_();
                const u8 tipo = l.u8_();
                if (tipo >= 1 && tipo <= std::size(RESISTENCIAS)) linea("%s resistance %+d", RESISTENCIAS[tipo - 1], v);
            } else {
                linea("MR %+d", v);
            }
            break;
        }
        case 36: linea("EXP bonus %+d%%", l.i8_()); break;
        case 37: linea("HP regen %+d", l.i8_()); break;
        case 38: linea("MP regen %+d", l.i8_()); break;
        default: return n;  // etiqueta desconocida: no se sabe cuánto ocupa
        }
    }
    return n;
}

bool item_equipado(std::string_view n) {
    return n.find(" ($9)") != std::string_view::npos || n.find(" ($117)") != std::string_view::npos;
}

size_t orden_inventario(const Inventario& inv, Slot* salida, size_t cap) {
    const size_t n = std::min(inv.items.tamano(), cap);
    size_t k = 0;
    for (Slot s = 0; s < inv.items.tamano() && k < n; ++s)
        if (item_equipado(nombre_item(inv, s))) salida[k++] = s;
    // Worn items beyond `cap` (absurd inventory sizes) are not reordered: they stay in natural order below.
    for (Slot s = 0; s < inv.items.tamano() && k < n; ++s)
        if (!item_equipado(nombre_item(inv, s))) salida[k++] = s;
    return k;
}

void mover_items(Inventario& inv, std::span<const ObjId> mover, ObjId destino) {
    TablaItems& t = inv.items;
    const Slot sd = t.buscar(destino);
    if (sd == SIN_SLOT) return;
    auto se_mueve = [&](Slot s) { return std::find(mover.begin(), mover.end(), t.ids()[s]) != mover.end(); };
    if (se_mueve(sd)) return;
    Slot primero = SIN_SLOT;
    for (Slot s = 0; s < t.tamano() && primero == SIN_SLOT; ++s)
        if (se_mueve(s)) primero = s;
    if (primero == SIN_SLOT) return;
    std::vector<Slot> quedan, movidos;
    for (Slot s = 0; s < t.tamano(); ++s) (se_mueve(s) ? movidos : quedan).push_back(s);
    auto pos = std::find(quedan.begin(), quedan.end(), sd) + (sd < primero ? 0 : 1);
    quedan.insert(pos, movidos.begin(), movidos.end());
    t.reordenar(quedan);
}

void ordenar_por_ids(Inventario& inv, std::span<const ObjId> ids) {
    TablaItems& t = inv.items;
    std::vector<Slot> orden;
    std::vector<bool> puesto(t.tamano());
    orden.reserve(t.tamano());
    for (ObjId id : ids)
        if (Slot s = t.buscar(id); s != SIN_SLOT && !puesto[s]) {
            orden.push_back(s);
            puesto[s] = true;
        }
    for (Slot s = 0; s < t.tamano(); ++s)
        if (!puesto[s]) orden.push_back(s);
    t.reordenar(orden);
}

void aplicar_inventario(Inventario& inv, std::span<const Evento> eventos) {
    TablaItems& t = inv.items;
    for (const Evento& e : eventos) {
        switch (e.tipo) {
        case Ev::INVENTARIO_VACIAR:
            inv.limpiar();
            inv.recien_cargado = true;
            break;
        case Ev::ITEM: {
            Slot s = t.insertar(e.id);
            t.col<I_GFX>()[s] = static_cast<u16>(e.v[IT_GFX]);
            t.col<I_USO>()[s] = static_cast<u8>(e.v[IT_USO]);
            t.col<I_BENDICION>()[s] = static_cast<u8>(e.v[IT_BENDICION]);
            t.col<I_CANTIDAD>()[s] = static_cast<u32>(e.v[IT_CANTIDAD]);
            t.col<I_IDENTIFICADO>()[s] = static_cast<u8>(e.v[IT_IDENTIFICADO]);
            t.col<I_NOMBRE>()[s] = guardar(inv, e.s[0]);
            t.col<I_ATRIBUTOS>()[s] = guardar(inv, e.s[1]);
            break;
        }
        case Ev::ITEM_QUITAR: t.quitar(e.id); break;
        case Ev::ITEM_NOMBRE: {
            Slot s = t.buscar(e.id);
            if (s != SIN_SLOT) t.col<I_NOMBRE>()[s] = guardar(inv, e.s[0]);
            break;
        }
        case Ev::ITEM_CANTIDAD: {
            Slot s = t.buscar(e.id);
            if (s == SIN_SLOT) break;
            t.col<I_NOMBRE>()[s] = guardar(inv, e.s[0]);
            t.col<I_CANTIDAD>()[s] = static_cast<u32>(e.v[IT_CANTIDAD]);
            // S_ItemStatus / S_ItemAmount traen los atributos si está identificado (así llega la
            // identificación con el pergamino).
            t.col<I_ATRIBUTOS>()[s] = guardar(inv, e.s[1]);
            if (!e.s[1].empty()) t.col<I_IDENTIFICADO>()[s] = 1;
            break;
        }
        case Ev::ITEM_BENDICION: {
            Slot s = t.buscar(e.id);
            if (s != SIN_SLOT) t.col<I_BENDICION>()[s] = static_cast<u8>(e.v[IT_BENDICION]);
            break;
        }
        default: break;
        }
    }
}

void cargar_item_quest(const TablaTexto& tabla, std::vector<ItemMision>& salida) {
    // "<gfx> <desc entry>" per line, with an optional ";comment" (the exe reads the same two numbers).
    salida.clear();
    for (size_t i = 0; i < tabla.cantidad(); ++i) {
        const std::string l(tabla.linea(i));
        unsigned g = 0, d = 0;
        if (std::sscanf(l.c_str(), "%u %u", &g, &d) == 2) salida.push_back({static_cast<u16>(g), d});
    }
}

bool item_de_mision(const std::vector<ItemMision>& quest, const TablaTexto& desc, u16 gfx, std::string_view name) {
    // The server's name carries the count, enchantment and worn suffixes: "$N" or the plain name, then " (..)".
    if (const size_t p = name.find(" ("); p != std::string_view::npos) name = name.substr(0, p);
    while (!name.empty() && (name[0] == '+' || name[0] == '-')) {
        const size_t sp = name.find(' ');
        if (sp == std::string_view::npos) break;
        name.remove_prefix(sp + 1);
    }
    const std::string_view full = desc.resolver(name);
    for (const auto& q : quest)
        if (q.gfx == gfx && (full == desc.linea(q.desc) || name == desc.linea(q.desc))) return true;
    return false;
}

}  // namespace lin

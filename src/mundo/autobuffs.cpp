#include "mundo/autobuffs.h"
#include <iterator>
#include "mundo/autopocion.h"

namespace lin {

// S_SkillIconShield y S_SkillBrave: tipo -> hechizo según L1SkillUse.sendIcon del servidor.
u32 hechizo_de_efecto(const EfectoActivo& ef) {
    switch (ef.clase) {
    case EA_HASTE: return ef.tipo == 2 ? 29 : 43;  // Slow; Haste (también la poción)
    case EA_BRAVE: return ef.tipo == 4 ? 52 : ef.tipo == 6 ? 186 : 0;  // Holy Walk; Bloodlust
    case EA_ESCUDO:
        return ef.tipo == 5 ? 3 : ef.tipo == 3 ? 99 : ef.tipo == 7 ? 159 : ef.tipo == 10 ? 168 : ef.tipo == 6 ? 151 : 0;
    case EA_AURA: return ef.tipo + 1u;  // el índice del hechizo
    case EA_ICONO:
        for (u32 i = 0; i < std::size(POCIONES_AUTO); ++i)
            if (POCIONES_AUTO[i].icono == ef.tipo) return POCION_AUTO + i;
        return ef.tipo == 40 ? 68 : 0;  // Immune to Harm
    case EA_FUERZA: return ef.tipo == 5 ? 42 : ef.tipo == 2 ? 109 : 0;  // Enchant Mighty; Dress Mighty
    case EA_DESTREZA: return ef.tipo == 5 ? 26 : ef.tipo == 2 ? 110 : 0;  // Enchant Dexterity; Dress Dexterity
    case EA_HECHIZO: return ef.tipo;
    default: return 0;
    }
}

const PocionAuto* pocion_auto(u32 id) {
    return id >= POCION_AUTO && id - POCION_AUTO < std::size(POCIONES_AUTO) ? &POCIONES_AUTO[id - POCION_AUTO] : nullptr;
}

u32 pocion_auto_de_item(const Inventario& inv, Slot s) {
    const u32 n = numero_de_nombre(nombre_item(inv, s));
    const u16 gfx = inv.items.col<I_GFX>()[s];
    for (u32 i = 0; i < std::size(POCIONES_AUTO); ++i)
        if (n ? POCIONES_AUTO[i].n == n : POCIONES_AUTO[i].gfx == gfx) return POCION_AUTO + i;
    return 0;
}

ObjId buscar_pocion_auto(const Inventario& inv, u32 id) {
    const TablaItems& t = inv.items;
    for (Slot s = 0; s < t.tamano(); ++s)
        if (t.col<I_CANTIDAD>()[s] && pocion_auto_de_item(inv, s) == id) return t.ids()[s];
    return 0;
}

u64 resta_efecto_de_hechizo(const EstadoJugador& j, u32 hechizo, u64 ahora) {
    u64 resta = 0;
    for (u32 i = 0; i < j.n_efectos; ++i)
        if (hechizo_de_efecto(j.efectos[i]) == hechizo && j.efectos[i].fin > ahora) resta = std::max(resta, j.efectos[i].fin - ahora);
    return resta;
}

bool acepta_autobuff(const Hechizo& h) {
    return h.existe() && !h.de_ataque && h.duracion_s > 0 && h.sobre_item == Hechizo::SIN_ITEM;
}

int elegir_autobuff(const EstadoJugador& j, const CasilleroAuto* c, int n, u64 ahora) {
    if (j.muerto || j.inmovil) return -1;
    for (int i = 0; i < n; ++i) {
        const CasilleroAuto& k = c[i];
        if (!k.hechizo || ahora < k.reintentar_ms || j.mp < k.mp) continue;
        if (k.lanzado_ms && ahora < k.lanzado_ms + ESPERA_RESPUESTA_MS) continue;
        const u64 resta = resta_efecto_de_hechizo(j, k.hechizo, ahora);
        if (resta >= MARGEN_AUTO_MS) continue;
        if (!resta && !k.efecto_visto && k.lanzado_ms && ahora < k.lanzado_ms + k.duracion_ms) continue;  // no se ve
        return i;
    }
    return -1;
}

}  // namespace lin

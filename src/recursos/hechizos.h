// Hechizos y habilidades, con los datos del cliente original (Text.pak):
//  - spelldesc-e.tbl: "<índice> <Nombre> (<MP>/<HP>[/<material>])", el nombre que se muestra;
//  - spellexdesc-e.tbl: la ficha ("\fHTargets: \f>PC/NPC^..."), de donde sale el objetivo;
//  - spells: "<nombre> <índice> <Clase> mpCost=.. range=.. graphic=.. useDelay=.. ...;";
//  - spellicon.idx (en Sprite.pak): u16 por índice, el ícono del hechizo (N.tbt, 32×32).
// El hechizo de índice i es el skill id i + 1 del servidor (C_UseSkill: fila = i / 8, bit = i % 8).
//
// Contra la tabla skills de L1J-JP (183 hechizos):
//  - objetivo: coincide en 175. Los 8 restantes son casos especiales que el servidor resuelve
//    solo (un arma del inventario, el muro de fuego por celda, "Caster" que ataca al objetivo
//    actual...);
//  - acción con la que se lanza: las clases de "ataque" (CLASES_ATAQUE en hechizos.cpp) usan la
//    18 (SkillAttack) y las demás la 19 (SkillBuff), igual que action_id del servidor salvo 1.
#pragma once
#include <string_view>
#include <vector>
#include "recursos/pak.h"

namespace lin {

constexpr u32 CANTIDAD_HECHIZOS = 28 * 8;  // 28 filas (niveles) de 8 en S_AddSkill
// Diseño original del libro de hechizos (exe 2009, tablas de casillas cerca de 0x4424xx). Ids de clase: 0 príncipe, 1 caballero, 2 elfo, 3 mago,
// 4 elfo oscuro, 5 caballero dragón, 6 ilusionista. Una casilla es fila * 4 + columna; id de habilidad = idx + 1. Devuelve 0 para una casilla vacía.
constexpr u32 spellbook_skill_id(int cls, int slot) {
    if (slot < 0) return 0;
    auto in = [&](int lo, int hi) { return slot >= lo && slot <= hi; };
    int idx = -1;
    switch (cls) {
    case 0:  // prince
        if (in(0, 47)) idx = slot;
        else if (in(48, 55)) idx = slot - 48 + 112;
        break;
    case 2:  // elf
        if (in(0, 47)) idx = slot;
        else if (in(48, 98)) idx = slot - 48 + 128;
        break;
    case 4:  // dark elf
        if (in(0, 15)) idx = slot;
        else if (in(16, 19)) idx = slot - 16 + 96;
        else if (slot == 20) idx = 108;
        else if (in(24, 27)) idx = slot - 24 + 100;
        else if (slot == 28) idx = 109;
        else if (in(32, 35)) idx = slot - 32 + 104;
        else if (slot == 36) idx = 110;
        break;
    case 5:  // dragon knight
        if (in(0, 14)) idx = slot + 180;
        break;
    case 6:  // illusionist
        if (in(0, 19)) idx = slot + 200;
        break;
    case 1:  // caballero: la pestaña 2 tiene las habilidades de caballero
        if (in(0, 79)) {
            switch (slot) {
            case 8: idx = 86; break;
            case 9: idx = 88; break;
            case 10: idx = 87; break;
            case 13: idx = 89; break;
            case 14: idx = 90; break;
            default: idx = slot;
            }
        }
        break;
    default:  // mago y desconocidos
        if (in(0, 79)) idx = slot;
    }
    return idx < 0 ? 0 : static_cast<u32>(idx + 1);
}
// Primera fila de grilla más alta de la barra de scroll (límites de 0x47e9d0) y cantidad de pestañas de la imagen de la franja.
constexpr int spellbook_max_scroll(int cls) { return cls == 0 ? 13 : cls == 2 ? 21 : cls == 4 ? 9 : 19; }

constexpr u8 ACC_HECHIZO_ATAQUE = 18, ACC_HECHIZO = 19;  // ActionCodes.ACTION_SkillAttack / SkillBuff

struct Hechizo {
    std::string_view nombre;  // vacío si el índice no existe
    std::string_view ficha;   // spellexdesc-e.tbl, con los códigos de color (\f) y saltos (^)
    u16 mp = 0, hp = 0;
    u16 demora_ms = 0;        // useDelay: hasta el próximo hechizo (el servidor ignora los de antes)
    u16 gfx = 0;
    u16 icono = 0;            // N.tbt; 0 si no tiene
    u8 alcance = 0;           // en celdas; 0 = sin límite propio
    u16 duracion_s = 0;       // duration= de spells (los buffs)
    // Encanta un ítem que el cliente indica en C_UseSkill (SpellEnchantWeapon: un arma;
    // SpellBlessedArmor: una armadura); SpellEnchantCurrentWeapon usa la equipada sin indicarla.
    enum : u8 { SIN_ITEM, SOBRE_ARMA, SOBRE_ARMADURA } sobre_item = SIN_ITEM;
    bool sobre_arma_actual = false;  // SpellEnchantCurrentWeapon (Bless Weapon, Holy Weapon)
    // Área alrededor del lanzador (S_RangeSkill tipo 0): cómo se reparte el gráfico. SpellTornado
    // (Tornado, Fire Storm): 4 copias alrededor; SpellBlizzard (Blizzard, Freezing Blizzard): por
    // toda el área. `area` = areaEffect (celdas).
    enum : u8 { AREA_UNA, AREA_CUATRO, AREA_LLENA } reparto = AREA_UNA;
    u8 area = 0;
    bool con_objetivo = false;  // se elige un personaje o monstruo; si no, se lanza al instante
    bool de_ataque = false;     // se lanza con la acción 18; si no, con la 19
    bool existe() const { return !nombre.empty(); }
};

class TablaHechizos {
public:
    bool cargar(const Recursos& r);
    // Por skill id (1..CANTIDAD_HECHIZOS); uno vacío si no existe.
    const Hechizo& operator[](u32 id) const;

private:
    std::vector<u8> desc_, ficha_, spells_;
    std::vector<Hechizo> hechizos_;
};

// Clasificación del objetivo a partir del texto "Targets:" de la ficha y de si el hechizo tiene
// alcance (separada para los tests).
bool hechizo_con_objetivo(std::string_view objetivos, bool tiene_alcance);

}  // namespace lin

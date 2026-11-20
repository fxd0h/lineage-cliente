// Numeric columns of the original client's buff table (exe 0x6e1020, 154 rows of 20 bytes): classic icon id, duration
// in seconds (0 none, -1 until cleared), tooltip and end-of-effect
// message indices into string-e.tbl (read at runtime, `TablaTexto::linea(idx)`) and the exclusion group. Only numbers
// are embedded; no game text.
#pragma once
#include "core/tipos.h"
#include "mundo/mundo.h"

namespace lin {

constexpr int RANURAS_BUFF = 154;
inline constexpr u16 BUFF_MSG[154] = {
    849, 850, 851, 852, 853, 971, 972, 973, 974, 854, 855, 856, 964, 857, 858, 859, 860, 861, 862, 980, 981,
    863, 962, 963, 872, 864, 865, 866, 737, 867, 868, 869, 870, 871, 968, 969, 970, 872, 910, 911, 965, 966,
    872, 967, 982, 910, 994, 984, 985, 986, 987, 988, 1001, 1009, 1010, 1011, 1012, 1013, 1014, 975, 993,
    1000, 999, 912, 1043, 1044, 1083, 1084, 1085, 1086, 1087, 1088, 1089, 851, 1077, 1090, 1082, 1127, 1127,
    1140, 1141, 1142, 1143, 1152, 1153, 1154, 1155, 1156, 1157, 1158, 1159, 910, 1197, 1219, 1220, 1221,
    1222, 1223, 1224, 1225, 1226, 1292, 1300, 1313, 1314, 1315, 1316, 1329, 1330, 1331, 1332, 1333, 1334,
    1335, 1336, 1337, 1338, 1339, 1340, 1341, 1342, 1343, 1344, 1345, 1346, 1389, 1347, 1348, 1349, 1350,
    1351, 1381, 1382, 1391, 1392, 1393, 1402, 1403, 1404, 1405, 1406, 1407, 1408, 1409, 1436, 1437, 1438,
    1439, 1440, 1441, 1442, 1443, 1444, 1446,
};
inline constexpr u16 BUFF_FIN_MSG[154] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 718, 718, 719, 720, 721, 722, 723, 723, 723, 724, 724, 724, 832,
    825, 825, 825, 0, 827, 828, 829, 830, 831, 0, 0, 0, 832, 0, 0, 0, 0, 832, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
inline constexpr u8 BUFF_GRUPO[154] = {
    0, 0, 3, 0, 5, 5, 5, 5, 5, 6, 7, 0, 4, 4, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 3, 2, 2, 0, 0, 0, 0, 0, 0, 0,
    6, 7, 0, 3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

inline constexpr u16 BUFF_ICONO[154] = {
    481, 480, 486, 484, 485, 485, 763, 725, 783, 482, 483, 552, 777, 777, 813, 804, 807, 745, 774, 735, 738, 855, 771,
    827, 1110, 880, 1567, 1557, 890, 1101, 1113, 1122, 1125, 1119, 1558, 1559, 1608, 1110, 1478, 1477, 1556, 1562,
    1561, 1554, 1607, 795, 1609, 757, 754, 824, 760, 852, 1623, 1740, 852, 1834, 2339, 2706, 1736, 1563, 1612, 1620,
    1626, 1476, 1889, 1883, 2339, 2357, 2342, 2348, 2351, 2345, 2354, 486, 2382, 2430, 1609, 2433, 2432, 2566, 2563,
    2563, 2588, 2564, 2564, 2564, 2564, 2564, 2564, 2564, 2565, 1478, 2708, 2564, 2564, 2564, 2564, 2564, 2564, 2564,
    2565, 2565, 1612, 3069, 2430, 2430, 2430, 3080, 3187, 3078, 3075, 3190, 3078, 3089, 3188, 3078, 3123, 3211, 3117,
    3104, 3097, 3115, 3106, 3127, 3129, 3254, 3113, 3109, 3119, 3125, 3111, 3201, 2566, 1612, 1612, 3080, 2564, 2564,
    2564, 2564, 2564, 2564, 2564, 2565, 2430, 2430, 2430, 2430, 2430, 2430, 2430, 2430, 2430, 486,
};
inline constexpr i16 BUFF_SEG[154] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 600, 1800, 3600, 1200, 64, 1200, 600, 600, 600, 0, 0, 0, 0, 0, 0, 0, 60, 180,
    300, 300, 600, 600, 900, 900, 32, 0, 600, 0, 300, 60, 0, 10, 1800, 600, 320, 1800, 12, 12, 12, 31, 16, 30, 30, 30,
    960, 64, 192, -1, -1, -1, -1, 120, 32, 64, 100, 3000, 100, 1000, 3000, 3000, 5000, 600, 600, 0, 900, 900, 900,
    900, 900, 900, 3600, 900, 900, 900, 900, 900, 900, 900, 900, 600, 3600, 900, 900, 900, 900, 900, 900, 900, 900,
    900, -1, 3600, 3600, 3600, 3600, 1200, 60, -1, 60, 60, -1, 300, 60, -1, 1200, 8, 32, 8, 300, 32, 8, 600, 5, 12,
    32, 8, 300, 64, 32, 8, 600, -1, -1, 16, 900, 900, 900, 900, 900, 900, 900, 900, 3600, 3600, 3600, 3600, 3600,
    3600, 3600, 3600, 3600, 480,
};

// Duration of a slot in seconds: its table value; -1 (until cleared by the server) lasts a day; 0 means no timer.
inline i32 segundos_de_ranura(int ranura) {
    if (ranura < 0 || ranura >= RANURAS_BUFF) return 0;
    return BUFF_SEG[ranura] < 0 ? 86400 : BUFF_SEG[ranura];
}

// S_SkillSound on the own character -> buff slot (exe FUN_0048a5c0), -1 when the graphic has none.
inline int ranura_de_skillsound(u32 gfx) {
    struct F { u16 gfx, ranura; };
    static constexpr F TABLA[] = {
        {0xe4, 0x29}, {0xc1, 0xd}, {0x880, 0xd}, {0x875, 0xc}, {0x87a, 0x2f}, {0x87d, 0x2d}, {0x881, 0x31}, {0x885, 0x37},
        {0x888, 0xf}, {0x88a, 0xe}, {0x8b4, 0x33}, {0x8b6, 0x32}, {0x8b8, 0x30}, {0x8ba, 0x2b}, {0x80b, 0x2e}, {0x8c3, 0x2e},
        {0x8e8, 0x1c}, {0x8ed, 0x10}, {0xb7e, 0x1d}, {0xb82, 0x1e}, {0xb85, 0x1f}, {0xb86, 0x20}, {0xb84, 0x21},
        {0xf47, 0x24}, {0xf5f, 0x2c}, {0xf67, 0x28}, {0x112c, 0x35}, {0x1130, 0x38}, {0x16c1, 0x38}, {0x1131, 0x39},
        {0x1132, 0x3a}, {0x1228, 0x41}, {0x12d8, 0x40}, {0x16c2, 0x43}, {0x16c5, 0x44}, {0x16c6, 0x45}, {0x16c7, 0x46},
        {0x16c8, 0x47}, {0x16c9, 0x48}, {0x1969, 0x80}, {0x196a, 0x81}, {0x196b, 0x7b}, {0x196c, 0x6c}, {0x1977, 0x71},
        {0x197f, 0x78}, {0x1982, 0x7c}, {0x1983, 0x6b}, {0x19ba, 0x6f}, {0x19bc, 0x72}, {0x19af, 0x82}, {0x19b0, 0x7e},
        {0x19b1, 0x79}, {0x19b2, 0x76}, {0x1a34, 0x77}, {0x1a3a, 0x7a}, {0x1a40, 0x7f}, {0x1a41, 0x83}, {0x1a9d, 0x74},
        {0x1b0a, 0x73}, {0x1b3f, 0x6d}, {0x1b40, 0x70}, {0x1b51, 0x68}, {0x1b52, 0x69}, {0x1b53, 0x6a}, {0x1b65, 0x67},
        {0x1bc6, 0x99}, {0x1bd0, 0x80}, {0x1bd1, 0x7b}, {0x1bd2, 0x71}, {0x1bd3, 0x78}, {0x1bd4, 0x6b}, {0x1bd5, 0x74},
    };
    if (gfx >= 0x1bbd && gfx <= 0x1bc5) return 0x90 + static_cast<int>(gfx - 0x1bbd);
    for (const F& f : TABLA)
        if (f.gfx == gfx) return f.ranura;
    return -1;
}

// S_SkillSound 0xbe on the own character: the slot comes from the own sprite (exe 0x48a656), -1 when none.
inline int ranura_de_gfx_be(u32 sprite) {
    switch (sprite) {
    case 0x4b: case 0x5ca: case 0x9d3: case 0x9d8: return 0x26;
    case 0x9a1: return 0x5b;
    case 0xa26: return 0x4f;
    case 0x9aa: return 0x50;
    case 0x998: return 0x51;
    case 0xbb4: return 0x84;
    default: return -1;
    }
}

// Effect type of a S_SkillBrave packet: type 4 (Holy Walk) gets the class variant in the high nibble
// (class byte 2 elf -> 0, 3 wizard -> 1, others -> 2).
inline u8 tipo_brave(u8 tipo, int clase) { return tipo == 4 ? static_cast<u8>(4 | ((clase == 2 ? 0 : clase == 3 ? 1 : 2) << 4)) : tipo; }

// Buff-table slot of an active effect, -1 when unknown. Derived from the packet handlers of the original client
// (each calls sub_4647a0(duration, slot)); the Shield / Shadow Armor pair (slots 4 / 5) is inferred.
inline int ranura_de_efecto_activo(u8 clase, u8 tipo) {
    switch (clase) {
    case EA_HASTE: return tipo == 1 ? 0 : tipo == 2 ? 1 : -1;
    case EA_BRAVE:
        switch (tipo) {
        case 1: case 3: case 5: return 2;
        case 6: return 110;
        default:
            // Type 4 carries the class variant in the high nibble (see tipo_brave): elf 0x18, wizard 0x2a, others 0x25.
            if ((tipo & 15) != 4) return -1;
            return (tipo >> 4) == 0 ? 0x18 : (tipo >> 4) == 1 ? 0x2a : 0x25;
        }
    case EA_TERCERA: return 73;
    case EA_EVA: return 0xb;  // opcode 0x11 (S_SkillIconBlessOfEva), exe 0x48e380
    case EA_ESCUDO:
        switch (tipo) {
        case 5: return 4;
        case 3: return 5;
        case 6: return 6;
        case 7: return 7;
        case 10: return 8;
        default: return -1;
        }
    case EA_FUERZA: return tipo == 2 ? 34 : 9;
    case EA_DESTREZA: return tipo == 2 ? 35 : 10;
    case EA_ICONO:
        if (tipo >= EFECTO_RANURA) return tipo - EFECTO_RANURA < RANURAS_BUFF ? tipo - EFECTO_RANURA : -1;
        if (tipo == 34) return 38;
        if (tipo == 40) return 41;
        if (tipo == 0x39) return 3;
        return -1;
    case EA_AURA:
        switch (tipo) {
        case 113: return 25;
        case 114: return 26;
        case 116: return 27;
        case 147: return 18;
        case 148: return 21;
        case 154: return 19;
        case 155: return 22;
        case 162: return 20;
        case 165: return 23;
        case 119: return 0x1c;
        case 221: return 0x4d;
        case 222: return 0x4e;
        case 223: return 0x84;
        case 227: return 0x4a;
        default: return -1;
        }
    default: return -1;
    }
}

}  // namespace lin

// Numeric columns of the original client's buff table (exe 0x6e1020, 154 rows of 20 bytes): tooltip and end-of-effect
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

// Buff-table slot of an active effect, -1 when unknown. Derived from the packet handlers of the original client
// (each calls sub_4647a0(duration, slot)); the Shield / Shadow Armor pair (slots 4 / 5) is inferred.
inline int ranura_de_efecto_activo(u8 clase, u8 tipo) {
    switch (clase) {
    case EA_HASTE: return tipo == 1 ? 0 : tipo == 2 ? 1 : -1;
    case EA_BRAVE:
        switch (tipo) {
        case 1: case 3: case 5: return 2;
        case 4: return 24;
        case 6: return 110;
        default: return -1;
        }
    case EA_TERCERA: return 73;
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

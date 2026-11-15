// Nombres de zona del mapa (zone3-e.tbl de Text.pak): cada línea útil es
//   "Nombre" clase mapa x1 y1 x2 y2 ;comentario
// con el rectángulo en 0 si vale para todo el mapa. La clase (0..5) indexa los colores de las
// líneas 2..7 de la tabla (azul = campo, verde..púrpura según el nivel de la zona).
#pragma once
#include <string_view>
#include <vector>
#include "recursos/textos.h"

namespace lin {

struct ZonaNombre {
    std::string_view nombre;
    u8 clase = 0;
    u16 mapa = 0;
    i32 x1 = 0, y1 = 0, x2 = 0, y2 = 0;
};

class TablaZonas {
public:
    bool cargar(const Recursos& r);
    // Zona que contiene (x, y): la de un rectángulo que lo incluya, y si no la del mapa entero.
    // nullptr si el mapa no figura.
    const ZonaNombre* buscar(u16 mapa, i32 x, i32 y) const;
    // Color (0xRRGGBB) de una clase de zona.
    u32 color(u8 clase) const { return clase < 6 ? colores_[clase] : 0xFFFFFF; }

private:
    TablaTexto tabla_;
    std::vector<ZonaNombre> zonas_;
    u32 colores_[6] = {0x2D5FD1, 0x0AAD5A, 0xAE9D0E, 0xD58217, 0xDE3C3C, 0x850EBD};
};

}  // namespace lin

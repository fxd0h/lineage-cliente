// Juegos de tiles N.til (en Tile.pak).
//
// Formato: u16 cantidad, (cantidad+1) × u32 offsets relativos a los datos, datos. Cada tile empieza
// con un byte de tipo: si el bit 1 está activo es un tile con transparencia (cabecera x,y,ancho,alto
// y filas en tramos RGB555); si no, es un medio rombo sólido de 24×24 con filas de ancho
// 2,4,…,24,22,…,2,0 (288 píxeles RGB555). El mismo dato sirve como mitad izquierda (alineado a
// la derecha) o derecha (alineado a la izquierda) de la celda.
//
// Si además está el bit 2 (tipos 0x06/0x07) el tile es translúcido: son las sombras de árboles y
// casas (capas 1–9; todas grises oscuras o negras en el mapa 2005, y casi ninguna pieza que se
// levanta lo tiene). Se decodifican con alfa ALFA_TRANSLUCIDO: el original las mezclaba con el
// suelo en vez de pintarlas negras.
#pragma once
#include "recursos/imagen.h"

namespace lin {

struct JuegoTiles {
    Bytes datos;           // el .til completo
    u32 cantidad = 0;

    bool abrir(Bytes til);
    Bytes tile(u32 i) const;
    // Decodifica la mitad de un tile; si es un tile con transparencia la mitad no importa.
    bool decodificar(u32 i, bool derecha, ImagenRGBA& salida) const;
    bool translucido(u32 i) const;
    // Placeholder tile for "no ground" (type bit 0x08; only 47.til #199/#202): never drawn, the void is black.
    bool vacio(u32 i) const;
};

// Íconos de inventario N.tbt (en los SpriteNN.pak; N = gfx de inventario del ítem): el mismo
// formato que un tile con transparencia, sin el byte de tipo.
bool decodificar_tbt(Bytes tbt, ImagenRGBA& salida);

constexpr int ANCHO_MEDIO_TILE = 24;
constexpr int ALTO_TILE = 24;
constexpr u8 ALFA_TRANSLUCIDO = 128;
// Type byte bits that make a transparent-format tile translucent: 0x04 (shadows) and 0x20 (blood,
// bones and dust stains on dungeon floors: pale dithered pixels meant to be blended, not opaque).
constexpr u8 TILE_TRANSLUCIDO = 0x04 | 0x20;
constexpr u8 TILE_VACIO = 0x08;

}  // namespace lin

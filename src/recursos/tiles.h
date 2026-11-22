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
#include "recursos/modo_mezcla.h"

namespace lin {

struct JuegoTiles {
    Bytes datos;           // el .til completo
    u32 cantidad = 0;

    bool abrir(Bytes til);
    Bytes tile(u32 i) const;
    // Decodifica la mitad de un tile; si es un tile con transparencia la mitad no importa.
    bool decodificar(u32 i, bool derecha, ImagenRGBA& salida) const;
    bool translucido(u32 i) const;
    // Tile de luz (bit de tipo 0x10: llamas de velas/braseros y su brillo): se dibuja en forma aditiva, así su fondo
    // oscuro no suma nada en vez de verse como un rectángulo opaco.
    bool aditivo(u32 i) const;
    // Mezcla del tile (byte de tipo 0x10 screen, 0x04 promedio, 0x20 multiplicar, con esa prioridad).
    ModoMezcla modo(u32 i) const;
    // Tile de relleno para "sin suelo" (bit de tipo 0x08; solo 47.til #199/#202): nunca se dibuja, el vacío es negro.
    bool vacio(u32 i) const;
};

// Íconos de inventario N.tbt (en los SpriteNN.pak; N = gfx de inventario del ítem): el mismo
// formato que un tile con transparencia, sin el byte de tipo.
bool decodificar_tbt(Bytes tbt, ImagenRGBA& salida);

constexpr int ANCHO_MEDIO_TILE = 24;
constexpr int ALTO_TILE = 24;
constexpr u8 ALFA_TRANSLUCIDO = 128;
// Bit del byte de tipo que hace translúcido (promedio del 50%) un tile de formato transparente: 0x04 (sombras). El bit 0x20 (manchas de
// sangre, huesos y polvo en los pisos de las mazmorras) es una mezcla de multiplicar, la dibuja la GPU, no va incorporada en el alfa.
constexpr u8 TILE_TRANSLUCIDO = 0x04;
constexpr u8 TILE_VACIO = 0x08;
constexpr u8 TILE_ADITIVO = 0x10;

}  // namespace lin

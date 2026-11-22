// Fuente TTF de la interfaz (Departure Mono, Latin-1) rasterizada con stb_truetype. Los textos de la
// interfaz están en UTF-8 y se dibujan con ella; el texto del servidor (CP949) sigue en la fuente
// bitmap del juego (recursos/fuente.h). Los glifos son celdas de ALTO_GLIFO píxeles de alto con la
// línea base en la misma fila, así que el dibujo común (Graficos::texto) no cambia.
#pragma once
#include "core/tipos.h"
#include "recursos/imagen.h"

namespace lin {

// Espacios de glifos del atlas: base + punto de código Unicode. Las bases van de a 0x200000, más que el mayor
// punto de código (0x10FFFF): con 0x100000 entre ellas, los planos 15 y 16 de un espacio pisaban el siguiente.
// Glifos TTF de la interfaz.
constexpr u32 GLIFO_TTF_BASE = 0x200000;
// Glifos de la cara de las páginas HTML (Arial en el cliente original).
constexpr u32 GLIFO_HTML_BASE = 0x400000;
// La misma cara en negrita (<b>; reemplazo de la Arial Bold de GDI, con avances propios).
constexpr u32 GLIFO_HTML_NEGRITA_BASE = 0x600000;
static_assert(GLIFO_HTML_BASE - GLIFO_TTF_BASE > 0x10FFFF && GLIFO_HTML_NEGRITA_BASE - GLIFO_HTML_BASE > 0x10FFFF);
// Alto de línea de las páginas HTML en la interfaz original (tmHeight de la fuente de tamaño 13).
constexpr int ALTO_LINEA_HTML_ORIGINAL = 13;
int ancho_html(u32 codepunto, bool negrita = false);
bool rasterizar_html(u32 codepunto, ImagenRGBA& salida, bool negrita = false);
// Campo HD (recursos/fuente_hd.h) desde los contornos reales; false para glifos sin tinta.
bool rasterizar_html_hd(u32 codepunto, ImagenRGBA& salida, bool negrita = false);

// Avance del carácter en píxeles (proporcional). Un carácter que la fuente no tiene se dibuja como '?'.
int ancho_ttf(u32 codepunto);
// Celda RGBA premultiplicada (blanca con alfa), del ancho del avance y ALTO_GLIFO de alto.
bool rasterizar_ttf(u32 codepunto, ImagenRGBA& salida);

}  // namespace lin

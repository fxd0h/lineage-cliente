// Texto HD: "campos" de glifos para la pasada de texto posterior al filtro (ver docs/fork/hd_text.md).
//
// Un campo es una imagen de HD_ESCALA texels por píxel lógico cuyo valor cruza 0,5 justo en el contorno del glifo
// (adentro > 0,5). La GPU lo muestrea en forma bilineal y lo umbraliza con un smoothstep en espacio de pantalla, así el contorno es
// una curva suave independiente de la resolución: nítida a cualquier escala de pantalla. Las fuentes de bitmap obtienen su campo suavizando la
// máscara binaria (un vectorizador de curva implícita); la letra HTML se rasteriza desde sus contornos reales.
#pragma once
#include "core/tipos.h"
#include "recursos/imagen.h"

namespace lin {

constexpr int HD_ESCALA = 8;  // texels del campo por píxel lógico
constexpr int HD_BORDE = 1;   // píxeles lógicos de margen vacío alrededor de la celda del glifo (el contorno puede abultarse)

// Campo de una máscara de 1 bit. `mascara` son w*h píxeles RGBA (alfa != 0 = tinta; las celdas de la fuente de bitmap). `salida` queda
// de (w + 2 HD_BORDE) * HD_ESCALA de ancho, con ox = oy = -HD_BORDE (lógicos), el campo en R, G, B y A (blanco
// premultiplicado, así la subida al atlas no necesita caso especial). false si el glifo no tiene tinta.
bool campo_de_mascara(const ImagenRGBA& mascara, ImagenRGBA& salida);

// Campo desde una imagen de cobertura de 8 bits de `w` x `h` texels ubicada en el texel (x0, y0) de una celda de `cw` x `ch` píxeles
// lógicos (de HD_ESCALA texels cada uno). Se usa para letras rasterizadas desde contornos. false si está vacía.
bool campo_de_cobertura(const u8* cobertura, int w, int h, int x0, int y0, int cw, int ch, ImagenRGBA& salida);

}  // namespace lin

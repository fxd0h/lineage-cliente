// Aura de bendición de los íconos del inventario, como la arma el cliente original (S3EP1 FUN_00445ac0 en 0x445ac0).
//
// El original aclara 1/3 hacia el blanco (x' = (2x + 255) / 3) cada píxel de pantalla bajo la silueta opaca del ícono
// dilatada 2 px, y otra vez bajo la dilatada 1 px. Los píxeles de la dilatación de 1 px se aclaran así
// dos veces, o sea una capa blanca de alfa 1 - (2/3)^2 = 5/9; el anillo de 2 px una sola vez (alfa 1/3). El
// aura es esa capa precalculada como una imagen (blanca, con esos alfas), dibujada una vez con Mezcla::SILUETA.
// Dilatación = la silueta corrida (+-d, 0) y (0, +-d), unida consigo misma.
#pragma once
#include "recursos/imagen.h"

namespace lin {

constexpr int AURA_PAD = 4;  // la imagen del aura es 2 px más grande que el ícono por cada lado

// Arma el aura de (w + 4) x (h + 4) de `icon` (el ícono queda en (2, 2)).
// `scale` > 1: ícono HD (scale texels por píxel lógico); el relleno y los radios de dilatación escalan con él.
void build_bless_aura(const ImagenRGBA& icon, ImagenRGBA& aura, int scale = 1);
// Reemplaza `img` (w x h) por una imagen de (2w + 4) x (h + 4): el ícono en (0, 0) y su aura en (w, 0).
void agregar_aura(ImagenRGBA& img, int scale = 1);

}  // namespace lin

// Lector mínimo de recursos PE (ejecutable de Windows). Solo lo necesario para sacar los recursos RT_CURSOR
// monocromos del binario del juego (S3EP1U.bin). Con revisión de límites, sin excepciones.
#pragma once
#include <vector>
#include "core/tipos.h"

namespace lin {

enum class PePixel : u8 { BLACK, WHITE, TRANSPARENT, INVERT };

struct PeCursor {
    int width = 0, height = 0;  // top-down pixels
    int hot_x = 0, hot_y = 0;
    std::vector<PePixel> pixels;  // width * height
};

// Ubica los datos crudos de un recurso (tipo, id) en una imagen PE (primer idioma). Vacío si falta.
Bytes pe_find_resource(Bytes pe, u32 type_id, u32 res_id);

// Decodifica el RT_CURSOR `res_id` (hotspot de 4 bytes + DIB de 1 bpp con máscaras XOR y AND). False si
// falta, está mal formado o no es de 1 bpp.
bool pe_load_cursor(Bytes pe, u32 res_id, PeCursor& out);

// RT_GROUP_CURSOR `group_id` (el nombre de recurso de LoadCursor) -> el id RT_CURSOR de su primera imagen; 0 si falta.
u32 pe_cursor_group_image(Bytes pe, u32 group_id);

}  // namespace lin

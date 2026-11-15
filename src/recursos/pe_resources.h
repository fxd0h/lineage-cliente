// Minimal PE (Windows executable) resource reader. Only what is needed to pull the monochrome
// RT_CURSOR resources out of the game binary (S3EP1U.bin). Bounds-checked, no exceptions.
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

// Locates the raw data of a resource (type, id) in a PE image (first language). Empty if missing.
Bytes pe_find_resource(Bytes pe, u32 type_id, u32 res_id);

// Decodes RT_CURSOR `res_id` (4-byte hotspot + 1 bpp DIB with XOR and AND masks). False if it is
// missing, malformed or not 1 bpp.
bool pe_load_cursor(Bytes pe, u32 res_id, PeCursor& out);

}  // namespace lin

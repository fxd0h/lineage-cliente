// Optional HD UI pack: img_<id>.png / tbt_<id>.png at HD_SCALE x the original size, plus manifest.tsv
// (name, sha, w, h, ox, oy, duplicate_of). Images keep their original logical size and draw offsets;
// only the texel density changes. Not part of the repo: read from the directory given by `hd=` at runtime.
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/mapa_hash.h"
#include "recursos/imagen.h"

namespace lin {

constexpr int HD_SCALE = 4;

class HdPack {
public:
    enum class Tipo : u8 { IMG = 0, TBT = 1 };

    // Reads <dir>/manifest.tsv (the manifest may also sit in `manifest_dir`, default = dir). false if missing/empty.
    bool abrir(const std::string& dir);
    bool abierto() const { return !entradas_.empty(); }
    size_t cantidad() const { return entradas_.size(); }
    // Parses manifest text (separate from abrir for tests).
    void leer_manifest(std::string_view texto);

    // Logical size/offsets of an entry; false if the pack has no such image.
    bool info(Tipo t, u32 id, u16& w, u16& h, i16& ox, i16& oy) const;
    // Decodes the HD PNG into `out` (premultiplied alpha, ox/oy logical). false if absent, unreadable, of a
    // size other than HD_SCALE x logical, or larger than max_w x max_h texels.
    bool cargar(Tipo t, u32 id, ImagenRGBA& out, u32 max_w, u32 max_h) const;

private:
    struct Entrada {
        u16 w = 0, h = 0;
        i16 ox = 0, oy = 0;
        std::string archivo;  // file stem when the image is a duplicate of another; empty = own name
    };
    static u64 clave(Tipo t, u32 id) { return (static_cast<u64>(t) << 32) | id; }
    std::string dir_;
    std::vector<Entrada> entradas_;
    MapaHash indice_{16384};
};

// Straight alpha -> premultiplied (so bilinear filtering does not bleed the colour of transparent texels).
void premultiplicar(ImagenRGBA& img);

// Logical -> texel mapping of a clipped HD region: the first `logical` pixels cover logical * scale texels.
constexpr u32 hd_texels(u32 logical, u32 scale) { return logical * scale; }

}  // namespace lin

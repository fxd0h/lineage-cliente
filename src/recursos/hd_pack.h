// Pack HD opcional de la interfaz: img_<id>.png / tbt_<id>.png a HD_SCALE x el tamaño original, más manifest.tsv
// (nombre, sha, w, h, ox, oy, duplicate_of). Las imágenes conservan su tamaño lógico original y sus desplazamientos de dibujo;
// solo cambia la densidad de texels. No es parte del repo: se lee en ejecución de la carpeta que indica `hd=`.
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

    // Lee <dir>/manifest.tsv (el manifiesto también puede estar en `manifest_dir`, por omisión = dir). false si falta o está vacío.
    bool abrir(const std::string& dir);
    bool abierto() const { return !entradas_.empty(); }
    size_t cantidad() const { return entradas_.size(); }
    // Interpreta el texto del manifiesto (separado de abrir para los tests).
    void leer_manifest(std::string_view texto);

    // Tamaño/desplazamientos lógicos de una entrada; false si el pack no tiene esa imagen.
    bool info(Tipo t, u32 id, u16& w, u16& h, i16& ox, i16& oy) const;
    // Decodifica el PNG HD en `out` (alfa premultiplicado, ox/oy lógicos). false si falta, no se puede leer, tiene un
    // tamaño distinto de HD_SCALE x el lógico, o es mayor que max_w x max_h texels.
    bool cargar(Tipo t, u32 id, ImagenRGBA& out, u32 max_w, u32 max_h) const;

private:
    struct Entrada {
        u16 w = 0, h = 0;
        i16 ox = 0, oy = 0;
        std::string archivo;  // nombre base del archivo cuando la imagen es duplicado de otra; vacío = nombre propio
    };
    static u64 clave(Tipo t, u32 id) { return (static_cast<u64>(t) << 32) | id; }
    std::string dir_;
    std::vector<Entrada> entradas_;
    MapaHash indice_{16384};
};

// Alfa directo -> premultiplicado (así el filtrado bilineal no corre el color de los texels transparentes).
void premultiplicar(ImagenRGBA& img);

// Correspondencia lógico -> texel de una región HD recortada: los primeros `logical` píxeles cubren logical * scale texels.
constexpr u32 hd_texels(u32 logical, u32 scale) { return logical * scale; }

}  // namespace lin

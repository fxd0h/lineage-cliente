// Contenedores .idx/.pak del juego original.
//
// .idx: u32 cantidad + cantidad × {u32 offset, char nombre[20], u32 tamaño}. En Text.idx las
// entradas van cifradas con DES. El .pak se mapea en memoria y un archivo es un slice de él.
//
// Sprites: un archivo se busca primero en SpriteNN.pak, con NN = (suma de los bytes del nombre)
// & 15, y si no está, en Sprite.pak (getHashIndex de l1j-client). Los SpriteNN tienen las
// versiones más nuevas (p. ej. 814.png del login está en Sprite00 y no en Sprite.pak).
#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <string_view>
#include <vector>
#include "plataforma/archivo_mapeado.h"

namespace lin {

class Pak {
public:
    bool abrir(const std::string& ruta_sin_extension, bool indice_cifrado, bool avisar = true);  // avisar: al log si falla
    // Devuelve el contenido (vista al archivo mapeado) o vacío si no existe. Nombre en minúsculas.
    Bytes buscar(std::string_view nombre) const;
    size_t cantidad() const { return entradas_.size(); }
    // Para el explorador: nombre de la entrada i.
    std::string_view nombre(size_t i) const { return entradas_[i].nombre; }

private:
    struct Entrada {
        u64 hash;
        u32 offset, tam;
        char nombre[21];
    };
    std::vector<Entrada> entradas_;  // ordenadas por hash para búsqueda binaria
    ArchivoMapeado datos_;
};

u64 hash_nombre(std::string_view nombre);  // FNV-1a de 64 bits sobre el nombre en minúsculas

// Todos los contenedores del juego. La primera fuente es la instalación principal (juego=); las siguientes son
// las carpetas de juego_extra=, probadas en orden para lo que les falte a las anteriores (sprites, tiles, archivos sueltos de
// sprite/ y map/, páginas html). Las tablas de texto (.tbl, .def, hechizos) salen solo de la principal.
class Recursos {
public:
    bool abrir(const std::string& carpeta_juego, const std::vector<std::string>& extras = {});
    const std::string& carpeta() const { return carpeta_; }

    Bytes sprite(std::string_view nombre) const;  // SpriteNN.pak, Sprite.pak, loose sprite/ per source
    Bytes tile(std::string_view nombre) const;    // Tile.pak, loose Tile/ per source
    // Text.pak: los archivos también van cifrados con DES; se devuelven ya descifrados.
    bool texto(std::string_view nombre, std::vector<u8>& salida) const;
    // Primera fuente que tiene `relativa` (p. ej. "map/4/7fff8000.s32", "sound/12.wav"); la ruta de la principal si ninguna.
    std::string ruta_archivo(const std::string& relativa) const;
    // No se registra qué fuente (0 = principal) atendió las últimas búsquedas; esto dice dónde se resuelve el nombre de un sprite.
    // La misma búsqueda pero solo en Sprite.pak (el archivo base): se saltan los parches SpriteNN.pak con hash. El cliente 2009 capturado
    // todavía muestra el arte base de algunas imágenes de la interfaz (botones de la franja inferior) que los parches reemplazaron.
    Bytes sprite_base(std::string_view nombre) const;
    int fuente_de_sprite(std::string_view nombre) const;
    size_t fuentes() const { return fuentes_.size(); }
    const std::string& carpeta_de(size_t i) const { return fuentes_[i]->carpeta; }

    const Pak& pak_sprites_base() const { return fuentes_[0]->sprite_base; }
    const Pak& pak_textos() const { return fuentes_[0]->texto; }

private:
    struct Fuente {
        std::string carpeta;
        Pak sprite_base, sprite_hash[16], tile, texto;
        mutable std::mutex mu;  // caché de archivos sueltos (sprite() se llama desde varios hilos)
        mutable std::unordered_map<std::string, ArchivoMapeado> sueltos;  // mapeo vacío = se sabe que falta
    };
    static Bytes suelto(const Fuente& f, std::string_view dir, std::string_view nombre);
    static Bytes en_fuente(const Fuente& f, std::string_view nombre);
    std::string carpeta_;
    std::vector<std::unique_ptr<Fuente>> fuentes_;
};

}  // namespace lin

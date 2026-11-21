#include <algorithm>
#include <cstdio>
#include <filesystem>
#include "recursos/des.h"
#include "recursos/pak.h"

namespace lin {

static char minuscula(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

u64 hash_nombre(std::string_view nombre) {
    u64 h = 0xcbf29ce484222325ull;
    for (char c : nombre) {
        h ^= static_cast<u8>(minuscula(c));
        h *= 0x100000001b3ull;
    }
    return h;
}

bool Pak::abrir(const std::string& base, bool indice_cifrado, bool avisar) {
    std::vector<u8> idx;
    if (!leer_archivo(base + ".idx", idx) || idx.size() < 4) {
        if (avisar) LIN_LOG("No se pudo leer %s.idx", base.c_str());
        return false;
    }
    if (!datos_.abrir(base + ".pak")) {
        if (avisar) LIN_LOG("No se pudo abrir %s.pak", base.c_str());
        return false;
    }
    LectorBinario l{idx};
    u32 n = l.u32_();
    if (static_cast<size_t>(n) * 28 + 4 > idx.size()) return false;
    if (indice_cifrado) Des(CLAVE_DES_LINEAGE).descifrar(idx.data() + 4, static_cast<size_t>(n) * 28);
    entradas_.resize(n);
    size_t validas = 0;
    for (u32 i = 0; i < n; ++i) {
        Entrada& e = entradas_[validas];
        e.offset = l.u32_();
        Bytes nom = l.tomar(20);
        e.tam = l.u32_();
        size_t len = 0;
        while (len < 20 && nom[len]) {
            e.nombre[len] = minuscula(static_cast<char>(nom[len]));
            ++len;
        }
        e.nombre[len] = 0;
        // Se descartan entradas que salen del .pak (el índice original tiene algunas rotas).
        if (static_cast<u64>(e.offset) + e.tam > datos_.datos().size()) continue;
        e.hash = hash_nombre({e.nombre, len});
        ++validas;
    }
    entradas_.resize(validas);
    // Ante nombres repetidos gana la última entrada del índice: stable_sort + búsqueda por el final.
    std::stable_sort(entradas_.begin(), entradas_.end(), [](const Entrada& a, const Entrada& b) { return a.hash < b.hash; });
    return true;
}

Bytes Pak::buscar(std::string_view nombre) const {
    u64 h = hash_nombre(nombre);
    auto it = std::upper_bound(entradas_.begin(), entradas_.end(), h, [](u64 v, const Entrada& e) { return v < e.hash; });
    if (it == entradas_.begin() || (it - 1)->hash != h) return {};
    const Entrada& e = *(it - 1);
    return datos_.datos().subspan(e.offset, e.tam);
}

bool Recursos::abrir(const std::string& carpeta, const std::vector<std::string>& extras) {
    carpeta_ = carpeta;
    fuentes_.clear();
    bool ok = true;
    auto agregar = [&](const std::string& c, bool primaria) {
        auto f = std::make_unique<Fuente>();
        f->carpeta = c;
        bool bien = f->sprite_base.abrir(c + "Sprite", false, primaria);
        bien = f->tile.abrir(c + "Tile", false, primaria) && bien;
        bien = f->texto.abrir(c + "Text", true, primaria) && bien;
        char nombre[16];
        for (int i = 0; i < 16; ++i) {
            std::snprintf(nombre, sizeof nombre, "Sprite%02d", i);
            f->sprite_hash[i].abrir(c + nombre, false, false);  // opcionales: instalaciones viejas no los tienen
        }
        if (primaria) ok = bien;
        fuentes_.push_back(std::move(f));  // an extra without paks still serves its loose files
    };
    agregar(carpeta, true);
    for (const std::string& e : extras) {
        if (e.empty()) continue;
        agregar(e.back() == '/' ? e : e + '/', false);
    }
    return ok;
}

// Loose file <dir>/<nombre> of a source (case-insensitive on Linux), mapped once and cached.
Bytes Recursos::suelto(const Fuente& f, std::string_view dir, std::string_view nombre) {
    std::string clave(dir);
    clave += nombre;
    std::lock_guard l(f.mu);
    auto it = f.sueltos.find(clave);
    if (it == f.sueltos.end()) {
        ArchivoMapeado m;
        std::error_code ec;
        const std::string ruta = ruta_real(f.carpeta + clave);
        if (std::filesystem::is_regular_file(ruta, ec)) m.abrir(ruta);
        it = f.sueltos.emplace(std::move(clave), std::move(m)).first;
    }
    return it->second.datos();
}

Bytes Recursos::en_fuente(const Fuente& f, std::string_view nombre) {
    unsigned suma = 0;
    for (char c : nombre) suma += static_cast<u8>(minuscula(c));
    Bytes b = f.sprite_hash[suma & 15].buscar(nombre);
    if (b.empty()) b = f.sprite_base.buscar(nombre);
    return b.empty() ? suelto(f, "sprite/", nombre) : b;
}

Bytes Recursos::sprite(std::string_view nombre) const {
    for (const auto& f : fuentes_)
        if (Bytes b = en_fuente(*f, nombre); !b.empty()) return b;
    return {};
}

Bytes Recursos::sprite_base(std::string_view nombre) const {
    for (const auto& f : fuentes_)
        if (Bytes b = f->sprite_base.buscar(nombre); !b.empty()) return b;
    return {};
}

int Recursos::fuente_de_sprite(std::string_view nombre) const {
    for (size_t i = 0; i < fuentes_.size(); ++i)
        if (!en_fuente(*fuentes_[i], nombre).empty()) return static_cast<int>(i);
    return -1;
}

Bytes Recursos::tile(std::string_view nombre) const {
    for (const auto& f : fuentes_) {
        Bytes b = f->tile.buscar(nombre);
        if (b.empty()) b = suelto(*f, "Tile/", nombre);
        if (!b.empty()) return b;
    }
    return {};
}

static bool es_html(std::string_view n) {
    auto fin = [&](std::string_view e) { return n.size() >= e.size() && n.substr(n.size() - e.size()) == e; };
    return fin(".html") || fin(".htm");
}

bool Recursos::texto(std::string_view nombre, std::vector<u8>& salida) const {
    const size_t n = es_html(nombre) ? fuentes_.size() : 1;  // text ids only from the primary
    for (size_t i = 0; i < n; ++i) {
        Bytes b = fuentes_[i]->texto.buscar(nombre);
        if (b.empty()) continue;
        salida.assign(b.begin(), b.end());
        Des(CLAVE_DES_LINEAGE).descifrar(salida.data(), salida.size());
        return true;
    }
    return false;
}

std::string Recursos::ruta_archivo(const std::string& relativa) const {
    std::error_code ec;
    for (const auto& f : fuentes_)
        if (std::filesystem::exists(ruta_real(f->carpeta + relativa), ec)) return f->carpeta + relativa;
    return carpeta_ + relativa;
}

}  // namespace lin

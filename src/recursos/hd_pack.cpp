#include "recursos/hd_pack.h"
#include <charconv>
#include <cstdlib>
#include "plataforma/archivo_mapeado.h"

namespace lin {

namespace {
// "img_123" / "tbt_45" -> tipo + id (variantes como "img_1067h" no son direccionables por el juego).
bool parsear_nombre(std::string_view n, HdPack::Tipo& t, u32& id) {
    std::string_view num;
    if (n.starts_with("img_")) {
        t = HdPack::Tipo::IMG;
        num = n.substr(4);
    } else if (n.starts_with("tbt_")) {
        t = HdPack::Tipo::TBT;
        num = n.substr(4);
    } else {
        return false;
    }
    if (num.empty()) return false;
    auto r = std::from_chars(num.data(), num.data() + num.size(), id);
    return r.ec == std::errc() && r.ptr == num.data() + num.size();
}

std::string_view campo(std::string_view& linea) {
    size_t p = linea.find('\t');
    std::string_view c = linea.substr(0, p);
    linea = p == std::string_view::npos ? std::string_view{} : linea.substr(p + 1);
    return c;
}

long numero(std::string_view s) { return std::strtol(std::string(s).c_str(), nullptr, 10); }
}  // namespace

void HdPack::leer_manifest(std::string_view texto) {
    entradas_.clear();
    indice_ = MapaHash(16384);
    bool primera = true;
    while (!texto.empty()) {
        size_t nl = texto.find('\n');
        std::string_view linea = texto.substr(0, nl);
        texto = nl == std::string_view::npos ? std::string_view{} : texto.substr(nl + 1);
        if (!linea.empty() && linea.back() == '\r') linea.remove_suffix(1);
        if (primera) {  // header
            primera = false;
            continue;
        }
        std::string_view nombre = campo(linea);
        campo(linea);  // sha
        const long w = numero(campo(linea)), h = numero(campo(linea));
        const long ox = numero(campo(linea)), oy = numero(campo(linea));
        std::string_view dup = campo(linea);
        Tipo t;
        u32 id;
        if (!parsear_nombre(nombre, t, id) || w <= 0 || h <= 0 || w > 0xFFFF / HD_SCALE || h > 0xFFFF / HD_SCALE) continue;
        Entrada e;
        e.w = static_cast<u16>(w);
        e.h = static_cast<u16>(h);
        e.ox = static_cast<i16>(ox);
        e.oy = static_cast<i16>(oy);
        if (!dup.empty()) e.archivo = std::string(dup);
        indice_.poner(clave(t, id), static_cast<u32>(entradas_.size()));
        entradas_.push_back(std::move(e));
    }
}

bool HdPack::abrir(const std::string& dir) {
    dir_ = dir;
    if (!dir_.empty() && dir_.back() != '/') dir_ += '/';
    entradas_.clear();
    std::vector<u8> bytes;
    // El manifiesto viene junto a las imágenes o en una carpeta hermana "src_ui".
    if (!leer_archivo(dir_ + "manifest.tsv", bytes) && !leer_archivo(dir_ + "../../src_ui/manifest.tsv", bytes)) return false;
    leer_manifest({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
    return abierto();
}

bool HdPack::info(Tipo t, u32 id, u16& w, u16& h, i16& ox, i16& oy) const {
    const u32* i = indice_.buscar(clave(t, id));
    if (!i) return false;
    const Entrada& e = entradas_[*i];
    w = e.w;
    h = e.h;
    ox = e.ox;
    oy = e.oy;
    return true;
}

void premultiplicar(ImagenRGBA& img) {
    for (u32& p : img.px) {
        const u32 a = p >> 24;
        if (a == 255) continue;
        const u32 r = ((p & 255) * a + 127) / 255, g = (((p >> 8) & 255) * a + 127) / 255, b = (((p >> 16) & 255) * a + 127) / 255;
        p = r | (g << 8) | (b << 16) | (a << 24);
    }
}

bool HdPack::cargar(Tipo t, u32 id, ImagenRGBA& out, u32 max_w, u32 max_h) const {
    const u32* i = indice_.buscar(clave(t, id));
    if (!i) return false;
    const Entrada& e = entradas_[*i];
    if (static_cast<u32>(e.w) * HD_SCALE > max_w || static_cast<u32>(e.h) * HD_SCALE > max_h) return false;
    std::string ruta = dir_;
    ruta += e.archivo.empty() ? std::string(t == Tipo::IMG ? "img_" : "tbt_") + std::to_string(id) : e.archivo;
    ruta += ".png";
    std::vector<u8> bytes;
    if (!leer_archivo(ruta, bytes) || !decodificar_png(bytes, out, false)) return false;
    if (out.ancho != e.w * HD_SCALE || out.alto != e.h * HD_SCALE) return false;
    out.ox = e.ox;
    out.oy = e.oy;
    premultiplicar(out);
    return true;
}

}  // namespace lin

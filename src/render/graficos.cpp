#include "render/graficos.h"
#include "recursos/fuente_hd.h"
#include <algorithm>
#include <cstdio>
#include "render/icon_aura.h"
#include "plataforma/archivo_mapeado.h"

namespace lin {

const RegionAtlas* Graficos::buscar(u64 k) const {
    const u32* i = indice_.buscar(k);
    return i ? &regiones_[*i] : nullptr;
}

template <class F>
const RegionAtlas& Graficos::cargar(u64 k, F&& decodificar) {
    if (const RegionAtlas* r = buscar(k)) return *r;
    u64 t0 = SDL_GetPerformanceCounter();
    Carga c = decodificar();
    if (c == Carga::PENDIENTE) {
        return pendiente_;  // no se recuerda: se vuelve a pedir el frame siguiente
    }
    const RegionAtlas& r = guardar(k, c == Carga::OK);
    ms_decodificacion_ += static_cast<double>(SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency();
    ++decodificados_;
    return r;
}

const RegionAtlas& Graficos::guardar(u64 k, bool ok) {
    RegionAtlas r;
    if (!ok) aura_w_ = 0;
    const u32 escala = escala_tmp_;
    escala_tmp_ = 1;
    if (ok && !gpu_.cabe_en_atlas(tmp_)) {
        // Más grande que una página: no entraría nunca. Se recuerda como fallo, si no se volvería a
        // leer y decodificar en cada frame.
        LIN_LOG("Imagen de %ux%u: no entra en el atlas", tmp_.ancho, tmp_.alto);
        ok = false;
    }
    if (ok && !gpu_.subir(tmp_, r, escala)) {
        // Sin lugar este frame (buffer de subida lleno o atlas sin página libre): no se recuerda, se reintenta.
        aura_w_ = 0;
        return pendiente_;
    }
    if (aura_w_) {  // ícono + aura se subieron como una sola imagen: la región cubre solo el ícono
        r.aura_dx = aura_w_;  // texels
        r.w = static_cast<u16>(aura_w_ / escala);
        r.h = static_cast<u16>(r.h - AURA_PAD);  // lógico: subir dividió los (h + pad * escala) texels por la escala
        aura_w_ = 0;
    }
    if (int p = gpu_.tomar_pagina_reemplazada(); p >= 0) olvidar_pagina(static_cast<u16>(p));
    u32 i;
    if (!libres_.empty()) {
        i = libres_.back();
        libres_.pop_back();
    } else {
        i = static_cast<u32>(regiones_.size());
        regiones_.emplace_back();
        claves_.emplace_back();
    }
    regiones_[i] = ok ? r : RegionAtlas{};
    claves_[i] = k;
    indice_.poner(k, i);
    return regiones_[i];
}

// Libera el casillero i: su clave sale del índice y se vuelve a decodificar si se pide.
void Graficos::olvidar(u32 i) {
    indice_.quitar(claves_[i]);
    regiones_[i] = {};
    libres_.push_back(i);
}

// La página se reemplazó en el atlas: lo que estaba ahí se vuelve a decodificar si se pide.
void Graficos::olvidar_pagina(u16 pagina) {
    for (u32 i = 0; i < regiones_.size(); ++i)
        if (regiones_[i].valida() && regiones_[i].pagina == pagina) olvidar(i);
}

const RegionAtlas& Graficos::img(u32 n) {
    return cargar(clave(T_IMG, n), [&] {
        if (hd_abierto_ && hd_.cargar(HdPack::Tipo::IMG, n, tmp_, TAM_PAGINA - 1, TAM_PAGINA - 1)) {
            escala_tmp_ = HD_SCALE;
            return Carga::OK;
        }
        char nombre[32];
        std::snprintf(nombre, sizeof nombre, "%ue.img", n);
        Bytes b = rec_.sprite(nombre);
        if (b.empty()) {
            std::snprintf(nombre, sizeof nombre, "%u.img", n);
            b = rec_.sprite(nombre);
        }
        return !b.empty() && decodificar_img(b, tmp_) ? Carga::OK : Carga::FALLO;
    });
}

// Imagen n de la interfaz solo desde el Sprite.pak base. El arte en reposo de los botones de la franja inferior (ids impares 1005..1017) existe dos veces: el archivo
// base tiene el ícono marrón monocromo que muestran en reposo las capturas de 2009, un parche SpriteNN.pak lo reemplaza con una versión
// de tile oscuro. El exe dibuja el primer id en reposo (0x47c760), así que esta es esa imagen como la tenía el cliente capturado.
const RegionAtlas& Graficos::img_base(u32 n) {
    return cargar(clave(T_IMG, n | 0x40000000u), [&] {
        char nombre[32];
        std::snprintf(nombre, sizeof nombre, "%ue.img", n);
        Bytes b = rec_.sprite_base(nombre);
        if (b.empty()) {
            std::snprintf(nombre, sizeof nombre, "%u.img", n);
            b = rec_.sprite_base(nombre);
        }
        if (b.empty() || !decodificar_img(b, tmp_)) return Carga::FALLO;
        return Carga::OK;
    });
}

const RegionAtlas& Graficos::img_mascara(u32 n, int px, int py, const u8* alfa, int mw, int mh) {
    const u64 k = clave(T_MAPA, static_cast<u64>(n) | static_cast<u64>(px & 0xFFFF) << 16 | static_cast<u64>(py & 0xFFFF) << 32);
    if (k != mapa_clave_) {  // la vista se movió u otra imagen: se descarta el recorte anterior
        if (const u32* i = indice_.buscar(mapa_clave_)) olvidar(*i);
        mapa_clave_ = k;
    }
    return cargar(k, [&] {
        if (mapa_src_n_ != n) {
            mapa_src_n_ = 0;
            char nombre[32];
            std::snprintf(nombre, sizeof nombre, "%ue.img", n);
            Bytes b = rec_.sprite(nombre);
            if (b.empty()) {
                std::snprintf(nombre, sizeof nombre, "%u.img", n);
                b = rec_.sprite(nombre);
            }
            if (b.empty() || !decodificar_img(b, mapa_src_)) return Carga::FALLO;
            mapa_src_n_ = n;
        }
        const int cw = std::min<int>(mw, mapa_src_.ancho - px), ch = std::min<int>(mh, mapa_src_.alto - py);
        if (px < 0 || py < 0 || cw <= 0 || ch <= 0) return Carga::FALLO;
        tmp_.preparar(static_cast<u16>(cw), static_cast<u16>(ch));
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x) {
                const u32 c = mapa_src_.px[static_cast<size_t>(y + py) * mapa_src_.ancho + x + px];
                const u32 a = (c >> 24) * alfa[static_cast<size_t>(y) * mw + x] / 255;
                tmp_.px[static_cast<size_t>(y) * cw + x] = (c & 0x00FFFFFFu) | a << 24;
            }
        return Carga::OK;
    });
}

const RegionAtlas& Graficos::icono(u32 n) {
    return cargar(clave(T_ICONO, n), [&] {
        // El aura va junto al ícono en la misma subida: se le deja lugar (2 anchos de ícono + relleno).
        if (hd_abierto_ && hd_.cargar(HdPack::Tipo::TBT, n, tmp_, (TAM_PAGINA - 1 - AURA_PAD * HD_SCALE) / 2, TAM_PAGINA - 1 - AURA_PAD * HD_SCALE)) {
            escala_tmp_ = HD_SCALE;
            aura_w_ = tmp_.ancho;
            agregar_aura(tmp_, HD_SCALE);
            return Carga::OK;
        }
        char nombre[32];
        std::snprintf(nombre, sizeof nombre, "%u.tbt", n);
        Bytes b = rec_.sprite(nombre);
        if (b.empty() || !decodificar_tbt(b, tmp_)) return Carga::FALLO;
        aura_w_ = tmp_.ancho;
        agregar_aura(tmp_);
        return Carga::OK;
    });
}

namespace {
constexpr int ANCHO_EMBLEMA = 16, ALTO_EMBLEMA = 12, BYTES_EMBLEMA = ANCHO_EMBLEMA * ALTO_EMBLEMA * 2;
}

// Si su página del atlas se reemplazó, se vuelve a subir desde los datos guardados (el servidor no lo
// manda otra vez: la app lo pide una sola vez por clan).
const RegionAtlas& Graficos::emblema(u32 clan) {
    static const RegionAtlas vacia;
    const u64 k = clave(T_EMBLEMA, clan);
    if (const RegionAtlas* r = buscar(k)) return *r;  // lo común: ya está en el atlas (una sola búsqueda)
    const u32* i = datos_emblema_.buscar(clan);
    if (!i && clan != CLAN_VISTA) return vacia;
    return cargar(k, [&] {
        const u8* d = clan == CLAN_VISTA ? emblema_vista_ : emblemas_.data() + *i;
        tmp_.preparar(ANCHO_EMBLEMA, ALTO_EMBLEMA);
        for (int k = 0; k < ANCHO_EMBLEMA * ALTO_EMBLEMA; ++k) {  // el negro puro es el fondo transparente
            const u16 c = static_cast<u16>(d[2 * k] | (d[2 * k + 1] << 8));
            tmp_.px[k] = (c & 0x7FFF) == 0 ? 0 : rgb555(c);
        }
        return Carga::OK;
    });
}

void Graficos::poner_emblema(u32 clan, Bytes datos, u32 ahora_ms) {
    if (datos.size() < BYTES_EMBLEMA) return;
    constexpr size_t MAX_EMBLEMAS = 512;
    size_t casillero;
    if (const u32* i = datos_emblema_.buscar(clan)) {
        casillero = *i / BYTES_EMBLEMA;
    } else if (emblema_clan_.size() < MAX_EMBLEMAS) {
        casillero = emblema_clan_.size();
        emblemas_.resize(emblemas_.size() + BYTES_EMBLEMA);
        emblema_clan_.push_back(clan);
        emblema_t_.push_back(ahora_ms);
    } else {  // lleno: reemplaza al más viejo (0x424b17)
        casillero = 0;
        for (size_t k = 1; k < emblema_t_.size(); ++k)
            if (emblema_t_[k] < emblema_t_[casillero]) casillero = k;
        // El clan reemplazado ya no está: su región del atlas se libera y el índice lo olvida.
        const u32 viejo = emblema_clan_[casillero];
        datos_emblema_.quitar(viejo);
        if (const u32* r = indice_.buscar(clave(T_EMBLEMA, viejo))) olvidar(*r);
        emblema_clan_[casillero] = clan;
    }
    emblema_t_[casillero] = ahora_ms;
    datos_emblema_.poner(clan, static_cast<u32>(casillero * BYTES_EMBLEMA));
    std::memcpy(emblemas_.data() + casillero * BYTES_EMBLEMA, datos.data(), BYTES_EMBLEMA);
    emblemas_sucios_ = true;
    // Uno nuevo reemplaza al anterior: se sube con el próximo emblema(). El casillero viejo se libera
    // (si no, olvidar_pagina, con la misma clave, podría borrar del índice al nuevo). Su lugar en el
    // atlas queda sin usar hasta que se reemplace la página: el empaquetado por estantes no libera huecos.
    if (const u32* r = indice_.buscar(clave(T_EMBLEMA, clan))) olvidar(*r);
}

void Graficos::poner_emblema_vista(Bytes datos) {
    if (datos.size() < BYTES_EMBLEMA) return;
    std::memcpy(emblema_vista_, datos.data(), BYTES_EMBLEMA);
    if (const u32* r = indice_.buscar(clave(T_EMBLEMA, CLAN_VISTA))) olvidar(*r);
}

void Graficos::emblemas_vaciar() {
    for (u32 clan : emblema_clan_) {
        datos_emblema_.quitar(clan);
        if (const u32* r = indice_.buscar(clave(T_EMBLEMA, clan))) olvidar(*r);
    }
    emblema_clan_.clear();
    emblema_t_.clear();
    emblemas_.clear();
    emblemas_sucios_ = false;
}

void Graficos::emblemas_cargar(const std::string& ruta) {
    emblemas_vaciar();
    std::FILE* f = std::fopen(ruta.c_str(), "rb");
    if (!f) return;
    u32 cab[2] = {};
    std::vector<u32> tabla;
    std::vector<u8> datos;
    bool ok = std::fread(cab, 4, 2, f) == 2 && cab[0] == 0x200 && cab[1] > 0 && cab[1] <= 0x200;
    if (ok) {
        tabla.resize(size_t{cab[1]} * 3);
        datos.resize(size_t{cab[1]} * BYTES_EMBLEMA);
        ok = std::fread(tabla.data(), 4, tabla.size(), f) == tabla.size() &&
             std::fread(datos.data(), 1, datos.size(), f) == datos.size();
    }
    std::fclose(f);
    if (!ok) return;  // corrupto: se empieza de cero, como el original (0x42476a)
    std::vector<bool> usado(cab[1]);
    for (u32 k = 0; k < cab[1]; ++k) {
        const u32 ofs = tabla[k * 3 + 1];
        if (ofs % BYTES_EMBLEMA || ofs / BYTES_EMBLEMA >= cab[1] || usado[ofs / BYTES_EMBLEMA]) return;  // casilleros únicos
        usado[ofs / BYTES_EMBLEMA] = true;
    }
    emblemas_ = std::move(datos);
    emblema_clan_.assign(cab[1], 0);
    emblema_t_.assign(cab[1], 0);
    for (u32 k = 0; k < cab[1]; ++k) {
        const u32 clan = tabla[k * 3], ofs = tabla[k * 3 + 1];
        emblema_clan_[ofs / BYTES_EMBLEMA] = clan;
        emblema_t_[ofs / BYTES_EMBLEMA] = tabla[k * 3 + 2];
        datos_emblema_.poner(clan, ofs);
    }
}

bool Graficos::emblemas_guardar(const std::string& ruta) {
    emblemas_sucios_ = false;
    const u32 n = static_cast<u32>(emblema_clan_.size());
    if (n == 0) return true;
    std::vector<u32> orden(n);
    for (u32 k = 0; k < n; ++k) orden[k] = k;
    std::sort(orden.begin(), orden.end(), [&](u32 a, u32 b) { return emblema_clan_[a] < emblema_clan_[b]; });
    const std::string tmp = ruta + ".tmp";
    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    const u32 cab[2] = {0x200, n};
    bool ok = std::fwrite(cab, 4, 2, f) == 2;
    for (u32 k : orden) {
        const u32 e[3] = {emblema_clan_[k], k * BYTES_EMBLEMA, emblema_t_[k]};
        ok = ok && std::fwrite(e, 4, 3, f) == 3;
    }
    ok = ok && std::fwrite(emblemas_.data(), 1, emblemas_.size(), f) == emblemas_.size();
    ok = std::fclose(f) == 0 && ok;
    if (!ok || std::rename(tmp.c_str(), ruta.c_str()) != 0) {
        std::remove(tmp.c_str());
        emblemas_sucios_ = true;
        return false;
    }
    return true;
}

const RegionAtlas& Graficos::png(u32 n) {
    return cargar(clave(T_PNG, n), [&] {
        char nombre[32];
        std::snprintf(nombre, sizeof nombre, "%u.png", n);
        Bytes b = rec_.sprite(nombre);
        return !b.empty() && decodificar_png(b, tmp_) ? Carga::OK : Carga::FALLO;
    });
}

const RegionAtlas& Graficos::png_archivo(const std::string& ruta) {
    return cargar(clave(T_ARCHIVO, std::hash<std::string>{}(ruta) >> 8), [&] {
        std::vector<u8> b;
        bool ok = leer_archivo(ruta, b) && decodificar_png(b, tmp_, false) && gpu_.cabe_en_atlas(tmp_);
        if (!ok) std::fprintf(stderr, "No se pudo cargar la imagen %s (PNG de hasta %d×%d)\n", ruta.c_str(), TAM_PAGINA - 1, TAM_PAGINA - 1);
        return ok ? Carga::OK : Carga::FALLO;
    });
}

const JuegoTiles* Graficos::juego_tiles(u32 n, bool& pendiente) {
    pendiente = false;
    if (const u32* i = indice_tiles_.buscar(n)) return juegos_[*i].cantidad ? &juegos_[*i] : nullptr;
    char nombre[32];
    std::snprintf(nombre, sizeof nombre, "%u.til", n);
    Bytes b = rec_.tile(nombre);
    if (!b.empty() && !precarga_.listo(clave(T_TILE, n), b)) {
        pendiente = true;
        return nullptr;
    }
    JuegoTiles j;
    if (!j.abrir(b)) j.cantidad = 0;
    indice_tiles_.poner(n, static_cast<u32>(juegos_.size()));
    juegos_.push_back(j);
    return j.cantidad ? &juegos_.back() : nullptr;
}

const RegionAtlas& Graficos::medio_tile(u32 valor, bool derecha) {
    return cargar(clave(T_TILE, (static_cast<u64>(valor) << 1) | (derecha ? 1 : 0)), [&] {
        bool pendiente;
        const JuegoTiles* j = juego_tiles(valor >> 8, pendiente);
        if (pendiente) return Carga::PENDIENTE;
        return j && j->decodificar(valor & 0xFF, derecha, tmp_) ? Carga::OK : Carga::FALLO;
    });
}

bool Graficos::tile_translucido(u32 valor, bool& pendiente) {
    const JuegoTiles* j = juego_tiles(valor >> 8, pendiente);
    return j && j->translucido(valor & 0xFF);
}

ModoMezcla Graficos::tile_modo(u32 valor, bool& pendiente) {
    const JuegoTiles* j = juego_tiles(valor >> 8, pendiente);
    return j ? j->modo(valor & 0xFF) : ModoMezcla::NORMAL;
}

const ArchivoSpr* Graficos::archivo_spr(GfxId gfx, u16 sub, bool& pendiente) {
    pendiente = false;
    u64 k = (static_cast<u64>(gfx) << 16) | sub;
    if (const u32* i = indice_spr_.buscar(k)) return sprs_[*i].frames.empty() ? nullptr : &sprs_[*i];
    char nombre[32];
    std::snprintf(nombre, sizeof nombre, "%u-%u.spr", gfx, sub);
    Bytes b = rec_.sprite(nombre);
    if (!b.empty() && !precarga_.listo(clave(T_SPR, k), b)) {
        pendiente = true;
        return nullptr;
    }
    ArchivoSpr a;
    if (b.empty() || !a.abrir(b)) a.frames.clear();
    indice_spr_.poner(k, static_cast<u32>(sprs_.size()));
    sprs_.push_back(std::move(a));
    return sprs_.back().frames.empty() ? nullptr : &sprs_.back();
}

const RegionAtlas& Graficos::frame_sprite(GfxId gfx, u16 sub, u16 frame) {
    return cargar(clave(T_SPR, (static_cast<u64>(gfx) << 32) | (static_cast<u64>(sub) << 16) | frame), [&] {
        bool pendiente;
        const ArchivoSpr* a = archivo_spr(gfx, sub, pendiente);
        if (pendiente) return Carga::PENDIENTE;
        return a && a->componer_frame(frame, tmp_) ? Carga::OK : Carga::FALLO;
    });
}

const RegionAtlas& Graficos::glifo(u32 g) {
    return cargar(clave(T_GLIFO, g), [&] { return fuente_.rasterizar(g, tmp_) ? Carga::OK : Carga::FALLO; });
}

const RegionAtlas& Graficos::glifo_hd(u32 g) {
    constexpr u32 VARIANTE_HD = 1u << 28;  // separada de los glifos de píxeles en la clave de la caché
    return cargar(clave(T_GLIFO, g | VARIANTE_HD), [&] {
        if (!fuente_.rasterizar_hd(g, tmp_)) return Carga::FALLO;
        escala_tmp_ = HD_ESCALA;
        return Carga::OK;
    });
}

const RegionAtlas& Graficos::glifo_hd_mascara(u32 id, int ancho, int filas, const u8* bits) {
    return cargar(clave(T_GLIFO, (1u << 29) | id), [&] {
        ImagenRGBA m;
        m.preparar(static_cast<u16>(ancho), static_cast<u16>(filas));
        for (int y = 0; y < filas; ++y)
            for (int x = 0; x < ancho; ++x)
                if (bits[y] >> x & 1) m.px[static_cast<size_t>(y) * ancho + x] = 0xFFFFFFFFu;
        if (!campo_de_mascara(m, tmp_)) return Carga::FALLO;
        escala_tmp_ = HD_ESCALA;
        return Carga::OK;
    });
}

// Las capas debajo de la interfaz se dibujan antes del filtro de escalado, así que solo el texto de la interfaz puede pasar por la pasada HD.
bool Graficos::texto_hd_para(u64 clave_orden) const { return (clave_orden >> 56) >= CAPA_UI && gpu_.texto_hd(); }

int Graficos::texto(std::string_view t, float x, float y, u64 clave_orden, u32 color) {
    glifos_de_texto(t.data(), t.size(), glifos_tmp_);
    const bool hd = texto_hd_para(clave_orden);
    int ancho = 0;
    for (const GlifoTexto& g : glifos_tmp_) {
        if (hd) gpu_.dibujar_en(glifo_hd(g.glifo), x + ancho, y, clave_orden, color, Mezcla::TEXTO_HD);
        else gpu_.dibujar_en(glifo(g.glifo), x + ancho, y, clave_orden, color);
        ancho += g.ancho;
    }
    return ancho;
}

int Graficos::texto_html(std::string_view t, float x, float y, u64 clave_orden, u32 color, bool negrita) {
    glifos_de_texto_html(t.data(), t.size(), glifos_tmp_, negrita);
    const bool hd = texto_hd_para(clave_orden);
    int ancho = 0;
    for (const GlifoTexto& g : glifos_tmp_) {
        if (hd) gpu_.dibujar_en(glifo_hd(g.glifo), x + ancho, y, clave_orden, color, Mezcla::TEXTO_HD);
        else gpu_.dibujar_en(glifo(g.glifo), x + ancho, y, clave_orden, color);
        ancho += g.ancho;
    }
    return ancho;
}

int Graficos::texto_contorno(std::string_view t, float x, float y, u64 clave_orden, u32 color, u32 contorno) {
    for (const auto& [dx, dy] : {std::pair{-1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 1.0f}}) texto(t, x + dx, y + dy, clave_orden, contorno);
    return texto(t, x, y, clave_orden + (1u << 24), color);
}

int Graficos::texto_nombre(std::string_view t, float x, float y, u64 clave_orden, u32 color, u32 nivel) {
    constexpr u32 NEGRO = 0xFF000000u;
    if (nivel == 0) return texto_contorno(t, x, y, clave_orden, color, NEGRO);
    for (const auto& [dx, dy] : {std::pair{-2.0f, -2.0f}, {2.0f, -2.0f}, {-2.0f, 2.0f}, {2.0f, 2.0f}, {-2.0f, 0.0f},
                                 {2.0f, 0.0f}, {0.0f, -2.0f}, {0.0f, 2.0f}, {-1.0f, -1.0f}, {1.0f, -1.0f},
                                 {-1.0f, 1.0f}, {1.0f, 1.0f}})
        texto(t, x + dx, y + dy, clave_orden, NEGRO);
    for (const auto& [dx, dy] : {std::pair{-1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, -1.0f}, {0.0f, 1.0f}})
        texto(t, x + dx, y + dy, clave_orden + (1u << 23), nivel);
    return texto(t, x, y, clave_orden + (1u << 24), color);
}

int Graficos::texto_sombra(std::string_view t, float x, float y, u64 clave_orden, u32 color) {
    texto(t, x + 1, y + 1, clave_orden, rgba(0, 0, 0));
    return texto(t, x, y, clave_orden + (1u << 24), color);
}

}  // namespace lin

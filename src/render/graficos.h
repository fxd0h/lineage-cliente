// Caché de gráficos: recurso del juego -> región del atlas. Decodifica la primera vez que se
// pide y recuerda también los fallos (región inválida) para no reintentar cada frame.
//
// Los tiles y los frames de sprites esperan a que su .til o .spr esté en memoria (Precarga): hasta
// entonces devuelven una región vacía, sin recordarla, y se dibujan uno o dos frames después. Las
// imágenes de interfaz, los íconos y los glifos se leen en el momento (son pocos y van en las
// pantallas fijas).
#pragma once
#include <deque>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "core/mapa_hash.h"
#include "recursos/fuente.h"
#include "recursos/hd_pack.h"
#include "recursos/pak.h"
#include "recursos/precarga.h"
#include "recursos/sprites.h"
#include "recursos/tabla_sprites.h"
#include "recursos/tiles.h"
#include "render/gpu.h"

namespace lin {

class Graficos {
public:
    Graficos(Gpu& gpu, const Recursos& rec, const FuenteBitmap& fuente) : gpu_(gpu), rec_(rec), fuente_(fuente) {}

    // Imagen de interfaz N: "Ne.img", "N.img" (con color transparente propio).
    const RegionAtlas& img(u32 n);
    // World-map picture n (original decode, no HD) cropped at (px, py) to the first `mw` x `mh` pixels
    // and multiplied by `alfa` (mw*mh bytes, the "0.alp" mask: soft edges). One cached region: it is
    // rebuilt (reusing the buffers) only when n or the crop position change.
    const RegionAtlas& img_sepia(u32 n);  // img(n) as luminance shifted to brown (bottom-strip buttons at rest)
    const RegionAtlas& img_mascara(u32 n, int px, int py, const u8* alfa, int mw, int mh);
    // Imagen N.png (fondos de pantalla, animaciones de la selección de personaje).
    const RegionAtlas& png(u32 n);
    // PNG fuera del juego (logo propio), con su alfa. Región vacía si no se pudo leer.
    const RegionAtlas& png_archivo(const std::string& ruta);
    // Medio tile de suelo: valor del .s32 (juego << 8 | tile) y mitad.
    const RegionAtlas& medio_tile(u32 valor, bool derecha);
    // Tile translúcido (sombra; ver tiles.h). `pendiente`: su .til todavía no está en memoria.
    bool tile_translucido(u32 valor, bool& pendiente);
    ModoMezcla tile_modo(u32 valor, bool& pendiente);
    // Frame de sprite ya compuesto: archivo "<gfx>-<sub>.spr", frame.
    const RegionAtlas& frame_sprite(GfxId gfx_archivos, u16 sub, u16 frame);
    // La región que se devolvió porque el dato todavía no está (se está cargando, o el atlas no tiene
    // lugar este frame): no es un fallo, se pide de nuevo el frame siguiente. Compara la dirección:
    // hay que pasarle la referencia devuelta, no una copia.
    bool pendiente(const RegionAtlas& r) const { return &r == &pendiente_; }
    const RegionAtlas& glifo(u32 g);
    // Ícono de inventario N.tbt (N = gfx de inventario del ítem).
    const RegionAtlas& icono(u32 n);
    // Emblema de clan (S_Emblem: 16 × 12 píxeles RGB555); región vacía si todavía no llegó.
    const RegionAtlas& emblema(u32 clan);
    void poner_emblema(u32 clan, Bytes datos, u32 ahora_ms);
    // Emblem cache file as the original's Emblem.dat (0x424640 / 0x424870): u32 0x200 (capacity), u32 count,
    // count x {u32 clan, u32 data offset, u32 time} sorted by clan, count x 384 bytes of RGB555. 512 clans at most;
    // a full cache replaces the oldest one. Written only when something changed.
    void emblemas_cargar(const std::string& ruta);
    bool emblemas_guardar(const std::string& ruta);
    // checkbmp preview: a 16 x 12 RGB555 image shown through emblema(CLAN_VISTA); never cached or saved.
    static constexpr u32 CLAN_VISTA = 0xFFFFFFFFu;
    void poner_emblema_vista(Bytes datos);
    bool emblemas_sucios() const { return emblemas_sucios_; }
    void emblemas_vaciar();

    // Texto CP949 con la fuente original; devuelve el ancho en píxeles.
    // HD text: glyph as a field (recursos/fuente_hd.h), cached apart from the pixel glyph. Invalid region if blank.
    const RegionAtlas& glifo_hd(u32 g);
    // True when text drawn with this order key goes through the HD pass (CAPA_EFECTOS and above, HD enabled and available).
    bool texto_hd_para(u64 clave_orden) const;
    // HD field of a glyph given as a 1-bit mask (`filas` rows of `ancho` bits, bit 0 = leftmost, one byte per row);
    // `id` must be unique per glyph. Used by the HUD digit font, which is not in the font files.
    const RegionAtlas& glifo_hd_mascara(u32 id, int ancho, int filas, const u8* bits);
    int texto(std::string_view t, float x, float y, u64 clave, u32 color = BLANCO);
    // Texto de las páginas HTML de la interfaz original: cara proporcional, sin sombra ni contorno.
    int texto_html(std::string_view t, float x, float y, u64 clave, u32 color = BLANCO, bool negrita = false);
    // Texto con sombra negra de 1 px (como los nombres sobre los personajes).
    int texto_sombra(std::string_view t, float x, float y, u64 clave, u32 color = BLANCO);
    // Las mismas con coordenadas enteras o mezcladas (la interfaz trabaja en píxeles enteros): se pasan
    // a float acá y no con un static_cast en cada llamada. Plantillas y no sobrecargas con int, para que
    // una llamada con un float y un int no sea ambigua.
    template <class X, class Y>
        requires std::is_arithmetic_v<X> && std::is_arithmetic_v<Y>
    int texto(std::string_view t, X x, Y y, u64 clave, u32 color = BLANCO) {
        return texto(t, static_cast<float>(x), static_cast<float>(y), clave, color);
    }
    template <class X, class Y>
        requires std::is_arithmetic_v<X> && std::is_arithmetic_v<Y>
    int texto_sombra(std::string_view t, X x, Y y, u64 clave, u32 color = BLANCO) {
        return texto_sombra(t, static_cast<float>(x), static_cast<float>(y), clave, color);
    }

    // eng.fnt text with the 1 px 4-way outline of 0x4334f0 (colour `color`, outline `contorno`, default black 0x5004f0).
    int texto_contorno(std::string_view t, float x, float y, u64 clave, u32 color = BLANCO, u32 contorno = 0xFF000000u);

    // World name label of the original client: 1 px black outline; with `nivel` != 0 an 8x14-ish black halo first and
    // then `nivel` as a 1 px coloured outline (monster level tier, 0x433a90 + 0x4334f0), the fill keeps `color`.
    int texto_nombre(std::string_view t, float x, float y, u64 clave, u32 color, u32 nivel = 0);

    // Enables the HD UI pack at `dir` (img()/icono() prefer its images). false (and HD stays off) if unreadable.
    bool abrir_hd(const std::string& dir) { return hd_abierto_ = hd_.abrir(dir); }
    bool hd_activo() const { return hd_abierto_; }

    Gpu& gpu() { return gpu_; }
    // Al comienzo de cada frame: lo que terminó de traer la precarga, y olvidar la página del atlas que
    // se vació al terminar el frame anterior (EmpaquetadoAtlas::fin_de_frame).
    void nuevo_frame() {
        precarga_.actualizar();
        if (int p = gpu_.tomar_pagina_reemplazada(); p >= 0) olvidar_pagina(static_cast<u16>(p));
    }
    u32 precargas_en_curso() const { return precarga_.en_curso(); }
    // Medición: tiempo de decodificación y gráficos nuevos desde la última llamada.
    double tomar_ms_decodificacion() { double v = ms_decodificacion_; ms_decodificacion_ = 0; return v; }
    u32 tomar_decodificados() { u32 v = decodificados_; decodificados_ = 0; return v; }

private:
    enum Tipo : u64 { T_IMG = 1, T_PNG, T_TILE, T_SPR, T_GLIFO, T_ICONO, T_EMBLEMA, T_ARCHIVO, T_MAPA };
    static u64 clave(Tipo t, u64 v) { return (static_cast<u64>(t) << 56) | v; }
    enum class Carga { OK, FALLO, PENDIENTE };
    // Busca k; si no está, decodifica en tmp_ con `decodificar` (devuelve Carga) y lo sube al atlas.
    template <class F>
    const RegionAtlas& cargar(u64 k, F&& decodificar);
    const RegionAtlas& guardar(u64 k, bool ok);
    void olvidar(u32 casillero);
    void olvidar_pagina(u16 pagina);
    const RegionAtlas* buscar(u64 k) const;
    // nullptr si no existe o si todavía no está en memoria (`pendiente`).
    const JuegoTiles* juego_tiles(u32 n, bool& pendiente);
    const ArchivoSpr* archivo_spr(GfxId gfx, u16 sub, bool& pendiente);

    Gpu& gpu_;
    const Recursos& rec_;
    const FuenteBitmap& fuente_;
    MapaHash indice_{4096};
    // deque: crecer no mueve las regiones, así siguen valiendo las referencias que ya se devolvieron
    // (un texto dibujado después de pedir un ícono puede agregar glifos en el mismo frame).
    std::deque<RegionAtlas> regiones_;
    RegionAtlas pendiente_;  // región vacía que se devuelve mientras algo está pendiente (no se guarda)
    std::vector<u64> claves_;   // clave de cada región (para olvidarlas)
    std::vector<u32> libres_;   // regiones olvidadas, para reusar
    MapaHash indice_tiles_, indice_spr_;
    MapaHash datos_emblema_{64};  // clan -> desplazamiento en emblemas_
    std::vector<u8> emblemas_;    // los bytes RGB555 de S_Emblem de cada clan
    std::vector<u32> emblema_clan_, emblema_t_;  // por casillero (desplazamiento / 384): clan y hora de entrada
    bool emblemas_sucios_ = false;
    u8 emblema_vista_[384] = {};
    std::vector<JuegoTiles> juegos_;
    std::vector<ArchivoSpr> sprs_;
    Precarga precarga_;
    ImagenRGBA tmp_;
    ImagenRGBA mapa_src_;  // img_mascara: the decoded picture, kept while the same one is asked
    u32 mapa_src_n_ = 0;
    u64 mapa_clave_ = 0;   // key of the cached masked crop (0 = none)
    HdPack hd_;
    bool hd_abierto_ = false;
    u32 escala_tmp_ = 1;  // texels per logical pixel of what tmp_ holds (HD_SCALE for HD pack images)
    u16 aura_w_ = 0;  // icon width while tmp_ holds icon + aura (see RegionAtlas::aura_dx)
    std::vector<GlifoTexto> glifos_tmp_;
    double ms_decodificacion_ = 0;

    u32 decodificados_ = 0;
};

}  // namespace lin

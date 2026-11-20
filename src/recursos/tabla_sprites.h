// Tabla de acciones de sprites (equivalente a list.spr del cliente original).
//
// Esta instalación solo trae list.enx (cifrado, formato desconocido), así que se usa
// datos/sprite_table.txt de l1j-client (GPLv3). Registros separados por \0, uno por gfx:
//   "<subsprites>[=<gfx de archivos>]" y luego entradas "<n> ...":
//   - 100–114: atributos (101 sombra, 102 tipo, 104 atributo, ...). "110 N" fija la velocidad de
//     las acciones que siguen en el registro: un tick dura 40 × 24 / N ms (N = 24 por defecto,
//     40 ms), la misma fórmula que spr_actions del servidor (frame_count × 40 × 24 / frame_rate).
//   - otro n: acción n: "<n> <dir> <frames>" y frames × "<sub> <frame> <duración>[sufijo]"
//     con sufijos ! (impacto), [n / <n (sonido), ] ^ > { n (otros).
// Un frame se dibuja con el archivo "<gfx de archivos>-<sub + heading>.spr", frame <frame>.
//
// Diseño orientado a datos: todos los frames de todas las acciones van en un solo arreglo y
// una tabla hash (gfx, acción) -> rango.
#pragma once
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "core/mapa_hash.h"
#include "recursos/modo_mezcla.h"

namespace lin {

struct FrameAccion {
    u16 sub;       // sub-sprite base (se suma el heading si la acción tiene dirección)
    u16 frame;     // frame dentro del .spr
    u8 duracion;   // en ticks
    bool impacto;  // sufijo "!": el golpe alcanza al objetivo en este frame
    i16 sonido;    // -1 si no hay
    u16 efecto;    // sufijo "]N": al mostrarse este frame nace el gráfico N (polvo, chispas...); 0 = ninguno
};

struct RangoAccion {
    u32 primer;
    u16 cantidad;
    u8 direccional;
    u8 velocidad;  // "110 N" vigente (24 por defecto): un tick dura 960 / velocidad ms
};

// Tipos de gfx (atributo 102 de la tabla; verificado con los gfx de suelo de ítems de etc_items y
// weapons, los monstruos de npcs y los personajes). Los NPC del pueblo son 6 o 12.
enum TipoGfx : u8 { TIPO_PERSONAJE = 5, TIPO_ITEM = 9, TIPO_MONSTRUO = 10 };

// Acciones conocidas (ActionCodes de L1J). Caminar, atacar, recibir daño y quieto tienen una
// variante por arma: acción + código de arma (4 espada, 11 hacha, 20 arco...); morir no.
enum Accion : u8 { ACC_CAMINAR = 0, ACC_ATACAR = 1, ACC_DANADO = 2, ACC_QUIETO = 3, ACC_MORIR = 8 };

// Moves a death row's leading entry (the sprite file's last frame) to the end; see tabla_sprites.cpp.
void poner_cadaver_al_final(std::vector<FrameAccion>& frames, const RangoAccion& r);

// Result of merging list.spz into the table.
struct ResumenLista {
    u32 nuevos = 0;        // gfx with actions that had no row in sprite_table.txt
    u32 reemplazados = 0;  // gfx already in sprite_table.txt: kept, the list.spz rows are ignored
    u32 omitidos = 0;      // entries already without actions, or without any file, left alone
    u32 atributos = 0;     // new gfx without actions (ground items, objects): only type, shadow, alias...
    u32 sin_archivo = 0;   // new gfx whose sprite file (alias) is missing from our paks: redirected or left out
    u32 recortados = 0;    // new gfx with frames (or whole actions) dropped because our paks lack those images
};

// Number of images in sprite file "<gfx>-<sub>.spr" of the loaded paks; 0 when the file is absent.
using ContadorFrames = std::function<u32(GfxId gfx, u16 sub)>;

class TablaSprites {
public:
    bool cargar(const std::vector<u8>& texto);
    bool cargar_archivo(const std::string& ruta);
    // Merges a plaintext list.spz (see lista_sprites.cpp): its gfx replace or add to the loaded ones.
    // Call after cargar(). Returns false when the text has no usable entry.
    // With `contador`, rows are checked against our sprite paks (list.spz comes from a newer client): frames
    // pointing at images we lack are dropped, an alias without files falls back to the gfx's own file, and
    // entries with no drawable file are left out (the gfx keeps the no-row behaviour).
    bool fusionar_lista(std::string_view texto, ResumenLista& resumen, const ContadorFrames* contador = nullptr);
    bool fusionar_lista_archivo(const std::string& ruta, ResumenLista& resumen,
                                const ContadorFrames* contador = nullptr);
    // gfx with at least one action row in sprite_table.txt (before any merge).
    u32 gfx_de_tabla() const { return gfx_de_tabla_; }

    const RangoAccion* accion(GfxId gfx, u8 accion) const;
    const FrameAccion& frame(u32 i) const { return frames_[i]; }
    // Sub-sprite for a gfx without table rows: the files are laid out 8 headings per group, group 0
    // first; a heading outside 0-7 falls back to sub 0.
    static u16 sub_sin_fila(u8 dir) { return dir < 8 ? dir : 0; }
    GfxId gfx_archivos(GfxId gfx) const { return gfx < alias_.size() ? alias_[gfx] : gfx; }
    i16 sombra(GfxId gfx) const { return gfx < sombra_.size() ? sombra_[gfx] : -1; }
    // Tipo de gfx (atributo 102): TIPO_ITEM para los ítems del suelo, TIPO_MONSTRUO... 0 si no tiene.
    u8 tipo(GfxId gfx) const { return gfx < tipo_.size() ? tipo_[gfx] : 0; }
    // Attribute 104 -> blend mode (screen for the spell effects and flames, multiply for the torch discs...).
    ModoMezcla modo(GfxId gfx) const { return gfx < atributos_.size() ? modo_de_atributo(atributos_[gfx]) : ModoMezcla::NORMAL; }
    // Attribute 111 (stride): the walk is two half-strides, action 0 and action 4 (each starts on the
    // other's pose), played on alternate steps. Valakas, Antharas, the race bugbears, the dogs...
    bool zancada(GfxId gfx) const { return gfx < zancada_.size() && zancada_[gfx]; }
    // Walk action for a step of a stride sprite (S3EP1U 0x4c6ff0: a per-object step counter picks from
    // {0, 4}, whatever the weapon status): 0 on even steps, 4 on odd ones if the sprite has it.
    u8 accion_caminar(GfxId gfx, u8 paso) const { return paso & 1 && accion(gfx, 4) ? 4 : 0; }
    // Attribute 105 (clothes): gfx drawn over the body, after it and in list order, with the body's
    // sub-sprite and frame and their own attribute 104 (S3EP1U 0x4cbc50 / 0x4cbdb0). Valakas -> 2003.
    std::span<const GfxId> ropa(GfxId gfx) const {
        return gfx < ropa_.size() ? std::span<const GfxId>(ropa_[gfx]) : std::span<const GfxId>{};
    }
    // Atributo 109: gráfico que sigue a este al terminar (la explosión de Energy Bolt al llegar); 0 si no hay.
    GfxId siguiente(GfxId gfx) const { return gfx < siguiente_.size() ? siguiente_[gfx] : 0; }
    // Duración total de una acción en ms (suma de los frames × ms por tick); 0 si no existe. Para
    // caminar coincide con la velocidad de movimiento que usa el servidor (spr_actions de L1J).
    u32 duracion_ms(GfxId gfx, u8 accion) const;
    u32 ticks(const RangoAccion& r) const;  // suma de las duraciones de los frames (como frame_count del servidor)
    // Frame de la acción `transcurrido_ms` después de empezar (`termino`: ya pasó un ciclo). En
    // bucle, o detenida en el último frame si !bucle (morir). Con `ciclo_ms` > 0 la acción se
    // estira o acorta para durar eso (caminar = un paso).
    const FrameAccion& frame_en(const RangoAccion& r, u64 transcurrido_ms, bool& termino, u64 ciclo_ms = 0,
                                bool bucle = true) const;
    // ms desde el comienzo de la acción hasta su primer frame de impacto ("!"); si no tiene, la
    // mitad de la acción.
    u32 ms_hasta_impacto(GfxId gfx, u8 accion) const;
    size_t cantidad_gfx() const { return alias_.size(); }

private:
    void copiar_filas_faltantes();
    u32 gfx_de_tabla_ = 0;
    std::vector<FrameAccion> frames_;
    std::vector<bool> zancada_;
    std::vector<std::vector<GfxId>> ropa_;
    std::vector<RangoAccion> rangos_;
    MapaHash indice_;              // (gfx << 8 | accion) -> índice en rangos_
    std::vector<GfxId> alias_;     // por gfx: gfx cuyos archivos .spr se usan
    std::vector<i16> sombra_;
    std::vector<u8> tipo_;
    std::vector<u16> atributos_;
    std::vector<GfxId> siguiente_;
};

}  // namespace lin

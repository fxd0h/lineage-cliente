// Inventario del jugador (módulo "Inventory" del plan): tabla columnar por id de ítem.
//
// S_InvList lo llena al entrar; S_AddItem, S_DeleteInventoryItem, S_ItemName, S_ItemAmount y
// S_ItemColor lo actualizan. El nombre llega armado por el servidor ("$N", cantidad, encantamiento
// y si está equipado: " ($9)" arma, " ($117)" armadura).
#pragma once
#include <string_view>
#include <vector>
#include "core/tabla_soa.h"
#include "proto/eventos.h"
#include "recursos/textos.h"

namespace lin {

struct TextoRef {
    u32 ofs = 0;
    u16 len = 0;
};

// Las arenas de textos solo crecen (cada nombre nuevo o cambiado se agrega al final): pasado este
// tamaño se compactan, copiando solo los que siguen en uso.
constexpr size_t MAX_ARENA_TEXTOS = 256 * 1024;

// `cada(f)` llama a f(TextoRef&) con cada referencia viva; quedan apuntando a la arena nueva.
template <class F>
void compactar_textos(std::vector<char>& textos, F&& cada) {
    std::vector<char> nueva;
    nueva.reserve(textos.size() / 4);
    cada([&](TextoRef& r) {
        const u32 ofs = static_cast<u32>(nueva.size());
        nueva.insert(nueva.end(), textos.begin() + r.ofs, textos.begin() + r.ofs + r.len);
        r.ofs = ofs;
    });
    textos.swap(nueva);
}

enum ColItem : size_t {
    I_GFX,          // ícono de inventario (N.tbt)
    I_USO,          // tipo de uso (L1J ItemTable: 1 arma, 2 armadura, 59 curación...)
    I_BENDICION,    // valor del protocolo de S_AddItem: 0 bendito, 1 normal, 2 maldito, 3 sin identificar, +128 sellado
    I_CANTIDAD,
    I_IDENTIFICADO,
    I_NOMBRE,
    I_ATRIBUTOS,    // bytes de getStatusBytes (en `textos`); vacío si no está identificado
};
using TablaItems = TablaSoa<u16, u8, u8, u32, u8, TextoRef, TextoRef>;

constexpr u8 USO_ARMA = 1, USO_ARMADURA = 2;

// Byte de bendición de S_AddItem / S_InvList / S_ItemColor / S_Trade*: L1ItemInstance.getStatusForPacket (el cliente
// original lo lee igual, FUN_004458f0 en 0x4458f0 lo guarda en item+0x6c). Se suma 128 si está sellado.
enum : u8 { BLESS_BLESSED = 0, BLESS_NORMAL = 1, BLESS_CURSED = 2, BLESS_UNIDENTIFIED = 3, BLESS_SEALED_FLAG = 128 };
constexpr u8 bless_kind(u8 wire) { return static_cast<u8>(wire & ~BLESS_SEALED_FLAG); }

struct Inventario {
    TablaItems items;
    std::vector<char> textos;  // nombres y atributos; se vacía con cada S_InvList
    bool recien_cargado = false;  // llegó S_InvList: la app le aplica el orden guardado y lo baja
    void limpiar() {
        items.limpiar();
        textos.clear();
    }
};

// Entrada de itemquest.tbl: gfx del objeto y la línea de desc-e.tbl con su nombre (pestaña "Quest" de la interfaz original).
struct ItemMision { u16 gfx; u32 desc; };
std::string_view nombre_item(const Inventario& inv, Slot s);
std::string_view atributos_item(const Inventario& inv, Slot s);
// Los atributos de un ítem identificado en texto, una línea por atributo ("Damage 6/8", "AC 3",
// "STR +1", "Usable by: Knight, Elf"...), separadas por '\n'. Devuelve el largo escrito.
size_t describir_atributos(std::string_view bytes, char* salida, size_t capacidad);
// Texto de la caja de hover del inventario original (0x446490 / 0x446520 / dibujo 0x44e45b): el nombre, ajustado a 23 columnas
// (una segunda línea solo si es más largo), después el texto de atributos ajustado a 22 columnas ('\n' fuerza un salto). Las primeras
// `nombre` líneas son las del nombre.
struct LineasTip {
    static constexpr size_t MAX_LINEAS = 22, ANCHO = 32;
    char texto[MAX_LINEAS][ANCHO]{};
    u8 n = 0, nombre = 0;
};
void armar_tooltip_item(std::string_view nombre, std::string_view detalles, LineasTip& salida);
// Equipado según el sufijo que agrega el servidor al nombre.
bool item_equipado(std::string_view nombre);
// Orden de la grilla del inventario (cliente original): los objetos puestos primero, el resto conserva el orden
// del servidor (estable). Escribe en `salida` las casillas de las primeras `cap` posiciones y devuelve cuántas
// escribió; las posiciones después de `cap` conservan su casilla natural.
size_t orden_inventario(const Inventario& inv, Slot* salida, size_t cap);
// Mueve `mover` (en el orden en que están en la tabla) junto a `destino`: delante si `destino` está
// antes que el primero de ellos, detrás si no. Así se ordena el inventario a mano.
void mover_items(Inventario& inv, std::span<const ObjId> mover, ObjId destino);
// Pone primero los ítems de `ids`, en ese orden (los que ya no están se ignoran); el resto queda
// detrás, en el orden que tenía. Así vuelve el orden guardado.
void ordenar_por_ids(Inventario& inv, std::span<const ObjId> ids);
// Aplica los eventos de ítems (los demás se ignoran).
void aplicar_inventario(Inventario& inv, std::span<const Evento> eventos);

void cargar_item_quest(const TablaTexto& tabla, std::vector<ItemMision>& salida);
bool item_de_mision(const std::vector<ItemMision>& quest, const TablaTexto& desc, u16 gfx, std::string_view name);

}  // namespace lin

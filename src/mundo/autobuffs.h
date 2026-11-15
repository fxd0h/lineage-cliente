// Auto-buffs (PLAN_JUGABILIDAD.md, punto 4): qué buff relanzar y cuándo. Sin estado de la app,
// para los tests; la interfaz y el lanzado están en app/autobuffs_ui.cpp.
#pragma once
#include "mundo/inventario.h"
#include "mundo/mundo.h"
#include "recursos/hechizos.h"

namespace lin {

constexpr int CASILLEROS_AUTO = 6;
constexpr u64 MARGEN_AUTO_MS = 2000;       // se renueva cuando al efecto le queda menos que esto
constexpr u64 ESPERA_FALLO_MS = 10000;     // tras "MP insuficiente" o "el hechizo falló"
constexpr u64 ESPERA_RESPUESTA_MS = 3000;  // tras lanzarlo, lo que puede tardar en verse el efecto

// Pociones que también se mantienen solas. En un casillero van con un id propio (POCION_AUTO + su
// lugar en POCIONES_AUTO), fuera del rango de los hechizos; hechizo_de_efecto devuelve ese id para
// su ícono. Se las reconoce como a las de curación (autopocion.h): por el $N del nombre o, si no lo
// trae, por el ícono. Datos de etcitem y C_ItemUSe de L1J: la azul (maná) dura 600 s y manda
// S_SkillIconGFX 34; la de sabiduría dura 300 s y manda S_SkillIconWisdomPotion (0x39).
constexpr u32 POCION_AUTO = 1000;
constexpr u8 ICONO_POCION_AZUL = 34, ICONO_SABIDURIA = 0x39;  // tipo de EA_ICONO
struct PocionAuto {
    u32 n;      // $N del nombre
    u16 gfx;    // ícono de inventario
    u8 icono;   // tipo de EA_ICONO de su efecto
    u32 duracion_s;
    std::string_view nombre;
};
constexpr PocionAuto POCIONES_AUTO[] = {
    {232, 75, ICONO_POCION_AZUL, 600, "Blue Potion"},
    {944, 1529, ICONO_SABIDURIA, 300, "Wisdom Potion"},
};
// La poción de ese id de casillero, o nullptr si es un hechizo (o nada).
const PocionAuto* pocion_auto(u32 id);
// El id de casillero del ítem de ese lugar del inventario; 0 si no es una de esas pociones.
u32 pocion_auto_de_item(const Inventario& inv, Slot s);
// El primer ítem del inventario que es esa poción; 0 si no queda.
ObjId buscar_pocion_auto(const Inventario& inv, u32 id);

struct CasilleroAuto {
    u32 hechizo = 0;  // 0 = vacío; desde POCION_AUTO, una poción
    i32 mp = 0;
    u64 duracion_ms = 0;
    u64 lanzado_ms = 0;     // cuándo salió su último C_UseSkill (0 = nunca)
    u64 reintentar_ms = 0;  // tras un fallo, no antes de esto
    bool efecto_visto = false;  // alguna vez se vio su efecto: si no está, terminó (aunque antes de tiempo)
};

// Hechizo que da un efecto activo, o 0 si viene de un ítem.
u32 hechizo_de_efecto(const EfectoActivo& ef);
// Lo que le queda (ms) al efecto que da el hechizo; 0 si no está activo.
u64 resta_efecto_de_hechizo(const EstadoJugador& j, u32 hechizo, u64 ahora);
// Buff que se puede mantener solo: no es de ataque, dura, y no pide elegir un ítem.
bool acepta_autobuff(const Hechizo& h);
// El primer casillero (de izquierda a derecha) que hay que lanzar ahora, o -1. Lanzar: el efecto no
// está activo o le queda menos de MARGEN_AUTO_MS; si su efecto nunca se vio (hay buffs sin ícono ni
// gráfico), tampoco se lanzó hace menos de su duración. Además alcanza el MP, pasó un posible fallo y la
// respuesta del lanzado anterior, y el personaje no está muerto ni inmóvil.
int elegir_autobuff(const EstadoJugador& j, const CasilleroAuto* c, int n, u64 ahora);

}  // namespace lin

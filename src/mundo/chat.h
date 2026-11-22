// Chat: registro de mensajes, globos sobre las cabezas y lo que escribe el jugador.
//
// - El registro es un anillo de líneas de tamaño fijo: agregar un mensaje no reserva memoria.
// - Los mensajes del servidor (S_ServerMessage) llegan como id + argumentos; el texto sale de
//   string-e.tbl (%0, %1 se reemplazan por los argumentos) y los "$N" de desc-e.tbl.
// - Los códigos de color "\fX" del texto se quitan; el primero decide el color de la línea.
// - Prefijos del original al escribir: ! grito, & global, $ comercio, @ clan, # grupo,
//   % alianza, * grupo de chat y "nombre texto para susurrar.
#pragma once
#include <span>
#include <string>
#include <string_view>
#include "proto/eventos.h"
#include "recursos/textos.h"

namespace lin {

constexpr size_t LARGO_LINEA_CHAT = 190;
constexpr u32 CAPACIDAD_CHAT = 256;
constexpr u32 CANTIDAD_GLOBOS = 16;
constexpr u64 MS_GLOBO = 5000;     // tiempo que el texto queda sobre la cabeza de quien habla
constexpr u8 CHAT_MENSAJE = 200;   // tipo de línea para los mensajes del servidor (no es de cable)

struct LineaChat {
    u32 color;
    u8 tipo;  // TipoChat o CHAT_MENSAJE
    u8 largo;
    i8 dir = -1;  // gritos de la interfaz original: ícono de dirección 0..7 dibujado antes del texto (código "\\d<n>" de S3EP1U, imagen 605 + n), -1 ninguno
    char texto[LARGO_LINEA_CHAT];
};

struct GloboChat {
    ObjId id;
    u64 inicio;
    u32 linea;  // número absoluto de línea en el registro
    // Regla de superposición de la interfaz original (S3EP1U 0x41a8e3): cuando un globo nuevo aparece, todo globo anterior cuyo rectángulo
    // se le superpone pasa a gris (RGB555 0x4a52) por el resto de su vida. `rect` = último rectángulo dibujado en pantalla.
    i32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool con_rect = false, revisado = false, gris = false;
};

struct Chat {
    LineaChat lineas[CAPACIDAD_CHAT];
    bool original_ui = false;     // interfaz original: colores y reglas de globos del cliente 2009 (docs/fork/RE_HUD.md)
    bool globo_comercio = false;  // opción showMargetMsg: el chat de comercio también se muestra sobre la cabeza de quien habla
    u32 total = 0;  // líneas agregadas desde el inicio (el anillo guarda las últimas CAPACIDAD_CHAT)
    GloboChat globos[CANTIDAD_GLOBOS] = {};

    u32 cantidad() const { return total < CAPACIDAD_CHAT ? total : CAPACIDAD_CHAT; }
    // i = 0 es la más reciente; i < cantidad().
    const LineaChat& reciente(u32 i) const { return lineas[(total - 1 - i) % CAPACIDAD_CHAT]; }
    std::string_view texto(const LineaChat& l) const { return {l.texto, l.largo}; }
    void agregar(std::string_view t, u32 color, u8 tipo, i8 dir = -1);
    // Globo vigente del objeto (vacío si no habló en los últimos MS_GLOBO).
    std::string_view globo(ObjId id, u64 ahora, u64 duracion = MS_GLOBO) const;
    GloboChat* globo_activo(ObjId id, u64 ahora, u64 duracion = MS_GLOBO);
    void limpiar() { total = 0; for (GloboChat& g : globos) g = {}; }
};

// `px`, `py`: celda propia (-1 si no se conoce), para el ícono de dirección de los gritos (interfaz original).
void aplicar_chat(Chat& c, const TablaTexto& mensajes, const TablaTexto& desc, std::span<const Evento> eventos, u64 ahora,
                  int px = -1, int py = -1);
// Dígito de dirección del grito de S3EP1U 0x483500..0x483697: sector 0..7 del desplazamiento (dx, dy) de quien habla respecto de quien escucha,
// -1 cuando ambos son cero.
int direccion_grito(int dx, int dy);

// Color de cada tipo de chat (0xAABBGGRR).
u32 color_chat(u8 tipo);
// Color del cliente original de una línea de chat (S3EP1U 0x484800 / S_Say 0x483500). `normal_path`: paquete S_Say (tipos 0 y
// 2) en vez del paquete global tipado; `texto` es la línea tal como se muestra.
u32 color_chat_original(u8 tipo, bool normal_path, std::string_view texto);

// Reemplaza los "$N" del texto por la entrada N de desc-e.tbl. Devuelve los bytes escritos.
size_t expandir_desc(const TablaTexto& desc, std::string_view t, char* destino, size_t capacidad);
// Color del código \fX (de '0' a 'd', paleta del original); `normal` si no es un código.
inline bool es_codigo_color(char c) { return c >= 0x30 && c < 0x65; }
u32 color_codigo(char c, u32 normal);

// Color del nombre de un jugador según su alineamiento (0x4cc140 del original): blanco de 0 a 499, hacia azul
// desde 500 y hacia rojo bajo 0. También colorea el número de alineamiento de la ventana de estado.
u32 color_alineamiento(i32 legal);

// Número con punto cada tres cifras (10.000), en buf.
const char* con_miles(u64 v, char* buf, size_t n);

// Recorre un texto con códigos llamando a f(tramo, color) por cada tramo visible, como el original
// (sub_432590): \fX cambia el color; \bX, \iN, \k, \j, \c y \\ no se muestran. `doble_byte`: fuera de
// UTF-8, un carácter de dos bytes puede tener una barra como segundo byte.
template <class F>
void recorrer_codigos(std::string_view t, u32 normal, bool doble_byte, F&& f) {
    u32 color = normal;
    size_t ini = 0;
    auto cortar = [&](size_t fin) {
        if (fin > ini) f(t.substr(ini, fin - ini), color);
    };
    for (size_t i = 0; i < t.size();) {
        if (doble_byte && static_cast<unsigned char>(t[i]) >= 0x80) {
            i += 2;
            continue;
        }
        const char c = t[i] == '\\' && i + 1 < t.size() ? t[i + 1] : 0;
        const char x = i + 2 < t.size() ? t[i + 2] : 0;
        size_t largo = 0;
        if ((c == 'f' || c == 'b') && es_codigo_color(x)) largo = 3;
        else if (c == 'i' && x >= '0' && x <= '5') largo = 3;
        else if (c == 'k' || c == 'j' || c == 'c' || c == '\\') largo = 2;
        if (!largo) {
            ++i;
            continue;
        }
        cortar(i);
        if (c == 'f') color = color_codigo(x, normal);
        i += largo;
        ini = i;
    }
    cortar(t.size());
}

// Arma el texto de un S_ServerMessage. `color` sale del primer código \fX, si lo hay.
size_t formatear_mensaje(const TablaTexto& mensajes, const TablaTexto& desc, const Evento& e, char* destino,
                         size_t capacidad, u32& color, bool original = false);

// Lo que escribió el jugador, ya separado según el prefijo.
struct EntradaChat {
    u8 tipo = CHAT_NORMAL;
    bool susurro = false;
    std::string_view destino;  // solo susurro
    std::string_view texto;
};
// false si no hay nada que enviar (línea vacía, prefijo sin texto, susurro sin destinatario).
bool interpretar_entrada(std::string_view linea, EntradaChat& salida);

}  // namespace lin

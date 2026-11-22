// Ficha de un cartel (clase de ventana del cliente original 0x4a6c90, abierta desde 0x408300): un clic en un objeto del mundo de tipo 4 (un
// cartel) a 2 celdas en x y 1 en y cuyo segundo texto del pack (título) es un id desc-e `$` muestra el marco 1197, el fondo translúcido 1198 en
// (8,11), el ícono `0x83 + rumbo` de la tabla de íconos en (25,45), el texto desc-e ajustado debajo y el botón de cerrar
// 969 en (182,19). l1j-en manda los carteles como S_SignboardPack (un S_CharPack con el nombre `$`) y no hace nada ante un clic,
// así que la ficha es solo del cliente.
#include <algorithm>
#include <charconv>
#include "app/app.h"
#include "recursos/fuente.h"

namespace lin {

namespace {

constexpr u32 IMG_FRAME = 1197, IMG_BACK = 1198, IMG_CLOSE = 969;
constexpr int CARD_W = 207, CARD_H = 289, ICON_BASE = 0x83;
constexpr int TEXT_X = 29, TEXT_Y = 78, TEXT_W = 150;

const u64 KEY_BACK = clave_orden(CAPA_UI, 10), KEY_FRAME = clave_orden(CAPA_UI, 11), KEY_CTRL = clave_orden(CAPA_UI, 16),
          KEY_TEXT = clave_orden(CAPA_UI_TEXTO, 16);

}  // namespace

Rect rect_cartel(const App& a) { return {(a.gpu.ancho() - CARD_W) / 2, a.gpu.alto() / 6, CARD_W, CARD_H}; }

void abrir_cartel(App& a, Slot s) {
    // l1j-en manda el id `$` en el segundo texto del pack (el título); el primero va vacío.
    const std::string_view nombre = texto(a.mundo, a.mundo.objetos.col<O_TITULO>()[s]);
    u32 id = 0;
    if (nombre.size() < 2 || nombre[0] != '$') return;
    const auto r = std::from_chars(nombre.data() + 1, nombre.data() + nombre.size(), id);
    if (r.ec != std::errc{} || r.ptr != nombre.data() + nombre.size()) return;
    a.cartel = {true, a.mundo.objetos.col<O_DIR>()[s], id};
}

void dibujar_cartel(App& a) {
    if (!a.cartel.abierto) return;
    arrastrar_ventana(a, IdVentana::CARTEL);
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const Rect r = rect_ventana(a, IdVentana::CARTEL);
    g.gpu().dibujar(g.img(IMG_BACK), r.x + 8, r.y + 11, KEY_BACK, rgba(255, 255, 255, opacidad_ventanas(a)));
    g.gpu().dibujar(g.img(IMG_FRAME), r.x, r.y, KEY_FRAME);
    g.gpu().dibujar(g.icono(ICON_BASE + a.cartel.rumbo), r.x + 25, r.y + 45, KEY_CTRL);
    const bool sobre_cierre = ui::sobre(e, r.x + 182, r.y + 19, 14, 14);
    g.gpu().dibujar(g.img(IMG_CLOSE), r.x + 182, r.y + 19, KEY_CTRL, sobre_cierre ? rgba(255, 255, 255) : rgba(200, 200, 200));
    if (sobre_cierre && e.clic) a.cartel.abierto = false;
    // Ajuste de líneas del texto desc-e (se corta en los espacios; '\n' fuerza un salto).
    const std::string_view t = a.desc.linea(a.cartel.desc);
    int y = r.y + TEXT_Y;
    size_t i = 0;
    while (i < t.size() && y < r.y + CARD_H - 20) {
        size_t fin = i, ultimo_blanco = std::string_view::npos;
        while (fin < t.size() && t[fin] != '\n') {
            if (t[fin] == ' ') ultimo_blanco = fin;
            if (ancho_texto(t.data() + i, fin + 1 - i) > TEXT_W) {
                if (ultimo_blanco != std::string_view::npos) fin = ultimo_blanco;
                break;
            }
            ++fin;
        }
        g.texto_sombra(t.substr(i, fin - i), static_cast<float>(r.x + TEXT_X), static_cast<float>(y), KEY_TEXT, rgba(255, 255, 254));
        y += ALTO_GLIFO + 2;
        i = fin < t.size() && (t[fin] == ' ' || t[fin] == '\n') ? fin + 1 : fin;
    }
}

}  // namespace lin

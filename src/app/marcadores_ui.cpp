// Marcadores de teleport (bookmarks).
//  - "/bookmark nombre" en el chat recuerda el lugar donde se está (C_AddBookmark); el servidor lo
//    confirma con S_Bookmarks, que también manda todos al entrar al juego.
//  - Al usar un pergamino de teleport (usos 6 y 29) o el hechizo Teleport / Mass Teleport con
//    marcadores guardados, se elige el destino: uno de ellos o "Random". Clic derecho sobre un
//    marcador lo borra (C_DeleteBookmark); Esc cancela.
#include "app/app.h"

namespace lin {

namespace {

constexpr u32 HECHIZO_TELEPORT = 5, HECHIZO_MASS_TELEPORT = 69;
constexpr int ANCHO = 240, FILA = ALTO_GLIFO + 10, TITULO = 22, FILAS_VISIBLES = 6;

// Alto fijo: las filas que no caben se recorren con la barra o la rueda.
int alto_ventana() { return TITULO + 4 + FILAS_VISIBLES * FILA + 6; }

// Posición de la ventana: la elegida al arrastrarla, o centrada al abrirla.
void posicion(const App& a, int& x, int& y) {
    x = a.destino.x < 0 ? (a.gpu.ancho() - ANCHO) / 2 : a.destino.x;
    y = a.destino.y;
}

bool igual_sin_mayusculas(std::string_view x, std::string_view y) {
    if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i)
        if ((x[i] | 0x20) != (y[i] | 0x20)) return false;
    return true;
}

// Ir al destino elegido (marcador o, con nullptr, al azar).
void ir(App& a, const Marcador* m) {
    ElegirDestino& d = a.destino;
    if (!a.cfg.ui_simple) d.enviado = true;  // the window stays up until the teleport happens (0x418d84, closed at 0x4623aa)
    else d.abierto = false;
    const u16 mapa = m ? m->mapa : 0;
    const u32 id = m ? m->id : 0;
    if (d.item) {
        enviar(a, cmd::usar_teleport(a.op, d.item, mapa, id));
    } else {
        a.destino_mapa = mapa;
        a.destino_marcador = id;
        a.destino_elegido = true;
        pedir_hechizo(a, d.hechizo, 0);
    }
}

}  // namespace

bool es_hechizo_teleport(u32 id) { return id == HECHIZO_TELEPORT || id == HECHIZO_MASS_TELEPORT; }

void eventos_marcadores(App& a, const Evento& e) {
    if (e.tipo == Ev::TELETRANSPORTE || e.tipo == Ev::MAPA) a.destino.abierto = false;  // the teleport went through
    if (e.tipo != Ev::MARCADOR) return;
    auto it = std::find_if(a.marcadores.begin(), a.marcadores.end(), [&](const Marcador& m) { return m.id == e.id; });
    if (it == a.marcadores.end()) it = a.marcadores.insert(a.marcadores.end(), Marcador{});
    it->id = e.id;
    it->mapa = static_cast<u16>(e.v[MK_MAPA]);
    it->x = static_cast<u16>(e.v[MK_X]);
    it->y = static_cast<u16>(e.v[MK_Y]);
    it->nombre.assign(e.s[0]);
}

bool comando_marcador(App& a, std::string_view orden, std::string_view nombre) {
    if (igual_sin_mayusculas(orden, "bookmark")) {
        if (nombre.empty()) a.chat.agregar("/bookmark <name>: remembers this place for teleporting.", rgba(255, 230, 120), CHAT_SISTEMA);
        else enviar(a, cmd::marcar_lugar(a.op, nombre));
        return true;
    }
    if (igual_sin_mayusculas(orden, "delbookmark")) {
        auto it = std::find_if(a.marcadores.begin(), a.marcadores.end(), [&](const Marcador& m) { return m.nombre == nombre; });
        if (it == a.marcadores.end()) {
            a.chat.agregar("/delbookmark: there is no bookmark with that name.", rgba(255, 230, 120), CHAT_SISTEMA);
        } else {
            enviar(a, cmd::borrar_marcador(a.op, nombre));
            a.marcadores.erase(it);
        }
        return true;
    }
    return false;
}

bool elegir_destino(App& a, ObjId item, u32 hechizo) {
    // Only with the Ring of Teleport Control on (S_Ability 1, bit 1 of jugador.anillos); otherwise random.
    // The original client (0x409690, 0x4ac830) opens its bookmark window whenever the ring is on, even with no bookmarks.
    if (!(a.mundo.jugador.anillos & 2) || (a.marcadores.empty() && a.cfg.ui_simple)) return false;
    a.destino = ElegirDestino{};
    a.destino.abierto = true;
    a.destino.item = item;
    a.destino.hechizo = hechizo;
    return true;
}

bool sobre_destino(const App& a) {
    if (!a.cfg.ui_simple) return sobre_ventana(a, IdVentana::DESTINO);
    int x = 0, y = 0;
    posicion(a, x, y);
    return a.destino.abierto && ui::sobre(a.entrada, x, y, ANCHO, alto_ventana());
}

void dibujar_destino(App& a) {
    ElegirDestino& d = a.destino;
    if (!d.abierto || !a.cfg.ui_simple) return;  // the original UI draws it as a framed window (dibujar_destino_orig)
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const u64 kf = clave_orden(CAPA_UI, 3), kt = clave_orden(CAPA_UI_TEXTO, 3);
    int x = 0, y = 0;
    posicion(a, x, y);
    // Arrastrar por el título (el botón de cerrar queda fuera).
    const int alto = alto_ventana();
    if (e.pulsado && ui::sobre(e, x, y, ANCHO - 22, TITULO)) {
        d.arrastrando = true;
        d.dx = e.mx - x;
        d.dy = e.my - y;
    }
    if (d.arrastrando) {
        a.clic_consumido = true;  // no camina aunque el puntero salga de la ventana
        if (!e.presionado) {
            d.arrastrando = false;
            e.clic = false;  // pegada al borde de la pantalla, soltar podría caer sobre una fila
        } else {
            x = std::clamp(e.mx - d.dx, 0, std::max(0, a.gpu.ancho() - ANCHO));
            y = std::clamp(e.my - d.dy, 0, std::max(0, a.gpu.alto() - alto));
        }
    }
    d.x = x;
    d.y = y;

    ui::recuadro(g, x, y, ANCHO, alto, 240);
    g.gpu().rectangulo(x, y, ANCHO, TITULO, kf + (1u << 24), rgba(44, 34, 24, 240));
    g.texto("Teleport to...", static_cast<float>(x + 8), static_cast<float>(y + 5), kt, ui::COLOR_TITULO);
    const bool cerrar_boton = ui::boton_texto(g, e, "x", x + ANCHO - 20, y + 3, 16, 16);

    // Filas: el marcador o "Random" al final. La rueda sobre la ventana las recorre.
    const u32 total = static_cast<u32>(a.marcadores.size()) + 1;
    const int ry = y + TITULO + 4, rancho = ANCHO - 8 - 24;
    barra_scroll_ventana(a, d.barra, x + ANCHO - 14, ry, FILAS_VISIBLES * FILA, total, FILAS_VISIBLES, d.desplaza,
                         clave_orden(CAPA_UI, 4), sobre_destino(a));
    const Marcador* elegido = nullptr;
    bool azar = false, borrado = false;
    for (int f = 0; f < FILAS_VISIBLES; ++f) {
        const size_t i = d.desplaza + static_cast<size_t>(f);
        if (i >= total) break;
        const int fy = ry + f * FILA;
        const bool encima = ui::sobre(e, x + 8, fy, rancho, FILA - 2);
        g.gpu().rectangulo(x + 8, fy, rancho, FILA - 2, kf, encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
        if (i == a.marcadores.size()) {
            g.texto("Random", static_cast<float>(x + 14), static_cast<float>(fy + 5), kt, rgba(170, 160, 140));
            azar |= encima && e.clic;
            continue;
        }
        const Marcador& m = a.marcadores[i];
        g.texto(m.nombre, static_cast<float>(x + 14), static_cast<float>(fy + 5), kt, ui::COLOR_TEXTO);
        if (encima && e.clic) elegido = &m;
        if (encima && e.clic_derecho) {  // borrarlo
            enviar(a, cmd::borrar_marcador(a.op, m.nombre));
            a.marcadores.erase(a.marcadores.begin() + static_cast<std::ptrdiff_t>(i));
            borrado = true;
            break;
        }
    }

    const bool cancelar = e.escape || (e.clic_derecho && !borrado) || cerrar_boton;
    e.escape = false;
    if (elegido || azar) ir(a, elegido);
    else if (cancelar || a.marcadores.empty()) d.abierto = false;
}

// Original UI: the bookmark list window of S3EP1U.bin. Teleport use with the Ring of Teleport Control (S_Ability 1) opens it
// (0x409690 scroll, 0x4ac830 spell); a click on a name sends the use with that bookmark (0x418b50 -> 0x418d62 `cdhhh`: item,
// then the three words of the S_Bookmarks row; the spell sends 0x500320 `ccchhh`). Class: window ctor 0x419040, draw
// 0x419360, row list 0x418a50 (rect (23,46)-(174,260), row 12 px, rows at (25,49), up to 17).
//  - translucent background 978 at (8,11), frame 979, ornament 980 at (2,11) over the rows, close 969 at (182,19);
//  - scroll bar (0x47cd80): track 971 at (176,60), up arrow at (176,52), down arrow at (177,239), range 0..count-1 (0..2 for
//    fewer than 3 rows);
//  - no title, no Random row, no buttons: names in plain colour 0x5ef7 (chat run 0x432aa0), the clicked row tinted over
//    145 px like the NPC lists (0x498d90); closing the window cancels, Esc closes it.
bool destino_orig_abierto(const App& a) { return !a.cfg.ui_simple && a.destino.abierto; }

Rect rect_destino_orig(const App& a) { return rect_lista_orig(a); }

void dibujar_destino_orig(App& a) {
    ElegirDestino& d = a.destino;
    if (!destino_orig_abierto(a)) return;
    constexpr u32 IMG_BG = 978, IMG_FRAME = 979, IMG_ORNAMENT = 980, IMG_CLOSE = 969;
    constexpr int CLOSE_X = 182, CLOSE_Y = 19, ROWS = 17, ROW_H = 12, ROW_X = 25, ROW_Y = 49, TINT_W = 145;
    constexpr int HIT_L = 23, HIT_T = 46, HIT_R = 174, HIT_B = 260;
    constexpr int SCROLL_X = 176, SCROLL_Y = 52, SCROLL_H = 199;
    arrastrar_ventana(a, IdVentana::DESTINO);
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const Rect r = rect_ventana(a, IdVentana::DESTINO);
    const u64 kb = clave_orden(CAPA_UI, 10), kf = clave_orden(CAPA_UI, 11), km = clave_orden(CAPA_UI, 12), kc = clave_orden(CAPA_UI, 16),
              kt = clave_orden(CAPA_UI_TEXTO, 16);
    g.gpu().dibujar(g.img(IMG_BG), r.x + 8, r.y + 11, kb, rgba(255, 255, 255, opacidad_ventanas(a)));
    g.gpu().dibujar(g.img(IMG_FRAME), r.x, r.y, kf);
    const bool sobre_cierre = ui::sobre(e, r.x + CLOSE_X, r.y + CLOSE_Y, 14, 14);
    g.gpu().dibujar(g.img(IMG_CLOSE), r.x + CLOSE_X, r.y + CLOSE_Y, kc, sobre_cierre ? rgba(255, 255, 255) : rgba(200, 200, 200));
    if ((sobre_cierre && e.clic) || e.escape) {
        d.abierto = false;
        e.escape = false;
        return;
    }
    const u32 total = static_cast<u32>(a.marcadores.size());
    // The bar scrolls one row per step up to the last one (the original range is count-1, 2 for fewer than 3 rows).
    const bool sobre_lista = sobre_ventana(a, IdVentana::DESTINO);
    original_scrollbar(a, d.barra, r.x + SCROLL_X, r.y + SCROLL_Y, SCROLL_H, std::max<u32>(total, 3) + ROWS - 1, ROWS, d.desplaza,
                       sobre_lista, 0, -2);
    const u32 primero = std::min(d.desplaza, total ? total - 1 : 0);
    // A click on a row: hit rows start 3 px above the drawn ones (list rect top 46 vs text 49).
    if (sobre_lista && e.clic && !d.enviado && ui::sobre(e, r.x + HIT_L, r.y + HIT_T, HIT_R - HIT_L, HIT_B - HIT_T)) {
        const u32 i = primero + static_cast<u32>((e.my - (r.y + HIT_T)) / ROW_H);
        if (i < total) {
            d.sel = static_cast<int>(i);
            ir(a, &a.marcadores[i]);
        }
    }
    for (int k = 0; k < ROWS && primero + static_cast<u32>(k) < total; ++k) {
        const u32 i = primero + static_cast<u32>(k);
        const int y = r.y + ROW_Y + k * ROW_H;
        if (d.sel == static_cast<int>(i)) g.gpu().rectangulo(r.x + ROW_X, y, TINT_W, ROW_H, km, rgba(0, 0, 85), Mezcla::PANTALLA);
        g.texto(a.marcadores[i].nombre, static_cast<float>(r.x + ROW_X), static_cast<float>(y), kt, rgb555(0x5ef7));
    }
    draw_original_overlay(a, r.x + 2, r.y + 11, IMG_ORNAMENT);
}

}  // namespace lin

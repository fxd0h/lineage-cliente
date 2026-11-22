// Ventanas del juego: dónde está cada una, moverlas arrastrando la barra de título (la posición se
// guarda en settings.ini, "ventanas="), los botones para abrirlas (Options abre los ajustes,
// ajustes_ui.cpp).
//  - Sin moverlas, cada una va en su lugar de siempre: inventario contra el borde derecho, estado y
//    hechizos a su izquierda (según cuáles estén abiertas) y el diálogo del NPC a la izquierda.
//  - Botones: con la UI original, los del panel (1005–1012.img: magia, estado, opciones,
//    inventario, cada uno con su versión resaltada); con la simple, botones de texto sobre la
//    línea de entrada del chat.
#include <algorithm>
#include <charconv>
#include <cstring>
#include <filesystem>
#include "app/app.h"
#include "proto/comandos.h"

namespace lin {

namespace {

constexpr std::string_view NOMBRES[] = {"estado", "inventario", "hechizos", "npc", "salida", "social", "intercambio", "volumen", "hpmp", "tiendapriv", "lista", "mascota", "destino", "cartel", "cocina"};
static_assert(std::size(NOMBRES) == static_cast<size_t>(IdVentana::CANTIDAD));
static_assert(static_cast<int>(IdVentana::CANTIDAD) == 15, "App::orden_ventanas se inicializa con 0..14");
constexpr int ANCHO_SALIDA = 200, ALTO_SALIDA = 120;
// Original exit window (client draw at 0x4a1570, built at 0x4a1180): background 1058 at (7,7), frame 1059, ornaments
// 1060 at (0,6), close button at (108,11), buttons Restart 1063 / Quit 1065 / Cancel 1067 (hover +1) at x 31, y 34/52/70.
constexpr int EXIT_W = 130, EXIT_H = 112;
constexpr int SEPARACION = 4, MARGEN = 7;
constexpr int PASO_LUGAR = 8;  // px entre los lugares que se prueban para una ventana que se abre

int indice(IdVentana v) { return static_cast<int>(v); }

// Ventanas de la interfaz original que viven en la lista emergente del cliente (0x4d0f30: como mucho 4 abiertas, una 5.ª cierra la más vieja) y
// comparten la zona de arrastre de 0x47bcd0: inventario, estado, libro de hechizos, intercambio, y el inventario de mascota solo en la lista.
bool original_listed(const App& a, IdVentana v) {
    if (a.cfg.ui_simple) return false;
    return v == IdVentana::ESTADO || v == IdVentana::INVENTARIO || v == IdVentana::HECHIZOS || v == IdVentana::INTERCAMBIO ||
           v == IdVentana::MASCOTA || v == IdVentana::COCINA;
}
bool original_draggable(const App& a, IdVentana v) { return original_listed(a, v) && v != IdVentana::MASCOTA; }
constexpr int MAX_LISTED_WINDOWS = 4;
// Rect de la zona de arrastre (0x47bcd0), relativo a la ventana: inventario/estado (0,5,191,25), intercambio 199 de ancho, libro de hechizos (6,9,197,29).
Rect original_drag_handle(IdVentana v) {
    if (v == IdVentana::HECHIZOS) return {6, 9, 197, 29};
    return {0, 5, v == IdVentana::INTERCAMBIO ? 199 : 191, 25};
}

void cargar_posiciones(App& a) {
    a.ventanas_cargadas = true;
    for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i) {
        std::string_view v = valor_de_personaje(a.cfg.ventanas, NOMBRES[i]);
        size_t coma = v.find(',');
        if (v.empty() || coma == std::string_view::npos) continue;
        PosVentana& p = a.pos_ventanas[i];
        std::from_chars(v.data(), v.data() + coma, p.x);
        std::from_chars(v.data() + coma + 1, v.data() + v.size(), p.y);
        p.movida = true;
    }
}

void guardar_posiciones(App& a) {
    for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i) {
        const PosVentana& p = a.pos_ventanas[i];
        if (p.movida) poner_valor_de_personaje(a.cfg.ventanas, NOMBRES[i], std::to_string(p.x) + ',' + std::to_string(p.y));
    }
    guardar_config(a.cfg);
}

// Lugar de siempre (sin moverla). Contra el borde derecho se apilan inventario, estado y hechizos:
// cada una a la izquierda de las anteriores que estén abiertas y en su lugar (una movida deja libre
// el suyo).
Rect por_defecto(const App& a, IdVentana v) {
    const int y = ui::VENTANA_Y;
    // Las ventanas con marco del cliente original tienen el tamaño de su imagen de marco.
    auto ancho = [&](IdVentana o) {
        if (!uses_original_frame(a, o)) return ui::VENTANA_ANCHO;
        return o == IdVentana::HECHIZOS ? SPELLBOOK_WINDOW_W : ORIGINAL_WINDOW_W;
    };
    const int w = ancho(v), h = !uses_original_frame(a, v) ? ui::VENTANA_ALTO : v == IdVentana::HECHIZOS ? SPELLBOOK_WINDOW_H : ORIGINAL_WINDOW_H;
    auto ocupa = [&](bool abierta_, IdVentana o) {
        const PosVentana& p = a.pos_ventanas[indice(o)];
        return abierta_ && !p.movida && !p.colocada ? ancho(o) + SEPARACION : 0;
    };
    // Bordes derechos de la pila contra el borde derecho de la pantalla.
    const int der_inv = a.gpu.ancho() - MARGEN;
    const int der_estado = der_inv - ocupa(a.ventana_inventario, IdVentana::INVENTARIO);
    const int der_hechizos = der_estado - ocupa(a.ventana_estado, IdVentana::ESTADO);
    if (!a.cfg.ui_simple && (v == IdVentana::INVENTARIO || v == IdVentana::ESTADO || v == IdVentana::HECHIZOS)) {
        // Valores por omisión originales (0x44cf6a, 0x4aefcb): inventario en 640-190, libro de hechizos en 640-207, estado en 0; y 0 del
        // bloque de 640x480, que se centra en una vista más ancha (nuestra franja del título queda arriba de la vista, no dentro).
        const int hx = (a.gpu.ancho() - 640) / 2;
        return {hx + (v == IdVentana::INVENTARIO ? 640 - ORIGINAL_WINDOW_W : v == IdVentana::HECHIZOS ? 640 - SPELLBOOK_WINDOW_W : 0),
                0, w, h};
    }
    switch (v) {
    case IdVentana::INVENTARIO: return {der_inv - w, y, w, h};
    case IdVentana::ESTADO: return {der_estado - w, y, w, h};
    case IdVentana::HECHIZOS: return {der_hechizos - w, y, w, h};
    case IdVentana::NPC:
        if (!a.cfg.ui_simple) {
            int nw, nh;
            original_npc_size(a, nw, nh);
            return {MARGEN, y, nw, nh};
        }
        return {MARGEN, y, w, h};
    case IdVentana::SALIDA:
        if (!a.cfg.ui_simple) return {(a.gpu.ancho() - EXIT_W) / 2, a.gpu.alto() / 3, EXIT_W, EXIT_H};
        return {(a.gpu.ancho() - ANCHO_SALIDA) / 2, a.gpu.alto() / 3, ANCHO_SALIDA, ALTO_SALIDA};
    case IdVentana::SOCIAL: return rect_social(a);
    case IdVentana::INTERCAMBIO: return rect_intercambio(a);
    case IdVentana::VOLUMEN: return rect_volumen(a);
    case IdVentana::HPMP: return rect_hpmp_flotante(a);
    case IdVentana::TIENDA_PRIV: return rect_tienda_original(a);
    case IdVentana::LISTA: return rect_lista_orig(a);
    case IdVentana::MASCOTA: return rect_mascota_orig(a);
    case IdVentana::DESTINO: return rect_destino_orig(a);
    case IdVentana::CARTEL: return rect_cartel(a);
    case IdVentana::COCINA: return rect_cocina(a);
    default: return {0, y, w, h};
    }
}

bool abierta(const App& a, IdVentana v) {
    switch (v) {
    case IdVentana::ESTADO: return a.ventana_estado;
    case IdVentana::INVENTARIO: return a.ventana_inventario;
    case IdVentana::HECHIZOS: return a.ventana_hechizos;
    case IdVentana::NPC: return a.npc || a.tienda != TipoTienda::NINGUNA;
    case IdVentana::SALIDA: return a.ventana_salida;
    case IdVentana::SOCIAL: return a.ventana_social;
    case IdVentana::INTERCAMBIO: return a.intercambio.abierto;
    case IdVentana::VOLUMEN: return a.ventana_volumen;
    case IdVentana::HPMP: return hpmp_flotante_abierta(a);
    case IdVentana::TIENDA_PRIV: return tienda_original_abierta(a);
    case IdVentana::LISTA: return a.lista.abierta && !a.cfg.ui_simple;
    case IdVentana::MASCOTA: return mascota_orig_abierta(a);
    case IdVentana::DESTINO: return destino_orig_abierto(a);
    case IdVentana::CARTEL: return a.cartel.abierto;
    case IdVentana::COCINA: return cocina_abierta(a);
    default: return false;
    }
}

bool solapan(const Rect& a, const Rect& b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }

bool contiene_puntero(const App& a, IdVentana v) {
    if (!abierta(a, v)) return false;
    Rect r = rect_ventana(a, v);
    return ui::sobre(a.entrada, r.x, r.y, r.w, r.h);
}

// La de más arriba abierta bajo el puntero (índice), o -1.
int ventana_bajo_puntero(const App& a) {
    for (int i = indice(IdVentana::CANTIDAD) - 1; i >= 0; --i)
        if (contiene_puntero(a, static_cast<IdVentana>(a.orden_ventanas[i]))) return a.orden_ventanas[i];
    return -1;
}

void al_frente(App& a, IdVentana v) {
    u8* o = a.orden_ventanas;
    u8* fin = o + indice(IdVentana::CANTIDAD);
    std::rotate(std::find(o, fin, static_cast<u8>(v)), std::find(o, fin, static_cast<u8>(v)) + 1, fin);
}

// Lugar para una ventana que se abre sin haberla movido: el de siempre si no tapa a otra abierta;
// si no, el primero libre recorriendo de arriba a la derecha hacia abajo a la izquierda, sobre la
// barra de atajos. Si no hay ninguno, el de siempre (y queda adelante).
void colocar(App& a, IdVentana v) {
    PosVentana& p = a.pos_ventanas[indice(v)];
    p.colocada = false;
    if (original_draggable(a, v)) return;  // el original nunca aparta una ventana
    if (p.movida || v == IdVentana::HPMP) return;  // el par flotante de HP/MP siempre empieza arriba al centro
    const Rect r = rect_ventana(a, v);
    auto libre = [&](const Rect& c) {
        for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i) {
            const IdVentana o = static_cast<IdVentana>(i);
            if (o != v && o != IdVentana::HPMP && abierta(a, o) && solapan(c, rect_ventana(a, o))) return false;
        }
        return true;
    };
    if (libre(r)) return;
    const int abajo = a.barra_y > r.h ? a.barra_y - SEPARACION : a.gpu.alto();
    for (int y = ui::VENTANA_Y; y + r.h <= abajo; y += PASO_LUGAR) {
        for (int x = a.gpu.ancho() - r.w - MARGEN; x >= MARGEN; x -= PASO_LUGAR) {
            if (!libre({x, y, r.w, r.h})) continue;
            p.x = x;
            p.y = y;
            p.colocada = true;
            return;
        }
    }
}

// Cierra una ventana de la lista como lo haría su propia X (el intercambio se cancela en el servidor).
void cerrar_listada(App& a, IdVentana v) {
    switch (v) {
    case IdVentana::ESTADO: a.ventana_estado = false; break;
    case IdVentana::INVENTARIO: a.ventana_inventario = false; break;
    case IdVentana::HECHIZOS: a.ventana_hechizos = false; break;
    case IdVentana::MASCOTA: a.ventana_mascota = false; break;
    case IdVentana::COCINA: a.cocina.abierta = false; break;
    case IdVentana::INTERCAMBIO: enviar(a, cmd::cancelar_intercambio(a.op)); break;
    default: break;
    }
}

// 0x4d0f30: una ventana recién abierta va al frente de la lista de 4 casillas; con la lista llena se cierra la más vieja (la que está
// más atrás en el orden de apilado).
void limitar_abiertas(App& a, IdVentana nueva) {
    int abiertas = 0;
    for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i) {
        const IdVentana o = static_cast<IdVentana>(i);
        abiertas += original_listed(a, o) && abierta(a, o);
    }
    for (int k = 0; k < indice(IdVentana::CANTIDAD) && abiertas > MAX_LISTED_WINDOWS; ++k) {
        const IdVentana o = static_cast<IdVentana>(a.orden_ventanas[k]);  // de atrás hacia adelante
        if (o == nueva || !original_listed(a, o) || !abierta(a, o)) continue;
        cerrar_listada(a, o);
        --abiertas;
    }
}

}  // namespace

bool alguna_ventana_abierta(const App& a) {
    for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i)
        if (static_cast<IdVentana>(i) != IdVentana::HPMP && abierta(a, static_cast<IdVentana>(i))) return true;  // la barra flotante de HP/MP siempre está
    return false;
}

void close_listed_windows(App& a) {
    a.ventana_estado = a.ventana_inventario = a.ventana_hechizos = a.ventana_salida = false;
    if (!a.cfg.ui_simple && a.intercambio.abierto) enviar(a, cmd::cancelar_intercambio(a.op));  // Esc cancela un intercambio pendiente
}

void guardar_posiciones_ventanas(App& a) { guardar_posiciones(a); }

void actualizar_ventanas(App& a) {
    if (!a.ventanas_cargadas) cargar_posiciones(a);
    for (int i = 0; i < indice(IdVentana::CANTIDAD); ++i) {
        const IdVentana v = static_cast<IdVentana>(i);
        const bool ab = abierta(a, v);
        if (ab && !a.ventana_abierta_antes[i]) {
            colocar(a, v);
            al_frente(a, v);
            if (original_listed(a, v)) limitar_abiertas(a, v);
        }
        if (!ab) a.pos_ventanas[i].colocada = false;
        a.ventana_abierta_antes[i] = ab;
    }
    a.ventana_arriba = ventana_bajo_puntero(a);
    if (a.entrada.pulsado && a.ventana_arriba >= 0) al_frente(a, static_cast<IdVentana>(a.ventana_arriba));
    // Un doble clic derecho dentro de una ventana la cierra (WM_RBUTTONDBLCLK, inventario 0x44d1b0 -> 0x4d1200).
    if (a.entrada.doble_derecho && a.ventana_arriba >= 0 && original_draggable(a, static_cast<IdVentana>(a.ventana_arriba))) {
        cerrar_listada(a, static_cast<IdVentana>(a.ventana_arriba));
        a.entrada.doble_derecho = a.entrada.clic_derecho = false;
        a.clic_consumido = true;
    }
}

Rect rect_ventana(const App& a, IdVentana v) {
    Rect r = por_defecto(a, v);
    const PosVentana& p = a.pos_ventanas[indice(v)];
    if ((p.movida || p.colocada) && original_draggable(a, v)) {
        // El original mantiene una ventana arrastrada entera dentro de la pantalla (0x47bd90).
        r.x = std::clamp(p.x, 0, std::max(0, a.gpu.ancho() - r.w));
        r.y = std::clamp(p.y, 0, std::max(0, a.gpu.alto() - r.h));
    } else if (p.movida || p.colocada) {
        // Siempre con la barra de título dentro de la pantalla (la ventana puede haber quedado fuera
        // al achicar la ventana del sistema o cambiar el zoom).
        r.x = std::clamp(p.x, 40 - r.w, a.gpu.ancho() - 40);
        r.y = std::clamp(p.y, 0, a.gpu.alto() - ui::ALTO_TITULO);
    }
    return r;
}

bool sobre_ventana(const App& a, IdVentana v) { return ventana_bajo_puntero(a) == indice(v); }

void barra_scroll_ventana(App& a, ui::BarraScroll& st, int x, int y, int alto, u32 total, u32 visibles, u32& pos,
                          u64 clave, bool sobre_lista) {
    ui::barra_scroll(*a.g, a.entrada, st, x, y, 10, alto, total, visibles, pos, clave, sobre_lista);
    if (st.presionada) a.clic_consumido = true;  // igual que al arrastrar una ventana: no camina
}

bool sobre_alguna_ventana(const App& a) { return ventana_bajo_puntero(a) >= 0; }

void arrastrar_ventana(App& a, IdVentana v) {
    if (!a.ventanas_cargadas) cargar_posiciones(a);
    const EntradaFrame& e = a.entrada;
    Rect r = rect_ventana(a, v);
    PosVentana& p = a.pos_ventanas[indice(v)];
    // Se toma de la barra de título, salvo el botón de cerrar (los últimos 30 px). The original windows only take the
    // drag handle rect of their ctor (0x47bcd0), minus the close button.
    Rect grab{r.x, r.y, r.w - 30, ui::ALTO_TITULO};
    const bool original = original_draggable(a, v);
    if (original) {
        const Rect hr = original_drag_handle(v);
        grab = {r.x + hr.x, r.y + hr.y, std::min(hr.w, r.w - 30) - hr.x, hr.h};
    }
    if (e.pulsado && a.ventana_arrastrada < 0 && ui::sobre(e, grab.x, grab.y, grab.w, grab.h)) {
        a.ventana_arrastrada = indice(v);
        a.arrastre_dx = e.mx - r.x;
        a.arrastre_dy = e.my - r.y;
        a.clic_consumido = true;  // no camina
    }
    if (a.ventana_arrastrada != indice(v)) return;
    if (e.presionado) {
        p.x = e.mx - a.arrastre_dx;
        p.y = e.my - a.arrastre_dy;
        if (original) {
            p.x = std::clamp(p.x, 0, std::max(0, a.gpu.ancho() - r.w));
            p.y = std::clamp(p.y, 0, std::max(0, a.gpu.alto() - r.h));
        }
        p.movida = true;
        p.colocada = false;
    } else {
        a.ventana_arrastrada = -1;
        guardar_posiciones(a);
    }
}

// La de los ajustes; opaca para la ventana que se está dibujando si tapa a otra (se leería el
// texto de la de abajo a través de su fondo).
u8 opacidad_ventanas(const App& a) {
    if (!a.cfg.ui_simple) {  // original: opción [0x6e1008] = 31, 20, 13 o 7 (de 31), la fija la acción "alpha" de la página de Opciones
        static constexpr u8 PASOS[4] = {255, 165, 107, 58};
        return PASOS[std::clamp(a.cfg.alpha_orig, 0, 3)];
    }
    return a.ventana_sobre_otra ? 255 : static_cast<u8>(std::clamp(a.cfg.opacidad_ventanas, 60, 255));
}

bool tapa_a_otra(const App& a, IdVentana v) {
    const Rect r = rect_ventana(a, v);
    for (int i = 0; i < indice(IdVentana::CANTIDAD) && a.orden_ventanas[i] != indice(v); ++i) {
        const IdVentana o = static_cast<IdVentana>(a.orden_ventanas[i]);
        if (abierta(a, o) && solapan(r, rect_ventana(a, o))) return true;
    }
    return false;
}

// Botón de la franja inferior (ctor 0x47c6d0, dibujo 0x47c760): el primer id `n` en reposo, el segundo (n + 1) mientras el puntero está encima o
// presionado (el hover fija +0x2d, la presión +0x2c: ambos eligen el id +0x4c / primero + 1). El botón de correo es arte simple y alterna
// la ventana de correo (correo_ui.cpp). El arte en reposo es la versión del archivo base (Graficos::img_base).
static bool boton_panel(App& a, u32 n, int x, int y, bool mono = true) {
    Graficos& g = *a.g;
    const RegionAtlas reposo = mono ? g.img_base(n) : g.img(n);
    const bool encima = ui::sobre(a.entrada, x, y, reposo.w, std::max<int>(reposo.h, 16));
    const u64 k = clave_orden(CAPA_UI, 1);
    if (encima) {
        g.gpu().dibujar(g.img(n + 1), x, y, k);
    } else {
        g.gpu().dibujar(reposo, x, y, k);
    }
    return encima && a.entrada.clic;
}

// Ayudas de la franja (administrador de ayudas [0x8f0b6c], ítems armados en 0x4bc1ef..: rect {l, t, r, b} (r, b exclusivos), x del ancla, índice de string-e).
// El puntero debe quedarse dentro del rect 700 ms (0x2bc, temporizador 0x4ba8e0 -> 0x4bb830); un clic la oculta y rearma la espera (0x4bbb50).
// Dibujo 0x4bb8f0: ancho 142 (22 celdas de 6 px + 10) o, para una sola línea, 6 * largo + 21 si es más angosto; alto 12 * líneas + 10;
// en (x del ancla, 478), corrida a la izquierda para caber en 640 y arriba de la franja (460 - h) porque pasaría de 480; relleno oscurecido al 50%
// (0x49a440), bisel de 2 px gris claro 23/31 (arriba, izquierda) y gris 15/31 (abajo, derecha), texto blanco en (x + 5, y + 5), paso de 12 px.
struct HintBoton { int l, r, ancla_x; u16 texto; };
constexpr HintBoton HINTS_PANEL[8] = {{491, 506, 495, 613}, {506, 525, 495, 1250}, {526, 543, 516, 614}, {544, 562, 537, 615},
                                      {563, 579, 558, 616}, {580, 600, 579, 617}, {601, 617, 600, 618}, {618, 639, 621, 619}};

static void hint_panel(App& a, int vx, int vy) {
    int idx = -1;
    const int mx = a.entrada.mx - vx, my = a.entrada.my - vy;
    if (my >= 460 && my < 478)
        for (int i = 0; i < 8; ++i)
            if (mx >= HINTS_PANEL[i].l && mx < HINTS_PANEL[i].r) idx = i;
    const u64 ahora = a.entrada.ahora_ms;
    if (idx != a.hint_boton || (idx >= 0 && a.entrada.clic)) {
        a.hint_boton = idx;
        a.hint_t0 = ahora;
    }
    if (idx < 0 || !(a.cfg.opciones_orig >> OPC_TOOLTIP & 1) || ahora - a.hint_t0 < 700) return;
    std::string_view t = a.mensajes.linea(HINTS_PANEL[idx].texto);
    if (t.empty()) return;
    std::string_view filas[8];
    int n = 0;
    while (!t.empty() && n < 8) {
        const size_t c = bytes_que_caben(t.data(), t.size(), 22 * 6);
        if (c == 0) break;
        filas[n++] = t.substr(0, c);
        t.remove_prefix(c);
        while (!t.empty() && t[0] == ' ') t.remove_prefix(1);
    }
    if (n == 0) return;
    int w = 142;
    if (n == 1) w = std::min(w, 6 * static_cast<int>(filas[0].size()) + 21);
    w &= ~3;
    const int h = 12 * n + 10;
    int x = HINTS_PANEL[idx].ancla_x, y = 478;
    if (x + w > 640) x = 640 - w;
    if (y + h > 480) y = 460 - h;
    x += vx, y += vy;
    Gpu& gpu = a.g->gpu();
    const u64 kf = clave_orden(CAPA_UI, 30), kt = clave_orden(CAPA_UI_TEXTO, 30);
    const u32 claro = rgba(189, 189, 189), oscuro = rgba(123, 123, 123);
    gpu.rectangulo(x, y, w, h, kf, rgba(0, 0, 0, 128));
    gpu.rectangulo(x, y, w - 1, 1, kf, claro);
    gpu.rectangulo(x + 1, y + 1, w - 2, 1, kf, claro);
    gpu.rectangulo(x, y + 1, 1, h - 1, kf, claro);
    gpu.rectangulo(x + 1, y + 2, 1, h - 2, kf, claro);
    gpu.rectangulo(x + 1, y + h - 1, w - 1, 1, kf, oscuro);
    gpu.rectangulo(x + 2, y + h - 2, w - 2, 1, kf, oscuro);
    gpu.rectangulo(x + w - 1, y, 1, h - 1, kf, oscuro);
    gpu.rectangulo(x + w - 2, y + 1, 1, h - 2, kf, oscuro);
    for (int i = 0; i < n; ++i)
        a.g->texto_sombra(filas[i], static_cast<float>(x + 5), static_cast<float>(y + 5 + 12 * i), kt, rgba(255, 255, 254));
}

void dibujar_botones_panel(App& a, int x, int y) {
    // Franja inferior original (S3EP1U 0x459595..0x45980a): ocho botones en y=460 de la pantalla de 640x480; x es
    // 491 ayuda, 507 correo, 527 estado, 544 magia, 563 inventario, 580 social, 601 opciones, 618 salir.
    constexpr int BY = 460;
    if (boton_panel(a, 1017, x + 491, y + BY)) alternar_ayuda(a);
    if (boton_panel(a, 1585, x + 507, y + BY, false)) {
        if (a.correo.abierto) a.correo.abierto = a.correo.escribiendo = false;
        else comando_correo(a, "mail");
    }
    if (boton_panel(a, 1007, x + 527, y + BY)) a.ventana_estado = !a.ventana_estado;
    if (boton_panel(a, 1005, x + 544, y + BY)) a.ventana_hechizos = !a.ventana_hechizos;
    if (boton_panel(a, 1011, x + 563, y + BY)) a.ventana_inventario = !a.ventana_inventario;
    if (boton_panel(a, 1015, x + 580, y + BY)) alternar_comunidad(a);
    if (boton_panel(a, 1009, x + 601, y + BY)) alternar_opciones(a);
    if (boton_panel(a, 1013, x + 618, y + BY)) a.ventana_salida = !a.ventana_salida;
    hint_panel(a, x, y);
}

// UI simple: 6 botones en 2 columnas × 3 filas, a la izquierda del minimapa.
static constexpr std::string_view BOTONES_INF[6] = {"Magic", "Status", "Items", "Social", "Options", "Exit"};
static bool* boton_inf_abierta(App& a, int i) {
    switch (i) {
    case 0: return &a.ventana_hechizos;
    case 1: return &a.ventana_estado;
    case 2: return &a.ventana_inventario;
    case 3: return &a.ventana_social;
    case 4: return &a.ventana_ajustes;
    default: return &a.ventana_salida;
    }
}
static int col_botones_inf() {
    int w = 0;
    for (std::string_view t : BOTONES_INF) w = std::max(w, ancho_texto(t.data(), t.size()) + 10);
    return w;
}

int ancho_botones_texto() { return 2 * col_botones_inf() + 4; }  // ancho del bloque de 2 columnas

// Dibuja el bloque de botones con su borde derecho en x_der y su tope en y_top. Devuelve el borde izq.
int dibujar_botones_texto(App& a, int x_der, int y_top) {
    const int cw = col_botones_inf(), fh = ALTO_GLIFO + 4;
    const int x0 = x_der - (2 * cw + 4);
    for (int i = 0; i < 6; ++i) {
        const int bx = x0 + (i / 3) * (cw + 4), by = y_top + (i % 3) * (fh + 2);
        if (ui::boton_texto(*a.g, a.entrada, BOTONES_INF[i], bx, by, cw, fh)) {
            bool* abierta = boton_inf_abierta(a, i);
            *abierta = !*abierta;
        }
    }
    return x0;
}

// Recuadro con título y "[x]" para las ventanas chicas (teclas, salida). Devuelve true al cerrar.
static bool recuadro(App& a, Rect r, std::string_view titulo) {
    Graficos& g = *a.g;
    const u8 op = opacidad_ventanas(a);
    g.gpu().rectangulo(r.x, r.y, r.w, r.h, clave_orden(CAPA_UI, 10), rgba(10, 8, 6, op));
    g.gpu().rectangulo(r.x + 1, r.y + 1, r.w - 2, ui::ALTO_TITULO - 2, clave_orden(CAPA_UI, 11),
                       rgba(40, 32, 22, static_cast<u8>(std::min(255, op + 60))));
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    g.texto(titulo, static_cast<float>(r.x + 8), static_cast<float>(r.y + 6), kt, rgba(214, 190, 140));
    g.texto("[x]", static_cast<float>(r.x + r.w - 26), static_cast<float>(r.y + 6), kt, rgba(255, 255, 254));
    return (a.entrada.clic && ui::sobre(a.entrada, r.x + r.w - 28, r.y + 2, 26, 20)) || a.entrada.escape;
}

void dibujar_ayuda_teclas(App& a) {
    if (!a.ayuda_teclas) return;
    static constexpr const char* FILAS[][2] = {
        {"F1", "This help"},
        {"F2", "Simple / original interface"},
        {"F3 / Shift+F3", "Debug overlay / FPS counter"},
        {"F4 / Shift+F4", "Pick up a nearby item (simple UI: camera) / shade in shadows"},
        {"F5-F12 / Ctrl+1..8", "Use the assigned spell or item (Alt+F10 = F10)"},
        {"Shift+F5", "Cycle scaling filter (integer/xBR/FSR/CRT)"},
        {"Ctrl+P / Shift+F12", "Screenshot (Capture/LinC0000.bmp, simple UI: captura.bmp)"},
        {"Shift+click", "Turn that way (to face someone)"},
        {"1..0 / Alt+1..0", "Shortcut bar (spells, items)"},
        {"Enter", "Open chat / send (Esc: cancel)"},
        {"A", "Keep attacking the target (until it dies or you walk)"},
        {"Ctrl+click", "Force attack (NPCs, other players)"},
        {"/pk", "PK mode: clicking a player attacks"},
        {"/exclude Name", "Block / unblock a player (no name: list)"},
        {"/buddy, /addbuddy, /delbuddy", "Buddy list"},
        {"Middle click", "Select bar slot / use it on target"},
        {"Ctrl+I / A / S", "Inventory / status / magic"},
        {"Ctrl+C / V / Q / H", "Social / options / restart-quit / this help"},
        {"Ctrl+T / Ctrl+O / Ctrl+X", "Chat size (4 steps) / chat colours / PK attack mode (original UI)"},
        {"Ctrl+Y / Ctrl+Z", "GM only: pick a target, send opcode 83 (Z repeats)"},
        {"Ctrl +/- / Ctrl 0", "Zoom in / out / auto"},
        {"Arrows, mouse", "Walk; click a monster to attack"},
        {"Ctrl+click", "Attack a player or NPC"},
        {"Enter / Esc", "Send chat / close window"},
    };
    const int n = static_cast<int>(sizeof FILAS / sizeof FILAS[0]);
    // Columnas a medida del texto: la de la tecla mide lo que su entrada más larga; la descripción ocupa el
    // resto (entre 160 y 300 px) y lo que no cabe se parte en renglones. Con la fuente nueva se salían.
    auto partir = [](std::string_view t, int ancho, auto&& f) {
        while (!t.empty()) {
            size_t k = bytes_que_caben(t.data(), t.size(), ancho);
            if (k == 0) k = t.size();
            f(t.substr(0, k));
            t = t.substr(k);
            while (!t.empty() && t.front() == ' ') t.remove_prefix(1);
        }
    };
    int col_tecla = 0, col_desc = 0;
    for (int i = 0; i < n; ++i) {
        col_tecla = std::max(col_tecla, ancho_texto(FILAS[i][0], std::strlen(FILAS[i][0])));
        col_desc = std::max(col_desc, ancho_texto(FILAS[i][1], std::strlen(FILAS[i][1])));
    }
    col_tecla += 16;
    const int ancho_desc = std::clamp(std::min(col_desc, a.gpu.ancho() - 20 - col_tecla - 24), 160, 300);
    const int ancho_total = col_tecla + ancho_desc + 24;
    constexpr int ALTO_LINEA = ALTO_GLIFO + 4, ENTRE_FILAS = 4;
    int renglones_total = 0;
    for (int i = 0; i < n; ++i) {
        int lineas = 0;
        partir(FILAS[i][1], ancho_desc, [&](std::string_view) { ++lineas; });
        renglones_total += std::max(1, lineas);
    }
    Rect r{(a.gpu.ancho() - ancho_total) / 2, 60, ancho_total,
           ui::ALTO_TITULO + 14 + renglones_total * ALTO_LINEA + n * ENTRE_FILAS};
    if (recuadro(a, r, "Keys (F1)") || a.entrada.escape) {
        a.ayuda_teclas = false;
        return;
    }
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    int linea = 0;
    for (int i = 0; i < n; ++i) {
        const int primera = linea;
        partir(FILAS[i][1], ancho_desc, [&](std::string_view t) {
            const float y = static_cast<float>(r.y + ui::ALTO_TITULO + 8 + linea * ALTO_LINEA + i * ENTRE_FILAS);
            if (linea == primera) a.g->texto(FILAS[i][0], static_cast<float>(r.x + 12), y, kt, rgba(255, 230, 120));
            a.g->texto(t, static_cast<float>(r.x + 12 + col_tecla), y, kt, rgba(230, 220, 190));
            ++linea;
        });
        if (linea == primera) ++linea;  // fila vacía: ocupa su renglón igual que en el cálculo
    }
}

void dibujar_salida(App& a) {
    if (!a.ventana_salida) return;
    arrastrar_ventana(a, IdVentana::SALIDA);
    Rect r = rect_ventana(a, IdVentana::SALIDA);
    if (!a.cfg.ui_simple) {
        if (draw_original_frame(a, r.x, r.y, 1058, 7, 7, 1059, 108, 11)) a.ventana_salida = false;
        draw_original_overlay(a, r.x, r.y + 6, 1060);
        // Menú de Esc (0x4a1180): botón 1 = Reiniciar estando muerto (C_RESTART) / cambiar de personaje estando vivo (C_CHANGECHAR,
        // rechazado dentro de los 15 s de un combate, texto 130); botón 2 = salir (rechazado dentro de los 15 s de un combate salvo muerto o
        // en una celda segura, texto 131); el botón 3 cierra. RE_ROUND4 sección 5.
        const bool muerto = a.mundo.jugador.muerto;
        const bool combate = a.t_combate && a.entrada.ahora_ms < a.t_combate + 15000;
        auto aviso = [&](size_t id) { a.chat.agregar(a.mensajes.linea(id), rgba(255, 255, 255), CHAT_SISTEMA); };
        if (draw_original_button(a, r.x + 31, r.y + 34, 1063, 1064)) {
            if (muerto) {
                reiniciar(a);
                a.ventana_salida = false;
            } else if (combate) {
                aviso(130);
            } else {
                enviar(a, cmd::cambiar_personaje(a.op));
                a.ventana_salida = false;
            }
        } else if (draw_original_button(a, r.x + 31, r.y + 52, 1065, 1066)) {
            const Slot yo = a.mundo.objetos.buscar(a.mundo.jugador.id);
            const bool segura = yo != SIN_SLOT && tipo_zona_celda(a.mundo, a.rec, a.mundo.objetos.col<O_X>()[yo],
                                                                  a.mundo.objetos.col<O_Y>()[yo]) == TipoZona::SEGURA;
            if (!muerto && !segura && combate) aviso(131);
            else a.salir = true;  // cerrar_app sends C_QUITGAME
        } else if (draw_original_button(a, r.x + 31, r.y + 70, 1067, 1068)) {
            a.ventana_salida = false;
        }
        return;
    }
    if (recuadro(a, r, "Exit")) {
        a.ventana_salida = false;
        return;
    }
    // Restart: vuelve al lugar de retorno de la zona (también sirve para salir de un mapa que esta
    // instalación no tiene). El servidor deja el HP en el nivel del personaje.
    if (ui::boton_texto(*a.g, a.entrada, "Restart (back to town)", r.x + 12, r.y + 32, r.w - 24, 20)) {
        reiniciar(a);
        a.ventana_salida = false;
    }
    // Elegir otro personaje (C_ChangeChar): el servidor saca al personaje y manda la lista.
    if (ui::boton_texto(*a.g, a.entrada, "Character select", r.x + 12, r.y + 60, r.w - 24, 20)) {
        enviar(a, cmd::cambiar_personaje(a.op));
        a.ventana_salida = false;
    }
    if (ui::boton_texto(*a.g, a.entrada, "Quit game", r.x + 12, r.y + 88, r.w - 24, 20)) a.salir = true;
}

void avisar_mapa(App& a, MapId mapa) {
    char t[160];
    std::snprintf(t, sizeof t, "map/%u", mapa);
    std::error_code ec;
    if (std::filesystem::is_directory(a.rec.ruta_archivo(t), ec)) return;
    std::snprintf(t, sizeof t,
                  "The server sent you to map %u, which this game installation does not have. Use Exit > Restart to go back.",
                  mapa);
    a.chat.agregar(t, rgba(255, 120, 100), CHAT_SISTEMA);
}

}  // namespace lin

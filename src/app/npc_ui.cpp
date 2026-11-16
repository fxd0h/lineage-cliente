// Entregable 7: NPCs y comercio.
//  - Hablar: clic sobre un NPC (no monstruo, ítem ni personaje) camina hasta él y manda C_NPCTalk;
//    el servidor responde con la página (S_NPCTalkReturn) y se muestra en el diálogo, a la
//    izquierda. Los vínculos "link" cambian de página sin pasar por el servidor; los "action"
//    se mandan con C_NPCAction ("buy", "sell", "retrieve", "teleport ..."). "deposit" lo resuelve
//    el cliente: abre el almacén con el propio inventario.
//  - Tienda y almacén: S_ShopSellList (comprar), S_ShopBuyList (vender) y S_RetrieveList
//    (retirar) abren la lista en la misma ventana. Clic en una pila pregunta cuántos (diálogo de
//    cantidad; lo que no se apila suma 1), Shift+clic suma 10, clic derecho resta 1 (con Shift,
//    nada); OK manda C_Result.
//  - Teletransporte: el servidor manda S_Teleport y espera C_Teleport para mover al personaje.
//  - Maestros de magia: el vínculo "buyspell" no va con C_NPCAction sino con C_SkillBuy; la lista
//    (S_SkillBuy) se elige como una tienda (un hechizo por fila) y OK manda C_SkillBuyOK.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "app/app.h"
#include "app/item_icon.h"
#include "recursos/fuente_ttf.h"

namespace lin {

namespace {

constexpr int MARGEN = 14, ARRIBA = 30;
constexpr int ANCHO_PAGINA = ui::VENTANA_ANCHO - 2 * MARGEN;
constexpr int ALTO_PAGINA = ui::VENTANA_ALTO - ARRIBA - 12;
// Original NPC dialog (client class at 0x43f130, draw 0x43f7f0): background 1355 at (8,11), frame 1356, ornaments
// 1101 at (2,11), close button at (226,15); scroll bar on x 226 with arrows at y 47 and 293 over the track 1354.
constexpr u32 IMG_NPC_BG = 1355, IMG_NPC_FRAME = 1356, IMG_NPC_ORNAMENT = 1101, IMG_NPC_TRACK = 1354;
constexpr int NPC_CLOSE_X = 226, NPC_CLOSE_Y = 15, NPC_TEXT_X = 21, NPC_TEXT_Y = 48, NPC_TEXT_W = 197, NPC_TEXT_H = 251;
constexpr int NPC_SCROLL_X = 226, NPC_SCROLL_Y = 47, NPC_SCROLL_H = 258, NPC_LINE = 12;  // scroll steps of a text line

// Original-client text style in the HTML page: Arial-like face, no outline (see recursos/html.h and fuente_ttf.cpp).
bool original_text(const App& a) { return !a.cfg.ui_simple && a.cfg.fuente_original; }
// The page window sets its text rectangle to the window rectangle inset by (3, 3, 7, 3) (SetRect at 0x43b4ea, from
// left+3, top+3, right-7, bottom-3), when the client class at 0x43f130 builds it. The window rectangle itself is
// the widget rect (10,48)-(207,299) built at 0x458994..0x4589b0 (ctor 0x43e170), shifted +11 in x to the frame's
// origin (it then spans the panel border to border: bright lines at x 22/218, y 47/299 in the capture).
constexpr int HTML_INSET_L = 3, HTML_INSET_T = 3, HTML_INSET_R = 7, HTML_INSET_B = 3;
int page_width(const App& a) {
    if (a.cfg.ui_simple) return ANCHO_PAGINA;
    return original_text(a) ? NPC_TEXT_W - HTML_INSET_L - HTML_INSET_R : NPC_TEXT_W;
}
constexpr int FILA_TIENDA = 34, FILAS_TIENDA = 6;
constexpr int DISTANCIA_CIERRE = 6;  // al alejarse más, el diálogo se cierra
constexpr u32 PRECIO_RETIRO = 30;    // adena por ítem retirado (C_Result del servidor)

bool medir_imagen(void* ctx, u32 n, int& w, int& h) {
    const RegionAtlas& r = static_cast<App*>(ctx)->g->img(n);
    if (!r.valida()) return false;
    w = r.w;
    h = r.h;
    return true;
}

void abrir_pagina(App& a, ObjId npc, std::string_view id, std::string_view datos) {
    std::vector<u8> html;
    std::string nombre = std::string(id) + "-e.html";
    if (!a.rec.texto(nombre, html)) {
        nombre = std::string(id) + ".html";
        if (!a.rec.texto(nombre, html)) {
            std::string aviso = "(page \"" + std::string(id) + "\" not found)";
            html.assign(aviso.begin(), aviso.end());
        }
    }
    if (g_traza_html)
        std::fprintf(stderr, "[html %.*s] datos=\"%.*s\"\n%.*s\n", static_cast<int>(id.size()), id.data(), static_cast<int>(datos.size()),
                     datos.data(), static_cast<int>(html.size()), reinterpret_cast<const char*>(html.data()));
    a.npc = npc;
    a.npc_pagina = id;
    // Los datos que son un "$N" (el nombre del mayordomo en "agpay", por ejemplo) van traducidos.
    // (`datos` puede ser a.npc_datos: los vínculos locales vuelven a pasar los mismos.)
    std::string traducidos;
    for (size_t i = 0; i <= datos.size();) {
        const size_t fin = std::min(datos.find(SEPARADOR_DATOS, i), datos.size());
        if (i) traducidos += SEPARADOR_DATOS;
        traducidos += a.desc.resolver(datos.substr(i, fin - i));
        i = fin + 1;
    }
    a.npc_datos = std::move(traducidos);
    a.npc_desplazamiento = 0;
    a.tienda = TipoTienda::NINGUNA;
    maquetar_html({reinterpret_cast<const char*>(html.data()), html.size()}, a.npc_datos, SEPARADOR_DATOS,
                  nombre_personaje(a), page_width(a), medir_imagen, &a, a.npc_html, original_text(a));
}

void cerrar(App& a) {
    a.npc = 0;
    a.tienda = TipoTienda::NINGUNA;
    a.tienda_filas.clear();
    a.tienda_marcada = a.tienda_flecha = -1;
}

}  // namespace

// Adena en el inventario (el ítem "$4"; no "$40...").
u32 adena_inventario(const App& a) {
    const TablaItems& t = a.mundo.inventario.items;
    for (Slot s = 0; s < t.tamano(); ++s) {
        std::string_view n = nombre_item(a.mundo.inventario, s);
        if (n.substr(0, 2) == "$4" && (n.size() == 2 || n[2] == ' ')) return t.col<I_CANTIDAD>()[s];
    }
    return 0;
}

namespace {

// `window_type` is the type of the original list window (0x4d1000); 0 picks the one of `tipo`.
void abrir_tienda(App& a, ObjId npc, TipoTienda tipo, u8 window_type = 0) {
    static constexpr u8 TYPES[] = {0, 0, 1, 3, 2, 0xf, 0x0b, 0xd};  // by TipoTienda
    a.tienda = tipo;
    a.tienda_tipo = window_type ? window_type : TYPES[static_cast<int>(tipo)];
    a.tienda_marcada = a.tienda_flecha = -1;
    a.tienda_npc = npc;
    a.tienda_filas.clear();
    a.tienda_desplazamiento = 0;
    if (tipo == TipoTienda::DEPOSITO) {  // del propio inventario, sin lo equipado
        const TablaItems& t = a.mundo.inventario.items;
        for (Slot s = 0; s < t.tamano(); ++s) {
            std::string_view n = nombre_item(a.mundo.inventario, s);
            if (item_equipado(n)) continue;
            FilaTienda f;
            f.id = t.ids()[s];
            f.gfx = t.col<I_GFX>()[s];
            f.maximo = t.col<I_CANTIDAD>()[s];
            f.bendicion = t.col<I_BENDICION>()[s];
            f.nombre = n;
            a.tienda_filas.push_back(std::move(f));
        }
    }
}

}  // namespace

// Un vínculo de la página: otra página local, el almacén o una acción para el servidor.
void activar(App& a, std::string_view vinculo, bool es_enlace) {
    if (vinculo.empty()) return;
    if (es_enlace) {
        abrir_pagina(a, a.npc, vinculo, a.npc_datos);
    } else if (vinculo == "buyspell") {
        enviar(a, cmd::pedir_magia(a.op, a.npc));
    } else if (vinculo == "fix") {  // herrero: la lista de armas dañadas (S_FixWeaponList)
        enviar(a, cmd::armas_a_reparar(a.op));
    } else if (vinculo == "deposit") {
        abrir_tienda(a, a.npc, TipoTienda::DEPOSITO);
    } else if (vinculo == "deposit-pledge") {  // clan warehouse (window type 4, C_Result type 4)
        abrir_tienda(a, a.npc, TipoTienda::DEPOSITO, 4);
    } else if (vinculo == "deposit-elven") {  // elf warehouse (window type 8)
        abrir_tienda(a, a.npc, TipoTienda::DEPOSITO, 8);
    } else if (vinculo.substr(0, 6) == "raise-") {  // RaiseAttr: subir un atributo (C_Attr 479)
        enviar(a, cmd::subir_atributo(a.op, vinculo.substr(6)));
        cerrar(a);
    } else if (vinculo == "asktime") {  // chambelán: la fecha del próximo asedio (castillos_ui.cpp)
        enviar(a, cmd::fecha_asedio(a.op));
    } else if (a.npc_casa && (vinculo == "map" || vinculo == "apply")) {  // tablón de subastas: con la casa
        enviar(a, cmd::accion_npc(a.op, a.npc, vinculo, std::to_string(a.npc_casa)));
    } else {
        enviar(a, cmd::accion_npc(a.op, a.npc, vinculo));
    }
}

namespace {

// Total de la selección: lo que se paga (compra, retiro) o se cobra (venta).
u64 total(const App& a) {
    u64 t = 0;
    for (const FilaTienda& f : a.tienda_filas) {
        if (a.tienda == TipoTienda::RETIRO) t += f.elegido ? PRECIO_RETIRO : 0;
        else t += static_cast<u64>(f.precio) * f.elegido;
    }
    return t;
}

void confirmar(App& a) {
    if (a.tienda == TipoTienda::MASCOTA || a.tienda == TipoTienda::REPARAR) {
        cerrar(a);
        return;
    }
    if (a.tienda == TipoTienda::MAGIA) {
        u32 indices[64];
        size_t n = 0;
        for (const FilaTienda& f : a.tienda_filas)
            if (f.elegido && n < 64) indices[n++] = f.id;
        if (n) enviar(a, cmd::aprender_magia(a.op, {indices, n}));
        cerrar(a);
        return;
    }
    LineaResultado lineas[255];
    size_t n = 0;
    for (const FilaTienda& f : a.tienda_filas)
        if (f.elegido && n < 255) lineas[n++] = {f.id, f.elegido};
    // The window type is the C_Result type (0 buy, 1 sell, 2 / 4 / 8 deposits, 3 / 5 / 9 retrievals).
    if (n) enviar(a, cmd::resultado(a.op, a.tienda_npc, static_cast<TipoResultado>(a.tienda_tipo), {lineas, n}));
    cerrar(a);
}

size_t nombre_fila(const App& a, const FilaTienda& f, char* buf, size_t cap) {
    return expandir_desc(a.desc, f.nombre, buf, cap);
}

int distancia_npc(const App& a) {
    const TablaObjetos& o = a.mundo.objetos;
    Slot yo = o.buscar(a.mundo.jugador.id), n = o.buscar(a.tienda != TipoTienda::NINGUNA ? a.tienda_npc : a.npc);
    if (yo == SIN_SLOT || n == SIN_SLOT) return 1 << 20;
    return std::max(std::abs(o.col<O_X>()[yo] - o.col<O_X>()[n]), std::abs(o.col<O_Y>()[yo] - o.col<O_Y>()[n]));
}

void dibujar_pagina(App& a, int x0, int y0) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const PaginaHtml& p = a.npc_html;
    const bool simple = a.cfg.ui_simple;
    const bool orig_text = original_text(a);
    const int left = simple ? MARGEN : NPC_TEXT_X + (orig_text ? HTML_INSET_L : 0);
    const int top = simple ? ARRIBA : NPC_TEXT_Y + (orig_text ? HTML_INSET_T : 0);
    const int view_h = simple ? ALTO_PAGINA : NPC_TEXT_H - (orig_text ? HTML_INSET_T + HTML_INSET_B : 0);
    const int max_desp = std::max(0, p.alto - view_h);
    if (!simple) {  // the scroll bar works in text lines
        if (draw_original_frame(a, x0, y0, IMG_NPC_BG, 8, 11, IMG_NPC_FRAME, NPC_CLOSE_X, NPC_CLOSE_Y)) {
            cerrar(a);
            return;
        }
        draw_original_overlay(a, x0 + 2, y0 + 11, IMG_NPC_ORNAMENT);
        u32 line = static_cast<u32>(std::clamp(a.npc_desplazamiento, 0, max_desp) / NPC_LINE);
        const u32 prev = line;
        original_scrollbar(a, a.scroll.tienda, x0 + NPC_SCROLL_X, y0 + NPC_SCROLL_Y, NPC_SCROLL_H,
                           static_cast<u32>((p.alto + NPC_LINE - 1) / NPC_LINE), static_cast<u32>(view_h / NPC_LINE), line, false,
                           IMG_NPC_TRACK, -2);
        if (line != prev) a.npc_desplazamiento = static_cast<int>(line) * NPC_LINE;
    }
    if (sobre_npc(a) && e.rueda) a.npc_desplazamiento = std::clamp(a.npc_desplazamiento - e.rueda * 30, 0, max_desp);
    a.npc_desplazamiento = std::clamp(a.npc_desplazamiento, 0, max_desp);
    const int ox = x0 + left, oy = y0 + top - a.npc_desplazamiento;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 12), ki = clave_orden(CAPA_UI, 13);
    const ElementoHtml* bajo = nullptr;
    // Original client: every word of the link under the pointer turns green, not only the one pointed at (0x43d2a2
    // compares the link index of each run with the hovered one).
    int sobre_vinculo = -1;
    if (orig_text) {
        for (const ElementoHtml& el : p.elementos)
            if (el.vinculo_len && ui::sobre(e, ox + el.x, oy + el.y - 1, el.w, el.h + 2)) sobre_vinculo = static_cast<int>(el.vinculo_ofs);
    }
    for (const ElementoHtml& el : p.elementos) {
        const int x = ox + el.x, y = oy + el.y;
        // Fuera de la vista (por la primera línea de texto: un botón un poco más alto que el resto
        // de su línea no se pierde).
        if (y < y0 + top - 2 || y + (orig_text ? ALTO_LINEA_HTML_ORIGINAL : ALTO_GLIFO) > y0 + top + view_h + 2) continue;
        const bool encima = el.vinculo_len && ui::sobre(e, x, y - 1, el.w, el.h + 2);
        if (encima) bajo = &el;
        if (el.tipo == ElementoHtml::Tipo::IMAGEN) {
            // Buttons: N.img, and with the pointer over it N+1 only when it is the same size. N+1 is
            // not always the highlighted variant (for "back" it is an unrelated elf sprite).
            const RegionAtlas base = g.img(el.imagen);  // copy: loading N+1 may move the cache
            const RegionAtlas hover = encima ? g.img(el.imagen + 1) : base;
            const bool use_hover = hover.valida() && hover.w == base.w && hover.h == base.h;
            g.gpu().dibujar(use_hover ? hover : base, x, y, ki);
        } else {
            if (orig_text) {  // no underline: the client never sets the underline flag of the run style
                const bool verde = el.vinculo_len && static_cast<int>(el.vinculo_ofs) == sobre_vinculo;
                g.texto_html(p.texto(el), static_cast<float>(x), static_cast<float>(y), kt, verde ? COLOR_SOBRE_HTML_ORIGINAL : el.color);
            } else {
                g.texto(p.texto(el), static_cast<float>(x), static_cast<float>(y), kt, encima ? rgba(255, 255, 254) : el.color);
                if (el.vinculo_len) g.gpu().rectangulo(x, y + ALTO_GLIFO, el.w, 1, ki, el.color);
            }
        }
    }
    if (simple && max_desp > 0) {  // barra de desplazamiento
        int h = ALTO_PAGINA * ALTO_PAGINA / p.alto, yb = y0 + ARRIBA + (ALTO_PAGINA - h) * a.npc_desplazamiento / max_desp;
        g.gpu().rectangulo(x0 + ui::VENTANA_ANCHO - 8, yb, 3, h, ki, rgba(160, 130, 80, 200));
    }
    if (bajo) {
        std::string_view ayuda = p.ayuda(*bajo);
        if (!ayuda.empty()) {
            int w = ancho_texto(ayuda.data(), ayuda.size());
            g.gpu().rectangulo(e.mx + 10, e.my + 14, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
            g.texto(ayuda, static_cast<float>(e.mx + 14), static_cast<float>(e.my + 17), clave_orden(CAPA_UI_TEXTO, 30));
        }
        if (e.clic) activar(a, p.vinculo(*bajo), bajo->es_enlace);
    }
}

// Click handling of a shop row, shared by the simple list and the original grid: repair, pet bag, the amount
// dialog for stacks, and plain +1 / -1 selection. `hover` is whether the pointer is over the row.
void row_click(App& a, int i, bool hover, std::string_view name) {
    const EntradaFrame& e = a.entrada;
    FilaTienda& f = a.tienda_filas[static_cast<size_t>(i)];
    const bool encima = hover;
    if (encima && e.clic && a.tienda == TipoTienda::REPARAR && f.precio) {
        // Reparar esta arma (C_SelectList; el servidor cobra y la deja sin daño).
        enviar(a, cmd::reparar_arma(a.op, f.id, a.tienda_npc));
        f.precio = 0;
        f.nombre += " (repaired)";
        return;
    }
    if (encima && e.clic && a.tienda == TipoTienda::MASCOTA) {
        // Usar o equipar el ítem de la mascota (por su fila) y volver a pedir la lista.
        enviar(a, cmd::usar_item_mascota(a.op, a.tienda_npc, static_cast<u8>(i)));
        enviar(a, cmd::inventario_mascota(a.op, a.tienda_npc));
        return;
    }
    if (encima && e.clic && !e.shift && f.maximo > 1 && a.tienda != TipoTienda::MAGIA) {
        // De una pila: el diálogo de cantidad (se escribe el número; 0 la quita de la lista).
        DialogoCantidad& d = a.cantidad;
        d = {};
        d.abierto = true;
        d.tienda = CANTIDAD_FILA_NPC;
        d.objeto = a.tienda_npc;
        d.item = static_cast<ObjId>(i);
        d.titulo.assign(name);
        d.accion = "How many? (0: none)";
        d.min = 0;
        d.max = static_cast<i32>(f.maximo);
        d.valor = f.elegido ? static_cast<i32>(f.elegido) : 1;
        if (a.tienda == TipoTienda::COMPRA || a.tienda == TipoTienda::VENTA) d.precio = static_cast<i32>(f.precio);
        a.entrada.clic = false;  // que este clic no llegue a los botones del diálogo
        return;
    }
    if (encima && e.clic) f.elegido = std::min(f.maximo, f.elegido + (e.shift ? (f.maximo > 10 ? 10u : f.maximo) : 1u));
    if (encima && e.clic_derecho) f.elegido = e.shift ? 0 : (f.elegido ? f.elegido - 1 : 0);
}


// Original list window of the 2009 US client (S3EP1U): the NPC shop (buy and sell), the warehouses, the pet bag, the
// spell list and the weapon repair list are all one window class (0x4f55d8, ctor 0x4bef40, draw 0x4bf690) whose
// type (0x4d1000) only changes the button art and the texts. The earlier 4x2-grid window (frame 1284, ctor 0x42dc30)
// is the private shop ("Selling item" / "Buying item"), not the NPC shop.
//  - background 1099 at (8,11), frame 1100, ornaments 1101 at (2,11), close (225,15), label "Adena:" 1106 at (29,31),
//    its number at (80,31);
//  - pane (24,44)-(225,304): 7 rows of 36 px from (25,51) (0x4be220); scroll bar 1354 (up 974 at (225,47), track at y 55,
//    down 972 at (226,293), thumb 976);
//  - row (0x4bd250): icon at the row origin, name at (+38,+3), amount chosen right-aligned to +176 at y +19 between the
//    arrows 556 (left) and 554 (right, at +180), the price left of the left arrow with the adena coin 325;
//  - buttons at y 306: Buy 1109 / Sell 1107 / OK 1114 at x 79, Cancel 1111 at x 131 (pressed art = id + 1).
constexpr u32 IMG_LIST_BG = 1099, IMG_LIST_FRAME = 1100, IMG_LIST_ORNAMENT = 1101, IMG_LIST_LABEL = 1106, IMG_LIST_TRACK = 1354;
constexpr u32 IMG_BUY = 1109, IMG_SELL = 1107, IMG_OK = 1114, IMG_CANCEL = 1111, IMG_COIN = 325, IMG_ROW_MARK = 536;
constexpr u32 IMG_ARROW_RIGHT = 554, IMG_ARROW_LEFT = 556;  // +1: pressed
constexpr int LIST_CLOSE_X = 225, LIST_CLOSE_Y = 15, LIST_ROWS = 7, LIST_ROW_H = 36, LIST_ROW_X = 25, LIST_ROW_Y = 51;
constexpr int LIST_PANE_X = 24, LIST_PANE_Y = 44, LIST_PANE_W = 201, LIST_PANE_H = 260;
constexpr int LIST_SCROLL_X = 225, LIST_SCROLL_Y = 47, LIST_SCROLL_H = 258;
constexpr int LIST_BTN_Y = 306, LIST_BTN_X = 79, LIST_CANCEL_X = 131;
constexpr int ARROW_SIDE = 12;  // clickable side of a quantity arrow (0x4be0..: rect of 12 x 12)

// Text colours 0x8f0f34[]: 0 blessed (255,246,180), 1 normal white, 2 cursed red, 3 unidentified grey; set at 0x4d16bc.
u32 state_color(u8 state) {
    static constexpr u16 PALETTE[] = {0x7fd6, 0x7fff, 0x5ca7, 0x5ef7};
    return rgb555(PALETTE[state & 3]);
}

// Original text of a number: thousands separated (0x44f4f0).
const char* digits(u64 v, char* buf, size_t n) { return con_miles(v, buf, n); }

// Quantity arrows, as 0x4bd840 (left) and 0x4bd8a0 (right): the amount wraps between 0 and the stack size (a row
// of a shop has no stack limit but 1000 per click series, the type 0 cap pushed at 0x4bddd1), and a price of
// 10 000 000 or more allows one only.
void step_amount(const App& a, FilaTienda& f, bool increase) {
    const bool buying = a.tienda == TipoTienda::COMPRA;
    const u32 stack = buying ? 0u : f.maximo, cap = buying ? 1000u : 0u;
    u32 n = f.elegido;
    if (increase) {
        ++n;
        if (stack && n > stack) n = 0;
        if (cap && n > cap) n = n == cap + 1 ? 0 : cap;
    } else if (n) {
        --n;
    } else if (stack) {
        n = stack;
    }
    if (cap && n > cap) n = cap;
    if (f.precio >= 10000000u && n > 1) n = increase ? 0 : 1;
    if (buying && f.precio >= 1000000u && n > 10) n = 10;
    f.elegido = n;
}

// The two-tone frame and the 50 % darkening of the list tooltip (0x4be41e..0x4be576): 142 px wide at the pane
// x + 10 (the y thresholds are screen coordinates), name lines at +5 / +5 + 12 k.
void draw_list_tooltip(App& a, int x0, int row_y, const char* line1, const char* line2, u32 color) {
    Graficos& g = *a.g;
    const int lines = line2[0] ? 2 : 1;
    const int h = 12 * lines + 10, w = 142, x = x0 + 34;
    int top = row_y + 32 > 300 ? row_y - 12 : row_y + 32;
    if (top > 157) top = std::max(10, top - 12 * (lines - 1) - 22);
    else top = std::min(top - 33, 330 - h);
    const u64 kb = clave_orden(CAPA_UI, 30), kl = clave_orden(CAPA_UI, 31), kt = clave_orden(CAPA_UI_TEXTO, 30);
    g.gpu().rectangulo(x, top, w, h, kb, rgba(0, 0, 0, 128));
    const u32 light = rgba(189, 189, 189), dark = rgba(123, 123, 123);
    g.gpu().rectangulo(x, top, w, 1, kl, light);
    g.gpu().rectangulo(x + 1, top + 1, w - 2, 1, kl, light);
    g.gpu().rectangulo(x, top, 1, h, kl, light);
    g.gpu().rectangulo(x + 1, top + 1, 1, h - 2, kl, light);
    g.gpu().rectangulo(x, top + h - 1, w, 1, kl, dark);
    g.gpu().rectangulo(x + 1, top + h - 2, w - 2, 1, kl, dark);
    g.gpu().rectangulo(x + w - 1, top, 1, h, kl, dark);
    g.gpu().rectangulo(x + w - 2, top + 1, 1, h - 2, kl, dark);
    g.texto_sombra(line1, static_cast<float>(x + 5), static_cast<float>(top + 5), kt, color);
    if (line2[0]) g.texto_sombra(line2, static_cast<float>(x + 5), static_cast<float>(top + 17), kt, color);
}

// A name as one or two lines: the second one starts at the last space that keeps the first within `width`.
void split_name(std::string_view name, int width, char* l1, char* l2, size_t cap) {
    size_t cut = name.size();
    while (cut > 0 && ancho_texto(name.data(), cut) > width) {
        const size_t sp = name.rfind(' ', cut - 1);
        cut = sp == std::string_view::npos || sp == 0 ? cut - 1 : sp;
    }
    const size_t n1 = std::min(cut, cap - 1);
    std::memcpy(l1, name.data(), n1);
    l1[n1] = 0;
    size_t start = cut;
    while (start < name.size() && name[start] == ' ') ++start;
    const size_t n2 = std::min(name.size() - start, cap - 1);
    std::memcpy(l2, name.data() + start, n2);
    l2[n2] = 0;
}

void draw_list_original(App& a, int x0, int y0) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 14), ki = clave_orden(CAPA_UI, 13), km = clave_orden(CAPA_UI, 12), kc = clave_orden(CAPA_UI, 16);
    if (draw_original_frame(a, x0, y0, IMG_LIST_BG, 8, 11, IMG_LIST_FRAME, LIST_CLOSE_X, LIST_CLOSE_Y)) {
        cerrar(a);
        return;
    }
    draw_original_overlay(a, x0 + 2, y0 + 11, IMG_LIST_ORNAMENT);
    const bool free_pointer = !sobre_cantidad(a);
    const int n = static_cast<int>(a.tienda_filas.size());
    const bool pane_hover = ui::sobre(e, x0 + LIST_PANE_X, y0 + LIST_PANE_Y, LIST_PANE_W, LIST_PANE_H) && free_pointer;
    original_scrollbar(a, a.scroll.tienda, x0 + LIST_SCROLL_X, y0 + LIST_SCROLL_Y, LIST_SCROLL_H, static_cast<u32>(n), LIST_ROWS,
                       a.tienda_desplazamiento, pane_hover, IMG_LIST_TRACK, -2);
    char b1[40], b2[40], b3[40];
    // Totals (0x4be800 sums price x amount, 0x4be830 counts the rows with an amount).
    u64 total_price = 0;
    u32 chosen_rows = 0;
    for (const FilaTienda& f : a.tienda_filas) {
        total_price += static_cast<u64>(f.precio) * f.elegido;
        chosen_rows += f.elegido ? 1u : 0u;
    }
    const u64 adena = adena_inventario(a);
    const bool fee = a.tienda == TipoTienda::RETIRO;
    const u64 amount = fee ? static_cast<u64>(chosen_rows) * PRECIO_RETIRO : total_price;
    // Rows.
    const bool show_price = true;  // 0x4bd250 flag [+0x95]: set by the list constructor (0x4bd980), cleared only for type 0x10
    int hover_row = -1;
    bool hover_arrow = false;
    char tip1[160] = "", tip2[160] = "";
    u32 tip_color = state_color(1);
    int tip_y = 0;
    const int first = static_cast<int>(a.tienda_desplazamiento);
    for (int k = 0; k < LIST_ROWS; ++k) {
        const int idx = first + k;
        if (idx >= n) break;
        FilaTienda& f = a.tienda_filas[static_cast<size_t>(idx)];
        const int rx = x0 + LIST_ROW_X, ry = y0 + LIST_ROW_Y + k * LIST_ROW_H;
        const u32 color = state_color(f.bendicion);
        digits(f.elegido, b1, sizeof b1);
        const int num_w = ancho_texto(b1, std::strlen(b1));
        const int num_x = rx + 180 - num_w - 4, ay = ry + 19;
        const int left_x = num_x - 13, right_x = rx + 180;
        const bool over_row = ui::sobre(e, x0 + LIST_PANE_X, ry - 2, LIST_PANE_W, LIST_ROW_H) && free_pointer;
        const bool over_left = ui::sobre(e, left_x, ay, ARROW_SIDE, ARROW_SIDE) && free_pointer;
        const bool over_right = ui::sobre(e, right_x, ay, ARROW_SIDE, ARROW_SIDE) && free_pointer;
        if (f.elegido) g.gpu().rectangulo(x0 + 24, ry - 2, 192, 34, km, rgba(120, 140, 255, 70));  // 0x498d90 with table 0x8f133c
        if (idx == a.tienda_marcada) g.gpu().dibujar(g.img(IMG_ROW_MARK), rx - 2, ry - 2, km);
        draw_item_icon(g.gpu(), g.icono(f.gfx), rx, ry, ki, f.bendicion);
        char name[256];
        const size_t m = nombre_fila(a, f, name, sizeof name);
        char l1[160], l2[160];
        split_name({name, m}, 140, l1, l2, sizeof l1);
        g.texto_sombra(l1, static_cast<float>(rx + 38), static_cast<float>(ry + 3), kt, color);
        // The second line shares its height with the price: it stops before the coin (16 characters at most, 0x4bd808).
        size_t n2 = std::strlen(l2);
        const int limit = (show_price ? left_x - 4 - ancho_texto(digits(f.precio, b2, sizeof b2), std::strlen(b2)) - 4 - 13 : left_x) - (rx + 38);
        while (n2 > 0 && ancho_texto(l2, n2) > limit) --n2;
        l2[n2] = 0;
        if (l2[0]) g.texto_sombra(l2, static_cast<float>(rx + 38), static_cast<float>(ry + 17), kt, color);
        // Amount and arrows.
        g.texto_sombra(b1, static_cast<float>(num_x), static_cast<float>(ay), kt, color);
        const bool held_left = a.tienda_flecha == idx * 2 && e.presionado && over_left;
        const bool held_right = a.tienda_flecha == idx * 2 + 1 && e.presionado && over_right;
        g.gpu().dibujar(g.img(IMG_ARROW_LEFT + (held_left ? 1 : 0)), left_x, ay, kc);
        g.gpu().dibujar(g.img(IMG_ARROW_RIGHT + (held_right ? 1 : 0)), right_x, ay, kc);
        if (show_price) {
            const u64 price = f.precio;
            digits(price, b2, sizeof b2);
            const int price_w = ancho_texto(b2, std::strlen(b2));
            const int price_x = left_x - price_w - 4;
            u32 price_color = color;
            if (a.tienda == TipoTienda::REPARAR) {  // types 0xd / 0xe colour the price by magnitude (0x4bd637)
                static constexpr u16 STEPS[] = {0x7fd6, 0x63b6, 0x5a36, 0x5fbc, 0x509e, 0x5e8d};
                static constexpr u64 LIMITS[] = {10000, 100000, 1000000, 10000000, 100000000};
                int s = 0;
                while (s < 5 && price >= LIMITS[s]) ++s;
                price_color = s == 0 ? color : rgb555(STEPS[s]);
            }
            g.texto_sombra(b2, static_cast<float>(price_x), static_cast<float>(ay), kt, price_color);
            g.gpu().dibujar(g.img(IMG_COIN), price_x - 13, ay, kc);
        }
        // Pointer: the icon column shows the tooltip; an arrow press starts a held arrow; the row press marks it.
        if (over_row && e.pulsado) a.tienda_marcada = idx;
        if (e.pulsado && over_left) a.tienda_flecha = idx * 2;
        if (e.pulsado && over_right) a.tienda_flecha = idx * 2 + 1;
        // The release over the arrow acts (0x4bdd9d); a click injected as release only (no press seen) counts too.
        if (e.clic && (a.tienda_flecha == idx * 2 || a.tienda_flecha < 0) && over_left) step_amount(a, f, false);
        if (e.clic && (a.tienda_flecha == idx * 2 + 1 || a.tienda_flecha < 0) && over_right) step_amount(a, f, true);
        if (over_left || over_right) hover_arrow = true;
        if (free_pointer && ui::sobre(e, rx, ry, LIST_ROW_H, LIST_ROW_H)) {
            hover_row = idx;
            std::snprintf(tip1, sizeof tip1, "%s", l1);
            std::snprintf(tip2, sizeof tip2, "%s", l2);
            tip_color = color;
            tip_y = ry;
        }
        // The pet bag and the repair list act on a plain click of the row (fork flow: the arrows do not apply to them).
        if (over_row && !over_left && !over_right && e.clic && (a.tienda == TipoTienda::MASCOTA || a.tienda == TipoTienda::REPARAR))
            row_click(a, idx, true, {name, m});
    }
    if (e.clic || !e.presionado) a.tienda_flecha = -1;
    // Adena label and the amount (0x4bf8e0..0x4bfc5d): "total/adena", red when the adena does not cover it.
    {
        g.gpu().dibujar(g.img(IMG_LIST_LABEL), x0 + 29, y0 + 31, kc);
        digits(amount, b1, sizeof b1);
        digits(adena, b2, sizeof b2);
        const bool pair = a.tienda == TipoTienda::COMPRA || a.tienda == TipoTienda::MAGIA || a.tienda == TipoTienda::REPARAR || fee;
        std::snprintf(b3, sizeof b3, pair ? "%s/%s" : "%s", b1, b2);
        g.texto_sombra(b3, static_cast<float>(x0 + 80), static_cast<float>(y0 + 31), kt, state_color(pair && amount > adena ? 2 : 0));
    }
    // Warehouse capacity: "%d/150" personal (type 3), "/200" clan (5), "/100" elf (9), at (193,309) (0x4bfc78).
    if (a.tienda == TipoTienda::RETIRO && (a.tienda_tipo == 3 || a.tienda_tipo == 5 || a.tienda_tipo == 9)) {
        std::snprintf(b3, sizeof b3, "%d/%d", n, a.tienda_tipo == 3 ? 150 : a.tienda_tipo == 5 ? 200 : 100);
        g.texto_sombra(b3, static_cast<float>(x0 + 193), static_cast<float>(y0 + 309), kt, state_color(0));
    }
    if (hover_row >= 0 && !hover_arrow && tip1[0]) draw_list_tooltip(a, x0, tip_y, tip1, tip2, tip_color);
    // Buttons: pressed art while the button is held (0x47c2d0 buttons have no hover art).
    auto button = [&](int x, u32 image) {
        const RegionAtlas base = g.img(image);
        const bool over = ui::sobre(e, x0 + x, y0 + LIST_BTN_Y, base.w, base.h) && free_pointer;
        g.gpu().dibujar(over && e.presionado ? g.img(image + 1) : base, x0 + x, y0 + LIST_BTN_Y, kc);
        return over && e.clic;
    };
    const u32 confirm_image = a.tienda == TipoTienda::COMPRA || a.tienda == TipoTienda::MAGIA ? IMG_BUY
                              : a.tienda == TipoTienda::VENTA                                  ? IMG_SELL
                                                                                               : IMG_OK;
    if (button(LIST_BTN_X, confirm_image)) confirmar(a);
    else if (button(LIST_CANCEL_X, IMG_CANCEL)) cerrar(a);
}

void dibujar_tienda(App& a, int x0, int y0) {
    if (!a.cfg.ui_simple) return draw_list_original(a, x0, y0);
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 12), kf = clave_orden(CAPA_UI, 12), ki = clave_orden(CAPA_UI, 13);
    const int n = static_cast<int>(a.tienda_filas.size());
    barra_scroll_ventana(a, a.scroll.tienda, x0 + ui::VENTANA_ANCHO - 12, y0 + ARRIBA, FILAS_TIENDA * FILA_TIENDA,
                         static_cast<u32>(n), FILAS_TIENDA, a.tienda_desplazamiento, clave_orden(CAPA_UI, 14), sobre_npc(a));
    char buf[256];
    for (int k = 0; k < FILAS_TIENDA; ++k) {
        int i = static_cast<int>(a.tienda_desplazamiento) + k;
        if (i >= n) break;
        FilaTienda& f = a.tienda_filas[static_cast<size_t>(i)];
        const int x = x0 + MARGEN, y = y0 + ARRIBA + k * FILA_TIENDA, w = ui::VENTANA_ANCHO - 2 * MARGEN;
        const bool encima = ui::sobre(e, x, y, w, FILA_TIENDA - 2) && !sobre_cantidad(a);  // el diálogo va encima
        g.gpu().rectangulo(x, y, w, FILA_TIENDA - 2, kf, f.elegido ? rgba(80, 64, 20, 230) : encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
        draw_item_icon(g.gpu(), g.icono(f.gfx), x + 1, y, ki, f.bendicion);
        size_t m = nombre_fila(a, f, buf, sizeof buf);
        const u32 color = rgba(255, 255, 254);
        // El nombre en una línea (cortado si no entra) y debajo precio y cantidad elegida.
        while (m > 0 && ancho_texto(buf, m) > w - 40) --m;
        g.texto({buf, m}, static_cast<float>(x + 38), static_cast<float>(y + 3), kt, color);
        char linea[64], m1[32];
        if (a.tienda == TipoTienda::DEPOSITO || a.tienda == TipoTienda::RETIRO || a.tienda == TipoTienda::MASCOTA)
            std::snprintf(linea, sizeof linea, "x%u", f.maximo);
        else if (a.tienda == TipoTienda::REPARAR)
            std::snprintf(linea, sizeof linea, "Damage %u: %s adena", f.maximo, con_miles(f.precio, m1, sizeof m1));
        else
            std::snprintf(linea, sizeof linea, "%s adena", con_miles(f.precio, m1, sizeof m1));
        g.texto(linea, static_cast<float>(x + 38), static_cast<float>(y + 17), kt, rgba(170, 160, 140));
        if (f.elegido) {
            std::snprintf(linea, sizeof linea, "%u", f.elegido);
            g.texto(linea, static_cast<float>(x + w - 6 - ancho_texto(linea, std::strlen(linea))), static_cast<float>(y + 17), kt,
                    rgba(255, 230, 120));
        }
        row_click(a, i, encima, {buf, m});
    }
    const int yp = y0 + ARRIBA + FILAS_TIENDA * FILA_TIENDA + 4;
    char pie[96], m1[32], m2[32];
    const u64 t = total(a);
    switch (a.tienda) {
    case TipoTienda::MAGIA:
        if (a.tienda_filas.empty()) {
            std::snprintf(pie, sizeof pie, "Nothing to learn now");
            break;
        }
        [[fallthrough]];
    case TipoTienda::COMPRA: std::snprintf(pie, sizeof pie, "Total %s / %s adena", con_miles(t, m1, sizeof m1), con_miles(adena_inventario(a), m2, sizeof m2)); break;
    case TipoTienda::VENTA: std::snprintf(pie, sizeof pie, "You get %s adena", con_miles(t, m1, sizeof m1)); break;
    case TipoTienda::RETIRO: std::snprintf(pie, sizeof pie, "Fee %s adena (30 each)", con_miles(t, m1, sizeof m1)); break;
    case TipoTienda::MASCOTA: std::snprintf(pie, sizeof pie, a.tienda_filas.empty() ? "The pet carries nothing" : "Click: use or equip"); break;
    case TipoTienda::REPARAR: std::snprintf(pie, sizeof pie, a.tienda_filas.empty() ? "No damaged weapons" : "Click a weapon to repair it"); break;
    default: std::snprintf(pie, sizeof pie, "Click: amount  Shift: +10  Right: -1"); break;
    }
    g.texto(pie, static_cast<float>(x0 + MARGEN), static_cast<float>(yp), kt, rgba(214, 190, 140));
    if (ui::boton_texto(g, e, "OK", x0 + MARGEN, yp + 18, 90, 18)) confirmar(a);
    if (ui::boton_texto(g, e, "Cancel", x0 + ui::VENTANA_ANCHO - MARGEN - 90, yp + 18, 90, 18)) cerrar(a);
}

}  // namespace

u8 tipo_objeto(const App& a, Slot s) {
    const GfxId gfx = a.mundo.objetos.col<O_GFX>()[s];
    const u8 tipo = a.sprites.tipo(gfx);
    const bool sin_tabla = tipo == 0 && !a.sprites.accion(gfx, ACC_QUIETO) && !a.sprites.accion(gfx, ACC_CAMINAR);
    // Otro jugador transformado o con un gfx que esta tabla no conoce: sigue siendo un personaje. Los
    // jugadores traen su nombre tal cual y nivel 0; monstruos y NPCs, "$N" o su nivel.
    if (sin_tabla || tipo == TIPO_MONSTRUO) {
        const TablaObjetos& o = a.mundo.objetos;
        const std::string_view crudo = texto(a.mundo, o.col<O_NOMBRE>()[s]);
        if (!crudo.empty() && crudo[0] != '$' && crudo.find('^') == std::string_view::npos && o.col<O_NIVEL>()[s] == 0 &&
            !o.col<O_MASCOTA>()[s])
            return TIPO_PERSONAJE;
    }
    // Sin el gfx en la tabla de sprites (se dibuja como un bloque magenta): un monstruo de una
    // versión más nueva que esta instalación, así se lo puede atacar; salvo los NPCs de los
    // pueblos, que se llaman "Oficio^Nombre" ("Storage^Dorin") y a los que se les habla.
    if (sin_tabla && s != a.mundo.objetos.buscar(a.mundo.jugador.id) &&
        a.desc.resolver(texto(a.mundo, a.mundo.objetos.col<O_NOMBRE>()[s])).find('^') == std::string_view::npos)
        return TIPO_MONSTRUO;
    // Asedio: los portones son puertas; la torre de guardia y la corona se atacan con un clic (la
    // corona se toma "atacándola": C_Attack y el servidor llama a su onAction).
    if (tipo == TIPO_ESTRUCTURA) return a.sprites.accion(gfx, ACC_ABRIR) ? TIPO_PUERTA : u8{TIPO_MONSTRUO};
    if (gfx == GFX_CORONA) return TIPO_MONSTRUO;
    return tipo;
}

bool es_npc(const App& a, Slot s) {
    u8 tipo = tipo_objeto(a, s);
    return tipo != TIPO_MONSTRUO && tipo != TIPO_ITEM && tipo != TIPO_PERSONAJE && s != a.mundo.objetos.buscar(a.mundo.jugador.id);
}

void eventos_npc(App& a, const Evento& e) {
    switch (e.tipo) {
    case Ev::HTML:
        if (e.s[0] == "monlist") break;  // Shape Change: forma_ui.cpp
        if (e.s[0].empty()) {
            if (a.npc == e.id) cerrar(a);
            break;
        }
        abrir_pagina(a, e.id, e.s[0], e.s[1]);
        a.npc_casa = static_cast<u32>(e.v[HT_CASA]);
        break;
    case Ev::TELETRANSPORTE:
        if (e.id == a.mundo.jugador.id) {
            enviar(a, cmd::teletransporte(a.op));
            cerrar(a);
        }
        break;
    case Ev::LISTA_TIENDA: {
        static constexpr TipoTienda TIPOS[] = {TipoTienda::COMPRA, TipoTienda::VENTA, TipoTienda::RETIRO, TipoTienda::MASCOTA,
                                               TipoTienda::REPARAR};
        static_assert(std::size(TIPOS) == LISTA_REPARAR + 1);
        if (a.npc == 0) a.npc = e.id;
        // La de reparar no trae el NPC: es el herrero con el que se habla.
        const TipoTienda tipo = TIPOS[e.v[LT_TIPO]];
        abrir_tienda(a, e.v[LT_TIPO] == LISTA_REPARAR ? a.npc : e.id, tipo,
                     tipo == TipoTienda::RETIRO && e.v[LT_ALMACEN] ? static_cast<u8>(e.v[LT_ALMACEN]) : u8{0});
        break;
    }
    case Ev::MAGIA_EN_VENTA: {
        // Precio del servidor (C_SkillBuyOK): 100 adena el nivel 1, 400 el 2 y 900 el 3 (k / 8 = nivel - 1).
        abrir_tienda(a, a.npc, TipoTienda::MAGIA);
        for (u32 k = 0; k < 64; ++k) {
            if (!(static_cast<u32>(e.v[MV_MASCARA + k / 32]) >> (k % 32) & 1)) continue;
            const Hechizo& h = a.hechizos[k + 1];
            FilaTienda f;
            f.id = k;
            f.gfx = h.icono;
            f.maximo = 1;
            f.bendicion = 1;
            f.precio = 100 * (k / 8 + 1) * (k / 8 + 1);
            f.nombre = h.existe() ? std::string(h.nombre) : "Spell " + std::to_string(k + 1);
            a.tienda_filas.push_back(std::move(f));
        }
        break;
    }
    case Ev::ITEM_TIENDA: {
        FilaTienda f;
        f.id = e.id;
        f.precio = static_cast<u32>(e.v[TI_PRECIO]);
        if (a.tienda == TipoTienda::VENTA || a.tienda == TipoTienda::REPARAR) {  // solo id y precio: el resto, del inventario
            Slot s = a.mundo.inventario.items.buscar(e.id);
            if (s == SIN_SLOT) break;
            f.gfx = a.mundo.inventario.items.col<I_GFX>()[s];
            f.maximo = a.tienda == TipoTienda::REPARAR ? static_cast<u32>(e.v[IT_CANTIDAD]) : a.mundo.inventario.items.col<I_CANTIDAD>()[s];
            f.bendicion = a.mundo.inventario.items.col<I_BENDICION>()[s];
            f.nombre = nombre_item(a.mundo.inventario, s);
        } else {
            f.gfx = static_cast<u16>(e.v[IT_GFX]);
            f.maximo = a.tienda == TipoTienda::COMPRA ? 9999 : static_cast<u32>(e.v[IT_CANTIDAD]);
            f.bendicion = a.tienda == TipoTienda::COMPRA ? u8{1} : static_cast<u8>(e.v[IT_BENDICION]);  // the shop sends no state: normal
            f.nombre = e.s[0];
            if (a.tienda == TipoTienda::MASCOTA && e.v[IT_IDENTIFICADO] >= 2) f.nombre += " (worn)";  // 2/3: equipado
        }
        a.tienda_filas.push_back(std::move(f));
        break;
    }
    default: break;
    }
}

void original_npc_size(const App& a, int& w, int& h) {
    const bool shop = a.tienda != TipoTienda::NINGUNA;
    w = shop ? SHOP_WINDOW_W : NPC_WINDOW_W;
    h = shop ? SHOP_WINDOW_H : NPC_WINDOW_H;
}

bool sobre_npc(const App& a) { return sobre_ventana(a, IdVentana::NPC); }

void procesar_npc(App& a, u64 ahora) {
    if ((a.npc || a.tienda != TipoTienda::NINGUNA) && distancia_npc(a) > DISTANCIA_CIERRE) cerrar(a);
}

void dibujar_npc(App& a) {
    if (!a.npc && a.tienda == TipoTienda::NINGUNA) return;
    Graficos& g = *a.g;
    char titulo[96] = "";
    if (a.tienda != TipoTienda::NINGUNA) {
        static const char* NOMBRES[] = {"", "Buy", "Sell", "Retrieve", "Deposit", "Learn magic", "Pet's bag", "Repair"};
        static_assert(std::size(NOMBRES) == static_cast<size_t>(TipoTienda::REPARAR) + 1);
        std::snprintf(titulo, sizeof titulo, "%s", NOMBRES[static_cast<int>(a.tienda)]);
    } else if (Slot s = a.mundo.objetos.buscar(a.npc); s != SIN_SLOT) {
        size_t n = expandir_desc(a.desc, texto(a.mundo, a.mundo.objetos.col<O_NOMBRE>()[s]), titulo, sizeof titulo - 1);
        titulo[n] = 0;
    }
    arrastrar_ventana(a, IdVentana::NPC);
    const Rect rv = rect_ventana(a, IdVentana::NPC);
    if (a.entrada.escape) {
        cerrar(a);
        return;
    }
    // The original shop draws its own frame; the other windows still use the generic one.
    if (!a.cfg.ui_simple) {
    } else if (ui::ventana(g, a.entrada, a.cfg.ui_simple, rv.x, rv.y, titulo, opacidad_ventanas(a))) {
        cerrar(a);
        return;
    }
    if (a.tienda != TipoTienda::NINGUNA) dibujar_tienda(a, rv.x, rv.y);
    else dibujar_pagina(a, rv.x, rv.y);
}

}  // namespace lin

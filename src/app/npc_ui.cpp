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

namespace lin {

namespace {

constexpr int MARGEN = 14, ARRIBA = 30;
constexpr int ANCHO_PAGINA = ui::VENTANA_ANCHO - 2 * MARGEN;
constexpr int ALTO_PAGINA = ui::VENTANA_ALTO - ARRIBA - 12;
// Original NPC dialog (client class at 0x43f130, draw 0x43f7f0): background 1355 at (8,11), frame 1356, ornaments
// 1101 at (2,11), close button at (226,15); scroll bar on x 226 with arrows at y 45 and 293 over the track 1354.
constexpr u32 IMG_NPC_BG = 1355, IMG_NPC_FRAME = 1356, IMG_NPC_ORNAMENT = 1101, IMG_NPC_TRACK = 1354;
constexpr int NPC_CLOSE_X = 226, NPC_CLOSE_Y = 15, NPC_TEXT_X = 18, NPC_TEXT_Y = 34, NPC_TEXT_W = 200, NPC_TEXT_H = 262;
constexpr int NPC_SCROLL_X = 226, NPC_SCROLL_Y = 45, NPC_SCROLL_H = 260, NPC_LINE = 12;  // scroll steps of a text line

int page_width(const App& a) { return a.cfg.ui_simple ? ANCHO_PAGINA : NPC_TEXT_W; }
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
                  nombre_personaje(a), page_width(a), medir_imagen, &a, a.npc_html);
}

void cerrar(App& a) {
    a.npc = 0;
    a.tienda = TipoTienda::NINGUNA;
    a.tienda_filas.clear();
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

void abrir_tienda(App& a, ObjId npc, TipoTienda tipo) {
    a.tienda = tipo;
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
    static constexpr TipoResultado TIPOS[] = {RESULTADO_COMPRA, RESULTADO_COMPRA, RESULTADO_VENTA, RESULTADO_RETIRO,
                                              RESULTADO_DEPOSITO};
    if (n) enviar(a, cmd::resultado(a.op, a.tienda_npc, TIPOS[static_cast<int>(a.tienda)], {lineas, n}));
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
    const int left = simple ? MARGEN : NPC_TEXT_X, top = simple ? ARRIBA : NPC_TEXT_Y, view_h = simple ? ALTO_PAGINA : NPC_TEXT_H;
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
                           IMG_NPC_TRACK);
        if (line != prev) a.npc_desplazamiento = static_cast<int>(line) * NPC_LINE;
    }
    if (sobre_npc(a) && e.rueda) a.npc_desplazamiento = std::clamp(a.npc_desplazamiento - e.rueda * 30, 0, max_desp);
    a.npc_desplazamiento = std::clamp(a.npc_desplazamiento, 0, max_desp);
    const int ox = x0 + left, oy = y0 + top - a.npc_desplazamiento;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 12), ki = clave_orden(CAPA_UI, 13);
    const ElementoHtml* bajo = nullptr;
    for (const ElementoHtml& el : p.elementos) {
        const int x = ox + el.x, y = oy + el.y;
        // Fuera de la vista (por la primera línea de texto: un botón un poco más alto que el resto
        // de su línea no se pierde).
        if (y < y0 + top - 2 || y + ALTO_GLIFO > y0 + top + view_h + 2) continue;
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
            g.texto(p.texto(el), static_cast<float>(x), static_cast<float>(y), kt, encima ? rgba(255, 255, 254) : el.color);
            if (el.vinculo_len) g.gpu().rectangulo(x, y + ALTO_GLIFO, el.w, 1, ki, el.color);
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


// Original shop window (client S3EP1U draw at 0x42dfd0, built at 0x42dc30): background 1283 at (7,7), frame 1284 at
// the corner, ornaments 1285; "selling" grid of 4x2 cells of 34 px at (43,51), "buying" grid at (43,145), two text
// bars at y 224 / 241, OK (1289) at (46,258) and Cancel (1286) at (109,258), close button at (174,18).
constexpr u32 IMG_SHOP_BG = 1283, IMG_SHOP_FRAME = 1284, IMG_SHOP_ORNAMENT = 1285;
constexpr u32 IMG_OK = 1289, IMG_OK_LIT = 1290, IMG_CANCEL = 1286, IMG_CANCEL_LIT = 1287;
constexpr int SHOP_CLOSE_X = 174, SHOP_CLOSE_Y = 18, SHOP_CELL = 34, SHOP_COLS = 4, SHOP_ROWS = 2;
constexpr int SHOP_GRID_X = 43, SHOP_SELL_Y = 51, SHOP_BUY_Y = 145, SHOP_SCROLL_X = 28;
constexpr int SHOP_BAR_X = 28, SHOP_BAR1_Y = 224, SHOP_BAR2_Y = 241;

// Rows of the shop that are part of the selection, as indices (in order). Returns how many fit in `out`.
size_t chosen_rows(const App& a, int* out, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; i < a.tienda_filas.size() && n < cap; ++i)
        if (a.tienda_filas[i].elegido) out[n++] = static_cast<int>(i);
    return n;
}

void draw_shop_original(App& a, int x0, int y0) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 14), ki = clave_orden(CAPA_UI, 13), kh = clave_orden(CAPA_UI, 14);
    if (draw_original_frame(a, x0, y0, IMG_SHOP_BG, 7, 7, IMG_SHOP_FRAME, SHOP_CLOSE_X, SHOP_CLOSE_Y)) {
        cerrar(a);
        return;
    }
    draw_original_overlay(a, x0, y0, IMG_SHOP_ORNAMENT);
    const bool free_pointer = !sobre_cantidad(a);  // the amount dialog sits on top
    const int n = static_cast<int>(a.tienda_filas.size());
    int chosen[256];
    const int n_chosen = static_cast<int>(chosen_rows(a, chosen, std::size(chosen)));
    // Scroll bars in the dark column left of each grid, one row (4 items) per step.
    u32& chosen_pos = a.scroll.tienda_elegidas_pos;
    const u32 sell_rows = static_cast<u32>((n + SHOP_COLS - 1) / SHOP_COLS), buy_rows = static_cast<u32>((n_chosen + SHOP_COLS - 1) / SHOP_COLS);
    original_scrollbar(a, a.scroll.tienda, x0 + SHOP_SCROLL_X, y0 + SHOP_SELL_Y, SHOP_ROWS * SHOP_CELL, sell_rows, SHOP_ROWS,
                       a.tienda_desplazamiento, ui::sobre(e, x0 + SHOP_GRID_X, y0 + SHOP_SELL_Y, SHOP_COLS * SHOP_CELL, SHOP_ROWS * SHOP_CELL) && free_pointer);
    original_scrollbar(a, a.scroll.tienda_elegidas, x0 + SHOP_SCROLL_X, y0 + SHOP_BUY_Y, SHOP_ROWS * SHOP_CELL, buy_rows, SHOP_ROWS,
                       chosen_pos, ui::sobre(e, x0 + SHOP_GRID_X, y0 + SHOP_BUY_Y, SHOP_COLS * SHOP_CELL, SHOP_ROWS * SHOP_CELL) && free_pointer);
    const FilaTienda* tip = nullptr;
    int tip_row = -1;
    char buf[256], num[32], m1[32];
    auto cell = [&](int grid_y, int k, int row, bool selection) {  // draws cell k of a grid (row: shop row or -1)
        const int cx = x0 + SHOP_GRID_X + (k % SHOP_COLS) * SHOP_CELL, cy = y0 + grid_y + (k / SHOP_COLS) * SHOP_CELL;
        if (row < 0) return;
        FilaTienda& f = a.tienda_filas[static_cast<size_t>(row)];
        const bool over = ui::sobre(e, cx, cy, SHOP_CELL - 1, SHOP_CELL - 1) && free_pointer;
        if (over) g.gpu().rectangulo(cx, cy, SHOP_CELL - 1, SHOP_CELL - 1, kh - 1, rgba(120, 100, 40, 110));
        draw_item_icon(g.gpu(), g.icono(f.gfx), cx + 1, cy + 1, ki, f.bendicion);
        // Amount: the selected one in the selection grid, the stack size in the list (only for stacks).
        const u32 amount = selection ? f.elegido : (a.tienda == TipoTienda::DEPOSITO || a.tienda == TipoTienda::RETIRO || a.tienda == TipoTienda::MASCOTA) ? f.maximo : 0;
        if (amount > 1) {
            std::snprintf(num, sizeof num, "%u", amount);
            g.texto_sombra(num, static_cast<float>(cx + SHOP_CELL - 3 - ancho_texto(num, std::strlen(num))), static_cast<float>(cy + SHOP_CELL - ALTO_GLIFO - 2),
                           kt, rgba(255, 255, 254));
        } else if (!selection && f.elegido) {  // marks what is already in the selection
            g.texto_sombra("*", static_cast<float>(cx + 3), static_cast<float>(cy + 2), kt, rgba(255, 230, 120));
        }
        if (over) {
            tip = &f;
            tip_row = row;
        }
    };
    for (int k = 0; k < SHOP_COLS * SHOP_ROWS; ++k) {
        const int i = static_cast<int>(a.tienda_desplazamiento) * SHOP_COLS + k;
        cell(SHOP_SELL_Y, k, i < n ? i : -1, false);
    }
    const int sell_tip = tip_row;
    const FilaTienda* sell_tip_row = tip;
    tip = nullptr;
    tip_row = -1;
    for (int k = 0; k < SHOP_COLS * SHOP_ROWS; ++k) {
        const int j = static_cast<int>(chosen_pos) * SHOP_COLS + k;
        cell(SHOP_BUY_Y, k, j < n_chosen ? chosen[j] : -1, true);
    }
    const int selection_hover = tip_row;
    // Clicks: a list cell selects (or asks the amount of a stack), a selection cell takes one back (Shift: all).
    if (sell_tip_row) {
        size_t m = nombre_fila(a, *sell_tip_row, buf, sizeof buf);
        row_click(a, sell_tip, true, {buf, m});
    } else if (selection_hover >= 0) {
        FilaTienda& f = a.tienda_filas[static_cast<size_t>(selection_hover)];
        if (e.clic) f.elegido = e.shift ? 0 : f.elegido - 1;
        if (e.clic_derecho) f.elegido = 0;
    }
    // Name and price of the item under the pointer.
    if (const FilaTienda* t = sell_tip_row ? sell_tip_row : tip; t) {
        const size_t m = nombre_fila(a, *t, buf, sizeof buf);
        std::string_view name{buf, m};
        char line[320];
        if (a.tienda == TipoTienda::COMPRA || a.tienda == TipoTienda::VENTA || a.tienda == TipoTienda::REPARAR)
            std::snprintf(line, sizeof line, "%.*s  %s adena", static_cast<int>(name.size()), name.data(), con_miles(t->precio, m1, sizeof m1));
        else
            std::snprintf(line, sizeof line, "%.*s", static_cast<int>(name.size()), name.data());
        const int w = ancho_texto(line, std::strlen(line));
        g.gpu().rectangulo(e.mx + 10, e.my + 14, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto(line, static_cast<float>(e.mx + 14), static_cast<float>(e.my + 17), clave_orden(CAPA_UI_TEXTO, 30));
    }
    // The two text bars: what the selection costs / yields, and the adena carried (or a hint).
    char l1[96], l2[96], m2[32];
    const u64 t = total(a);
    l2[0] = 0;
    switch (a.tienda) {
    case TipoTienda::MAGIA:
        if (a.tienda_filas.empty()) {
            std::snprintf(l1, sizeof l1, "Nothing to learn now");
            break;
        }
        [[fallthrough]];
    case TipoTienda::COMPRA:
        std::snprintf(l1, sizeof l1, "Total %s adena", con_miles(t, m1, sizeof m1));
        std::snprintf(l2, sizeof l2, "Adena %s", con_miles(adena_inventario(a), m2, sizeof m2));
        break;
    case TipoTienda::VENTA: std::snprintf(l1, sizeof l1, "You get %s adena", con_miles(t, m1, sizeof m1)); break;
    case TipoTienda::RETIRO: std::snprintf(l1, sizeof l1, "Fee %s adena (30 each)", con_miles(t, m1, sizeof m1)); break;
    case TipoTienda::MASCOTA: std::snprintf(l1, sizeof l1, a.tienda_filas.empty() ? "The pet carries nothing" : "Click: use or equip"); break;
    case TipoTienda::REPARAR: std::snprintf(l1, sizeof l1, a.tienda_filas.empty() ? "No damaged weapons" : "Click a weapon to repair it"); break;
    default: std::snprintf(l1, sizeof l1, "Click: amount  Shift: +10"); break;
    }
    g.texto_sombra(l1, static_cast<float>(x0 + SHOP_BAR_X), static_cast<float>(y0 + SHOP_BAR1_Y + 2), kt, rgba(255, 255, 254));
    if (l2[0]) g.texto_sombra(l2, static_cast<float>(x0 + SHOP_BAR_X), static_cast<float>(y0 + SHOP_BAR2_Y + 2), kt, rgba(255, 255, 254));
    if (draw_original_button(a, x0 + 46, y0 + 258, IMG_OK, IMG_OK_LIT)) confirmar(a);
    else if (draw_original_button(a, x0 + 109, y0 + 258, IMG_CANCEL, IMG_CANCEL_LIT)) cerrar(a);
}

void dibujar_tienda(App& a, int x0, int y0) {
    if (!a.cfg.ui_simple) return draw_shop_original(a, x0, y0);
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
        abrir_tienda(a, e.v[LT_TIPO] == LISTA_REPARAR ? a.npc : e.id, TIPOS[e.v[LT_TIPO]]);
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
            f.bendicion = static_cast<u8>(e.v[IT_BENDICION]);
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

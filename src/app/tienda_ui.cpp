// Tienda de jugadores (solo en las salas del mercado de Giran: mapas 340, 350, 360 y 370).
//  - Vendedores: S_DoActionGFX con la acción 70 y el título (S_DoActionShop); se los ve sentados con
//    el título sobre el nombre. Un clic camina hasta él y pide su lista (C_ShopList); S_PrivateShop
//    trae lo que vende (tipo 0) o lo que compra (tipo 1, solo los ítems propios que le sirven).
//  - Comprar: C_Result tipo 0 con (orden, cantidad); venderle: C_Result tipo 1 con el ítem propio,
//    la cantidad (3 bytes) y el orden. La cantidad se pregunta con el diálogo de cantidad.
//  - La propia: "/shop Título" abre la ventana para armarla; un clic en un ítem del inventario pide
//    el precio por unidad y lo agrega (toda la pila); "Open" la abre (C_Shop 0) y "Close" la cierra.
// shortcut: la propia solo vende (la lista de compra de C_Shop va vacía); agregarla si se usa.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "app/app.h"
#include "app/item_icon.h"

namespace lin {

namespace {

constexpr int ANCHO = 320, FILA = 36, FILAS = 8;  // FILAS: filas a la vista (de la tienda mirada o la propia)
constexpr int FILA_VENTA = ALTO_GLIFO + 6;

int x_ventana(const App& a) { return (a.gpu.ancho() - ANCHO) / 2; }
int y_ventana() { return 60; }
int alto_ventana(const App& a) {
    const TiendaJugadores& t = a.tienda_pj;
    const int filas = t.vista ? FILAS : std::clamp<int>(static_cast<int>(t.venta.size()), 1, FILAS);
    return 64 + filas * (t.vista ? FILA : FILA_VENTA) + 30;
}

void quitar_vendedor(App& a, ObjId id) {
    std::erase_if(a.tienda_pj.vendedores, [&](const Vendedor& v) { return v.id == id; });
}

void preguntar(App& a, u8 modo, ObjId objeto, ObjId item, i32 max, std::string_view titulo, const char* pregunta) {
    DialogoCantidad& d = a.cantidad;
    d = {};
    d.abierto = true;
    d.tienda = modo;
    d.objeto = objeto;
    d.item = item;
    d.titulo.assign(titulo);
    d.accion = pregunta;
    d.min = 1;
    d.max = std::max(max, 1);
    d.valor = 1;
}


}  // namespace

const Vendedor* vendedor(const App& a, ObjId id) {
    for (const Vendedor& v : a.tienda_pj.vendedores)
        if (v.id == id) return &v;
    return nullptr;
}

void eventos_tienda(App& a, const Evento& e) {
    TiendaJugadores& t = a.tienda_pj;
    switch (e.tipo) {
    case Ev::ACCION_OBJETO:
        if (e.v[0] == 70) {  // abrió la tienda (también al aparecer, si ya la tenía)
            quitar_vendedor(a, e.id);
            t.vendedores.push_back({e.id, std::string(e.s[0])});
        } else if (e.v[0] == ACC_QUIETO) {  // la cerró
            quitar_vendedor(a, e.id);
            if (t.vista == e.id) t.vista = 0;
        }
        break;
    case Ev::QUITAR_OBJETO:
        quitar_vendedor(a, e.id);
        if (t.vista == e.id) t.vista = 0;
        break;
    case Ev::MAPA:  // otro mapa: los vendedores del anterior ya no están (el servidor no manda su QUITAR)
        if (static_cast<MapId>(e.v[0]) != a.mundo.mapa) {  // los eventos de la app van antes que aplicar_eventos
            t.vendedores.clear();
            t.vista = 0;
        }
        break;
    case Ev::TIENDA:
        // Otra tienda u otra lista: desde arriba. La misma (tras una compra) conserva la posición.
        if (t.vista != e.id || t.tipo != static_cast<u8>(e.v[TP_TIPO])) t.desplaza_vista = 0;
        if (t.vista != e.id) t.grilla[0].clear(), t.grilla[1].clear();
        t.vista = e.id;
        t.tipo = static_cast<u8>(e.v[TP_TIPO]) & 1;
        t.lineas.clear();
        t.grilla[t.tipo].clear();
        break;
    case Ev::TIENDA_LINEA: {
        if (e.id != t.vista) break;
        LineaTiendaVista l;
        l.orden = static_cast<u8>(e.v[TP_ORDEN]);
        l.cantidad = static_cast<u32>(e.v[TP_CANTIDAD]);
        l.precio = static_cast<u32>(e.v[TP_PRECIO]);
        if (t.tipo == 0) {
            l.bendicion = static_cast<u8>(e.v[TP_BENDICION]);
            l.gfx = static_cast<u16>(e.v[TP_GFX]);
            l.nombre.assign(e.s[0]);
        } else {
            // Lo que compra: el servidor manda el ítem propio que le sirve; nombre e ícono del inventario.
            l.mi_item = static_cast<ObjId>(e.v[TP_ITEM]);
            const Slot s = a.mundo.inventario.items.buscar(l.mi_item);
            if (s != SIN_SLOT) {
                l.gfx = a.mundo.inventario.items.col<I_GFX>()[s];
                l.nombre.assign(nombre_item(a.mundo.inventario, s));
            }
        }
        t.grilla[t.tipo].push_back(l);
        t.lineas.push_back(std::move(l));
        break;
    }
    default: break;
    }
}

void abrir_tienda_de(App& a, ObjId v) {
    enviar(a, cmd::ver_tienda(a.op, 0, v));
    if (!a.cfg.ui_simple) enviar(a, cmd::ver_tienda(a.op, 1, v));  // la ventana original muestra las dos listas a la vez
}

bool comando_tienda(App& a, std::string_view orden, std::string_view arg) {
    if (orden != "shop" && orden != "Shop" && orden != "SHOP") return false;
    TiendaJugadores& t = a.tienda_pj;
    if (t.abierta) {
        enviar(a, cmd::cerrar_tienda(a.op));
        t.abierta = false;
        return true;
    }
    t.armando = true;
    t.vista = 0;
    if (!arg.empty()) t.titulo.assign(arg);
    a.ventana_inventario = true;
    return true;
}

bool clic_item_tienda(App& a, ObjId item) {
    TiendaJugadores& t = a.tienda_pj;
    if (!t.armando || t.abierta) return false;
    if (a.cfg.ui_simple) t.rejilla = 0;  // la ventana simple solo tiene la lista de venta
    const Slot s = a.mundo.inventario.items.buscar(item);
    if (s == SIN_SLOT || item_equipado(nombre_item(a.mundo.inventario, s))) return true;
    char nombre[256];
    const size_t n = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), nombre, sizeof nombre);
    preguntar(a, t.rejilla ? 4 : 3, 0, item, 2000000000, std::string_view(nombre, n), "Price per unit (adena)");
    return true;
}

void aceptar_cantidad_tienda(App& a) {
    DialogoCantidad& d = a.cantidad;
    TiendaJugadores& t = a.tienda_pj;
    const u32 valor = static_cast<u32>(d.valor);
    if (d.tienda == 1) {  // comprar: C_Result tipo 0 con (orden, cantidad)
        const LineaResultado l{static_cast<u32>(d.item), valor};
        enviar(a, cmd::resultado(a.op, d.objeto, RESULTADO_COMPRA, std::span(&l, 1)));
        enviar(a, cmd::ver_tienda(a.op, t.tipo, d.objeto));  // la lista actualizada
    } else if (d.tienda == 2) {  // venderle
        const auto it = std::find_if(t.lineas.begin(), t.lineas.end(), [&](const LineaTiendaVista& x) { return x.mi_item == d.item; });
        if (it != t.lineas.end()) enviar(a, cmd::vender_a_tienda(a.op, d.objeto, d.item, valor, it->orden));
        enviar(a, cmd::ver_tienda(a.op, t.tipo, d.objeto));
    } else if (d.tienda == 3 || d.tienda == 4) {  // precio: la pila entera (4 = la grilla "Buying item"; máximo 8 por grilla)
        const Slot s = a.mundo.inventario.items.buscar(d.item);
        if (s == SIN_SLOT) return;
        auto& lista = d.tienda == 3 ? t.venta : t.compra;
        std::erase_if(lista, [&](const cmd::LineaTienda& l) { return l.item == d.item; });
        if (lista.size() < 8) lista.push_back({static_cast<u32>(d.item), valor, a.mundo.inventario.items.col<I_CANTIDAD>()[s]});
    }
    d.tienda = 0;
}

bool sobre_tienda(const App& a) {
    const TiendaJugadores& t = a.tienda_pj;
    if (!t.vista && !t.armando) return false;
    if (!a.cfg.ui_simple) {
        const Rect r = rect_ventana(a, IdVentana::TIENDA_PRIV);
        return ui::sobre(a.entrada, r.x, r.y, r.w, r.h);
    }
    return ui::sobre(a.entrada, x_ventana(a), y_ventana(), ANCHO, alto_ventana(a));
}

void dibujar_tienda(App& a) {
    TiendaJugadores& t = a.tienda_pj;
    if (!t.vista && !t.armando) return;
    if (!a.cfg.ui_simple) return;  // dibujar_tienda_original, desde la pila de ventanas
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = x_ventana(a), y = y_ventana(), alto = alto_ventana(a);
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 80), ki = clave_orden(CAPA_UI, 81);
    ui::recuadro(g, x, y, ANCHO, alto, 235);
    char txt[160];

    if (t.vista) {
        // La tienda de otro: lo que vende o lo que compra; un clic en una línea pregunta cuántos.
        const Slot sv = a.mundo.objetos.buscar(t.vista);
        char nombre[64];
        const size_t nn = sv != SIN_SLOT ? expandir_desc(a.desc, texto(a.mundo, a.mundo.objetos.col<O_NOMBRE>()[sv]), nombre, sizeof nombre) : 0;
        std::snprintf(txt, sizeof txt, "Shop of %.*s", static_cast<int>(nn), nombre);
        g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
        if (ui::boton_texto(g, e, "Sells", x + 10, y + 28, 70, 18)) enviar(a, cmd::ver_tienda(a.op, 0, t.vista));
        if (ui::boton_texto(g, e, "Buys", x + 86, y + 28, 70, 18)) enviar(a, cmd::ver_tienda(a.op, 1, t.vista));
        g.texto(t.tipo == 0 ? "It sells:" : "It buys (your items):", static_cast<float>(x + 166), static_cast<float>(y + 31), kt,
                rgba(170, 160, 140));
        if (t.lineas.empty()) g.texto("Nothing.", static_cast<float>(x + 12), static_cast<float>(y + 64), kt, ui::COLOR_TEXTO);
        // Las filas a la vista se recorren con la rueda o la barra; la barra va a la derecha.
        const u32 total = static_cast<u32>(t.lineas.size());
        barra_scroll_ventana(a, t.barra_vista, x + ANCHO - 12, y + 56, FILAS * FILA, total, FILAS, t.desplaza_vista,
                             clave_orden(CAPA_UI, 82), sobre_tienda(a));
        for (u32 k = 0; k < FILAS && t.desplaza_vista + k < total; ++k) {
            const size_t i = t.desplaza_vista + k;
            const LineaTiendaVista& l = t.lineas[i];
            const int fy = y + 56 + static_cast<int>(k) * FILA;
            const bool encima = ui::sobre(e, x + 8, fy, ANCHO - 26, FILA - 2);
            g.gpu().rectangulo(x + 8, fy, ANCHO - 26, FILA - 2, clave_orden(CAPA_UI, 80), encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
            if (l.gfx) draw_item_icon(g.gpu(), g.icono(l.gfx), x + 10, fy + 1, ki, l.bendicion);
            char nom[256];
            const size_t n = expandir_desc(a.desc, l.nombre, nom, sizeof nom);
            g.texto({nom, n}, static_cast<float>(x + 48), static_cast<float>(fy + 4), kt, bless_kind(l.bendicion) == BLESS_BLESSED ? rgba(255, 230, 120) : ui::COLOR_TEXTO);
            char mil[32];
            std::snprintf(txt, sizeof txt, "%s adena  x%u", con_miles(l.precio, mil, sizeof mil), l.cantidad);
            g.texto(txt, static_cast<float>(x + 48), static_cast<float>(fy + 18), kt, rgba(255, 220, 130));
            if (encima && e.clic) {
                if (t.tipo == 0) preguntar(a, 1, t.vista, l.orden, static_cast<i32>(l.cantidad), {nom, n}, "Buy how many?");
                else preguntar(a, 2, t.vista, l.mi_item, static_cast<i32>(l.cantidad), {nom, n}, "Sell how many?");
            }
        }
        if (ui::boton_texto(g, e, "Close", x + ANCHO - 86, y + alto - 26, 76, 20) || e.escape) {
            t.vista = 0;
            e.escape = false;
        }
        return;
    }

    // La propia: lo que se va a vender, el título y abrir/cerrar.
    std::snprintf(txt, sizeof txt, "My shop: %s", t.titulo.empty() ? "(no title: /shop <title>)" : t.titulo.c_str());
    g.texto(txt, static_cast<float>(x + 10), static_cast<float>(y + 8), kt, ui::COLOR_TITULO);
    g.texto(t.abierta ? "Open. /shop again or Close to close it." : "Click an inventory item to sell it (asks the price).",
            static_cast<float>(x + 10), static_cast<float>(y + 30), kt, rgba(170, 160, 140));
    const u32 total = static_cast<u32>(t.venta.size());
    barra_scroll_ventana(a, t.barra_venta, x + ANCHO - 12, y + 56, FILAS * FILA_VENTA, total, FILAS, t.desplaza_venta,
                         clave_orden(CAPA_UI, 82), sobre_tienda(a));
    int fy = y + 56;
    for (u32 k = 0; k < FILAS && t.desplaza_venta + k < total; ++k, fy += FILA_VENTA) {
        const size_t i = t.desplaza_venta + k;
        const Slot s = a.mundo.inventario.items.buscar(t.venta[i].item);
        char nom[256];
        const size_t n = s != SIN_SLOT ? expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), nom, sizeof nom) : 0;
        char mil[32];
        std::snprintf(txt, sizeof txt, "%.*s  - %s adena", static_cast<int>(n), nom, con_miles(t.venta[i].precio, mil, sizeof mil));
        g.texto(txt, static_cast<float>(x + 12), static_cast<float>(fy), kt, ui::COLOR_TEXTO);
        if (!t.abierta && ui::boton_texto(g, e, "x", x + ANCHO - 44, fy - 2, 20, ALTO_GLIFO + 4)) {
            t.venta.erase(t.venta.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
    const int by = y + alto - 26;
    if (!t.abierta && !t.venta.empty() && ui::boton_texto(g, e, "Open", x + ANCHO - 170, by, 76, 20)) {
        enviar(a, cmd::abrir_tienda(a.op, t.venta, t.titulo));
        t.abierta = true;
    }
    if (ui::boton_texto(g, e, t.abierta ? "Close shop" : "Cancel", x + ANCHO - 86, by, 76, 20)) {
        if (t.abierta) enviar(a, cmd::cerrar_tienda(a.op));
        t.abierta = false;
        t.armando = false;
    }
}


// ---- Ventana original (ctor 0x42dc30, dibujo 0x42dfd0 / grillas 0x42eec0, callback de OK 0x42d8f0; S3EP1U.bin) ----
//  - fondo translúcido 1283 en (7,7), marco 1284, adorno 1285 en (-2,11) (los rótulos "Selling item" / "Buying item" son
//    parte del arte), cerrar 1061 en (174,18), zona de arrastre (1,5,198,20);
//  - dos grillas de 4 x 2 celdas, paso 34, en x 43, y 51 (venta) y 145 (compra); una celda ocupada muestra la marca 696 en (-1,-1)
//    y el ícono de su objeto; la celda bajo el puntero muestra una caja con el nombre del objeto y el precio (0x42f0a0..0x42f698);
//  - dos cajas de edición de 150 x 12 en (34,224) y (34,241): el título de la tienda, hasta 40 caracteres cada una (0x42da5f); OK 1289 (hover
//    1291) en (46,258) abre la tienda (C_Shop, lista de compra después de la de venta), Cancelar 1286 (hover 1288) en (109,258) cierra.
// El lado del comprador de la misma ventana muestra lo que la tienda vende y compra (S_PrivateShop tipo 0 y 1); un clic en una
// celda pregunta cuántos. Sin verificar: cómo elige el exe la grilla para un objeto nuevo (el fork: la última grilla cliqueada, venta por omisión),
// quitar una línea (el fork: clic en su celda mientras la tienda no está abierta) y la unión de las dos líneas del título.
namespace {

constexpr u32 IMG_BG = 1283, IMG_FRAME = 1284, IMG_ORN = 1285, IMG_MARK = 696, IMG_X = 1061, IMG_OK = 1289, IMG_CANCEL = 1286;
constexpr int GRID_X = 43, GRID_Y[2] = {51, 145}, CELL = 34, COLS = 4, ROWS = 2, TITLE_X = 34, TITLE_Y[2] = {224, 241};
constexpr int TITLE_W = 150, TITLE_H = 12, MAX_TITLE = 40, BTN_OK_X = 46, BTN_CANCEL_X = 109, BTN_Y = 258;

}  // namespace

bool tienda_original_abierta(const App& a) { return !a.cfg.ui_simple && (a.tienda_pj.vista || a.tienda_pj.armando); }

Rect rect_tienda_original(const App& a) {
    const RegionAtlas f = a.g ? a.g->img(IMG_FRAME) : RegionAtlas{};
    return {(a.gpu.ancho() - f.w) / 2, 60, f.w, f.h};
}

void dibujar_tienda_original(App& a) {
    if (!tienda_original_abierta(a)) return;
    TiendaJugadores& t = a.tienda_pj;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    arrastrar_ventana(a, IdVentana::TIENDA_PRIV);
    const Rect r = rect_ventana(a, IdVentana::TIENDA_PRIV);
    const u64 kb = clave_orden(CAPA_UI, 10), kf = clave_orden(CAPA_UI, 11), ko = clave_orden(CAPA_UI, 15),
              kc = clave_orden(CAPA_UI, 16), kt = clave_orden(CAPA_UI_TEXTO, 16), kicon = clave_orden(CAPA_UI, 14);
    g.gpu().dibujar(g.img(IMG_BG), r.x + 7, r.y + 7, kb, rgba(255, 255, 255, opacidad_ventanas(a)));
    g.gpu().dibujar(g.img(IMG_FRAME), r.x, r.y, kf);
    g.gpu().dibujar(g.img(IMG_ORN), r.x - 2, r.y + 11, ko);
    const bool propia = t.armando && !t.vista;
    const bool sobre_x = ui::sobre(e, r.x + 174, r.y + 18, 14, 14);
    g.gpu().dibujar(g.img(IMG_X), r.x + 174, r.y + 18, kc, sobre_x ? rgba(255, 255, 255) : rgba(200, 200, 200));
    bool cerrar = (sobre_x && e.clic) || e.escape;

    // Contenido de las celdas de las dos grillas: nombre, precio e ícono de la línea en cada celda.
    struct Celda {
        bool ocupada = false;
        u16 gfx = 0;
        u8 bendicion = 0;
        u32 precio = 0, cantidad = 0;
        std::string nombre;
        int linea = -1;
    };
    Celda celdas[2][COLS * ROWS];
    auto desde_propia = [&](const std::vector<cmd::LineaTienda>& v, Celda* c) {
        for (size_t i = 0; i < v.size() && i < COLS * ROWS; ++i) {
            const Slot s = a.mundo.inventario.items.buscar(v[i].item);
            if (s == SIN_SLOT) continue;
            char nom[256];
            const size_t n = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), nom, sizeof nom);
            c[i] = {true, static_cast<u16>(a.mundo.inventario.items.col<I_GFX>()[s]), 0, v[i].precio, v[i].cantidad,
                    std::string(nom, n), static_cast<int>(i)};
        }
    };
    if (propia) {
        desde_propia(t.venta, celdas[0]);
        desde_propia(t.compra, celdas[1]);
    } else {
        for (int gr = 0; gr < 2; ++gr)
            for (size_t i = 0; i < t.grilla[gr].size() && i < COLS * ROWS; ++i) {
                const LineaTiendaVista& l = t.grilla[gr][i];
                char nom[256];
                const size_t n = expandir_desc(a.desc, l.nombre, nom, sizeof nom);
                celdas[gr][i] = {true, l.gfx, l.bendicion, l.precio, l.cantidad, std::string(nom, n), static_cast<int>(i)};
            }
    }
    const Celda* sobre = nullptr;
    int sobre_x_c = 0, sobre_y_c = 0;
    for (int gr = 0; gr < 2; ++gr) {
        for (int i = 0; i < COLS * ROWS; ++i) {
            const int cx = r.x + GRID_X + (i % COLS) * CELL, cy = r.y + GRID_Y[gr] + (i / COLS) * CELL;
            const Celda& c = celdas[gr][i];
            if (c.ocupada) {
                g.gpu().dibujar(g.img(IMG_MARK), cx - 1, cy - 1, kicon - 1);
                if (c.gfx) draw_item_icon(g.gpu(), g.icono(c.gfx), cx, cy, kicon, c.bendicion);
            }
            if (!ui::sobre(e, cx, cy, CELL - 2, CELL - 2) || sobre_cantidad(a)) continue;
            if (c.ocupada) sobre = &c, sobre_x_c = cx, sobre_y_c = cy;
            if (!e.clic) continue;
            if (propia) {
                t.rejilla = static_cast<u8>(gr);
                if (c.ocupada && !t.abierta) {
                    auto& v = gr ? t.compra : t.venta;
                    v.erase(v.begin() + c.linea);
                }
            } else if (c.ocupada) {  // el comprador: cuántos de esta línea (grilla de venta = la vende, grilla de compra = la compra)
                const LineaTiendaVista& l = t.grilla[gr][static_cast<size_t>(c.linea)];
                if (gr == 0) preguntar(a, 1, t.vista, l.orden, static_cast<i32>(l.cantidad), c.nombre, "Buy how many?");
                else preguntar(a, 2, t.vista, l.mi_item, static_cast<i32>(l.cantidad), c.nombre, "Sell how many?");
            }
        }
        if (propia && t.rejilla == gr)  // la grilla que recibe el próximo objeto del inventario
            g.gpu().rectangulo(r.x + GRID_X - 3, r.y + GRID_Y[gr] - 3, COLS * CELL + 4, 2, kicon - 2, rgba(255, 220, 130, 160));
    }
    // Títulos: cajas de edición para la tienda propia, el texto del título del vendedor para el comprador.
    if (propia) {
        std::string* campos[2] = {&t.titulo, &t.titulo2};
        for (int i = 0; i < 2; ++i) {
            if (ui::sobre(e, r.x + TITLE_X, r.y + TITLE_Y[i], TITLE_W, TITLE_H + 2) && e.clic) t.foco = i;
            ui::caja_texto(g, e, *campos[i], r.x + TITLE_X + 2, r.y + TITLE_Y[i], TITLE_W - 4, t.foco == i && !t.abierta, false, MAX_TITLE);
        }
    } else if (const Vendedor* v = vendedor(a, t.vista)) {
        g.texto_sombra(v->titulo, static_cast<float>(r.x + TITLE_X + 2), static_cast<float>(r.y + TITLE_Y[0] + 1), kt, rgba(255, 255, 254));
    }
    // Caja de hover: nombre y precio (0x42f0a0..).
    if (sobre) {
        char mil[32], precio[64];
        std::snprintf(precio, sizeof precio, "%s adena", con_miles(sobre->precio, mil, sizeof mil));
        const int w = std::max(ancho_texto(sobre->nombre.data(), sobre->nombre.size()), ancho_texto(precio, std::strlen(precio))) + 10;
        const int bx = std::min(sobre_x_c, r.x + r.w - w), by = sobre_y_c + CELL;
        g.gpu().rectangulo(bx, by, w, 2 * ALTO_GLIFO + 8, clave_orden(CAPA_UI, 30), rgba(8, 8, 12, 235));
        g.texto_sombra(sobre->nombre, static_cast<float>(bx + 5), static_cast<float>(by + 3), clave_orden(CAPA_UI_TEXTO, 30), rgba(255, 255, 254));
        g.texto_sombra(precio, static_cast<float>(bx + 5), static_cast<float>(by + 5 + ALTO_GLIFO), clave_orden(CAPA_UI_TEXTO, 30), rgba(255, 220, 130));
    }
    // OK abre la tienda (la ventana del comprador no tiene OK: cliquear una celda compra); Cancelar cierra.
    if (propia) {
        const bool puede = !t.abierta && (!t.venta.empty() || !t.compra.empty());
        if (puede && draw_original_button(a, r.x + BTN_OK_X, r.y + BTN_Y, IMG_OK, IMG_OK + 2)) {
            std::string titulo = t.titulo;
            if (!t.titulo2.empty()) titulo += (titulo.size() < MAX_TITLE ? " " : "") + t.titulo2;
            enviar(a, cmd::abrir_tienda(a.op, t.venta, titulo, t.compra));
            t.abierta = true;
        }
    }
    if (draw_original_button(a, r.x + BTN_CANCEL_X, r.y + BTN_Y, IMG_CANCEL, IMG_CANCEL + 2)) cerrar = true;
    if (cerrar) {
        e.escape = false;
        if (t.vista) t.vista = 0;
        else {
            if (t.abierta) enviar(a, cmd::cerrar_tienda(a.op));
            t.abierta = false;
            t.armando = false;
        }
    }
}

}  // namespace lin

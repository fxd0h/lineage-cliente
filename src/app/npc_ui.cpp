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
// Diálogo de NPC original (clase del cliente en 0x43f130, dibujo 0x43f7f0): fondo 1355 en (8,11), marco 1356, adornos
// 1101 en (2,11), botón de cerrar en (226,15); barra de scroll en x 226 con flechas en y 47 y 293 sobre el riel 1354.
constexpr u32 IMG_NPC_BG = 1355, IMG_NPC_FRAME = 1356, IMG_NPC_ORNAMENT = 1101, IMG_NPC_TRACK = 1354;
constexpr int NPC_CLOSE_X = 226, NPC_CLOSE_Y = 15, NPC_TEXT_Y = 48, NPC_TEXT_H = 251;
constexpr int NPC_SCROLL_X = 226, NPC_SCROLL_Y = 47, NPC_SCROLL_H = 258, NPC_LINE = 13;  // paso de scroll = alto de línea de texto (0x43c080 / 0x43c0b0 usan 0x435370 = 13)

// Estilo de texto del cliente original en la página HTML: letra tipo Arial, sin contorno (ver recursos/html.h y fuente_ttf.cpp).
bool original_text(const App& a) { return !a.cfg.ui_simple && a.cfg.fuente_original; }
// El widget de página (ctor 0x43e170) recibe el rect (10,48)-(207,299) en 0x4589b0; su diagramación (0x43b3e0, SetRect en 0x43b4ea)
// usa el rect de texto (left+3, top+3, right-7, bottom-3) = (13,51)-(200,296) como coordenadas que ya incluyen el origen
// del widget, y el dibujo (0x436680 -> 0x43cf90) suma otra vez el borde izquierdo del widget (x del panel + 10) a la x de cada tramo
// (RECT(left + run.x, ...), 0x43d0a4). Así la primera columna de glifos queda en x 10 + 13 = 23 de la ventana y una línea se corta en 10 + 200 = 210
// (la región de recorte se ensancha 9 a la derecha, 0x4366c8..0x4366db, para que ese sobrante se vea). El origen vertical
// es la y de la línea diagramada (51 = tope del widget 48 + margen 3) más otra vez el tope del widget (48) menos el dy de OffsetRect de
// -43 - scroll en 0x4367cc / 0x436804 = y de la ventana + 56 para la primera línea (NPC_TEXT_ORIGIN_DY abajo, 5 más que el
// tope del recorte). El rect de recorte en sí no cambia.
constexpr int HTML_INSET_L = 3, HTML_INSET_T = 3, HTML_INSET_R = 7, HTML_INSET_B = 3;
constexpr int NPC_WIDGET_L = 10, NPC_WIDGET_R = 207;
constexpr int NPC_TEXT_ORIGIN_DY = 51 + 48 - 43 - (48 + HTML_INSET_T);  // 5: first line y 56 vs clip top 51
constexpr int NPC_TEXT_X = NPC_WIDGET_L + NPC_WIDGET_L + HTML_INSET_L;                   // 23: first glyph column
constexpr int NPC_TEXT_W = (NPC_WIDGET_R - HTML_INSET_R) - (NPC_WIDGET_L + HTML_INSET_L);  // 187: wrap width
int page_width(const App& a) {
    if (a.cfg.ui_simple) return ANCHO_PAGINA;
    return NPC_TEXT_W;
}
constexpr ObjId LOCAL_NPC = 0xFFFFFFFFu;  // dueño de las páginas del cliente
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
            // El original (0x437440) arma "%s-%c.html" y, si falta la página, vuelve sin
            // abrir ninguna ventana (0x43a9ca): el NPC se queda callado. Solo la interfaz de modo texto inventada conserva un aviso.
            if (!a.cfg.ui_simple) return;
            std::string aviso = "(page \"" + std::string(id) + "\" not found)";
            html.assign(aviso.begin(), aviso.end());
        }
    }
    if (g_traza_html)
        std::fprintf(stderr, "[html %.*s] datos=\"%.*s\"\n%.*s\n", static_cast<int>(id.size()), id.data(), static_cast<int>(datos.size()),
                     datos.data(), static_cast<int>(html.size()), reinterpret_cast<const char*>(html.data()));
    a.npc = npc;
    a.npc_local = false;
    a.npc_pagina = id;
    if (id.substr(0, 7) != "monlist") a.npc_poly = 0;  // any other page leaves the polymorph mode (0x6f1e60)
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
    a.npc_local = false;
    a.npc_poly = 0;
    a.tienda = TipoTienda::NINGUNA;
    a.tienda_filas.clear();
    a.tienda_marcada = a.tienda_flecha = -1;
    a.tienda_digitos = 0;
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

// `window_type` es el tipo de la ventana de lista original (0x4d1000); 0 elige la de `tipo`.
void abrir_tienda(App& a, ObjId npc, TipoTienda tipo, u8 window_type = 0) {
    static constexpr u8 TYPES[] = {0, 0, 1, 3, 2, 0xf, 0x0b, 0xd};  // by TipoTienda
    a.tienda = tipo;
    a.tienda_tipo = window_type ? window_type : TYPES[static_cast<int>(tipo)];
    a.tienda_marcada = a.tienda_flecha = -1;
    a.tienda_digitos = 0;
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
void abrir_pagina_local(App& a, std::string_view id) {
    const std::string pagina(id);  // `id` puede apuntar dentro de a.npc_pagina
    abrir_pagina(a, LOCAL_NPC, pagina, datos_opciones(a));
    a.npc_local = true;
    a.npc_casa = 0;
}

namespace {
// Botón original Opciones / Ayuda (0x4569a0 / 0x458360): la página, alterna si ya está abierta (0xee la cierra).
void alternar_pagina(App& a, std::string_view pagina) {
    if (a.npc_local && a.npc_pagina == pagina) cerrar(a);
    else abrir_pagina_local(a, pagina);
}
}  // namespace

void alternar_opciones(App& a) {
    if (a.cfg.ui_simple) a.ventana_ajustes = !a.ventana_ajustes;
    else alternar_pagina(a, "option1");
}

void alternar_ayuda(App& a) {
    if (a.cfg.ui_simple) a.ayuda_teclas = !a.ayuda_teclas;
    else alternar_pagina(a, "help");
}

// Ctrl+C / botón de las manos (0x456a30): la página es option2p si el personaje es príncipe (clase 0), si no option2; con
// esa página ya abierta cierra la ventana.
void alternar_comunidad(App& a) {
    if (a.cfg.ui_simple) {
        a.ventana_social = !a.ventana_social;
        return;
    }
    const bool principe = a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size()) && a.pjs[static_cast<size_t>(a.pj_sel)].v[PJ_CLASE] == 0;
    const std::string_view pagina = principe ? "option2p" : "option2";
    if (a.npc_local && a.npc_pagina == pagina) cerrar(a);
    else abrir_pagina_local(a, pagina);
}

// Página de polymorph "monlist" (Text.pak; los enlaces monlist0..9 son subpáginas): una acción es el nombre del monstruo. El
// manejador de acciones por omisión del original (0x40c855..0x40c97b) la manda, y cierra la ventana, como C_NPCAction con el id propio (Shape Change,
// estado 1) o como respuesta al uso del objeto (pergamino, estado 2). "none" es el botón Release; el original manda un nombre vacío para él,
// este servidor quiere la palabra.
void accion_polimorfia(App& a, std::string_view accion) {
    const std::string nombre(accion);
    if (a.npc_poly == 2) enviar(a, cmd::usar_item_texto(a.op, a.forma_item, nombre));
    else enviar(a, cmd::accion_npc(a.op, a.mundo.jugador.id, nombre));
    cerrar(a);
}

void abrir_polimorfia(App& a, ObjId pergamino) {
    abrir_pagina_local(a, "monlist");
    a.npc_poly = 2;
    a.forma_item = pergamino;
}

void activar(App& a, std::string_view vinculo, bool es_enlace) {
    if (vinculo.empty()) return;
    if (a.npc_poly && !es_enlace) {
        accion_polimorfia(a, vinculo);
        return;
    }
    if (a.npc_local) {  // páginas del cliente: los enlaces abren páginas, las acciones son órdenes propias del cliente
        if (es_enlace) abrir_pagina_local(a, vinculo);
        else accion_comunidad(a, vinculo);
        return;
    }
    if (es_enlace) {
        abrir_pagina(a, a.npc, vinculo, a.npc_datos);
    } else if (vinculo == "create") {  // bpledge2: el cliente pide el nombre (texto 83) y manda él mismo C_CreateClan
        accion_comunidad(a, vinculo);
        cerrar(a);
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
    // El tipo de ventana es el tipo de C_Result (0 compra, 1 venta, 2 / 4 / 8 depósitos, 3 / 5 / 9 retiros).
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
    const int left = simple ? MARGEN : NPC_TEXT_X;
    const int top = simple ? ARRIBA : NPC_TEXT_Y + (orig_text ? HTML_INSET_T : 0);
    const int view_h = simple ? ALTO_PAGINA : NPC_TEXT_H - (orig_text ? HTML_INSET_T + HTML_INSET_B : 0);
    // Objeto de página original (0x43c080 / 0x43c0b0): línea arriba/abajo mueven el desplazamiento en píxeles [+0x74] una línea de texto, y el
    // desplazamiento se detiene en el alto del contenido [+0x78] menos una línea, así que el rango es alto - 13 (no alto - vista). Aquí la barra
    // solo existe cuando el contenido desborda la vista.
    const int max_desp = simple ? std::max(0, p.alto - view_h) : (p.alto > view_h ? p.alto - NPC_LINE : 0);
    if (!simple) {  // la barra de scroll trabaja en líneas de texto
        if (draw_original_frame(a, x0, y0, IMG_NPC_BG, 8, 11, IMG_NPC_FRAME, NPC_CLOSE_X, NPC_CLOSE_Y)) {
            cerrar(a);
            return;
        }
        draw_original_overlay(a, x0 + 2, y0 + 11, IMG_NPC_ORNAMENT);
        u32 line = static_cast<u32>(std::clamp(a.npc_desplazamiento, 0, max_desp) / NPC_LINE);
        const u32 prev = line;
        original_scrollbar(a, a.scroll.tienda, x0 + NPC_SCROLL_X, y0 + NPC_SCROLL_Y, NPC_SCROLL_H,
                           static_cast<u32>(max_desp / NPC_LINE + 1), 1, line, false,
                           IMG_NPC_TRACK, -2);
        if (line != prev) a.npc_desplazamiento = static_cast<int>(line) * NPC_LINE;
    }
    // La página original no tiene manejador de rueda (los manejadores de 0x20a son de otros controles); el fork deja que la rueda
    // avance una línea por paso.
    if (sobre_npc(a) && e.rueda) a.npc_desplazamiento = std::clamp(a.npc_desplazamiento - e.rueda * NPC_LINE, 0, max_desp);
    a.npc_desplazamiento = std::clamp(a.npc_desplazamiento, 0, max_desp);
    const int ox = x0 + left, oy = y0 + top + (orig_text ? NPC_TEXT_ORIGIN_DY : 0) - a.npc_desplazamiento;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 12), ki = clave_orden(CAPA_UI, 13);
    const ElementoHtml* bajo = nullptr;
    // Cliente original: todas las palabras del enlace bajo el puntero se ponen verdes, no solo la apuntada (0x43d2a2
    // compara el índice de enlace de cada tramo con el del hover).
    int sobre_vinculo = -1;
    if (orig_text) {
        for (const ElementoHtml& el : p.elementos)
            if (el.vinculo_len && e.my >= y0 + top && e.my < y0 + top + view_h && ui::sobre(e, ox + el.x, oy + el.y - 1, el.w, el.h + 2)) sobre_vinculo = static_cast<int>(el.vinculo_ofs);
    }
    for (const ElementoHtml& el : p.elementos) {
        const int x = ox + el.x, y = oy + el.y;
        // Recorte al rect de la página: en la interfaz original un elemento debe caber entero (0x4367e5..0x436816, imágenes 0x4360f0).
        const int view_top = y0 + top, view_bottom = view_top + view_h;
        const int text_h = orig_text ? ALTO_LINEA_HTML_ORIGINAL : ALTO_GLIFO;
        const int el_h = el.tipo == ElementoHtml::Tipo::IMAGEN ? el.h : text_h;
        if (simple) {  // ventana del fork: prueba de la primera línea, un botón más alto no se pierde
            if (y < view_top - 2 || y + text_h > view_bottom + 2) continue;
        } else if (y < view_top || y + el_h > view_bottom) {
            // Original: los tramos de texto y las imágenes <img> (el botón de volver 331 es una) se dibujan solo si caben enteros
            // dentro del rect de la página; una imagen que cruza un borde se salta, no se recorta (0x4361b9..0x436240).
            continue;
        }
        const bool encima = el.vinculo_len && ui::sobre(e, x, y - 1, el.w, el.h + 2) && (simple || (e.my >= view_top && e.my < view_bottom));
        if (encima) bajo = &el;
        if (el.tipo == ElementoHtml::Tipo::IMAGEN) {
            // Botones: N.img, y con el puntero encima N+1 solo si es del mismo tamaño. N+1 no
            // siempre es la variante resaltada (para "back" es un sprite de elfo sin relación).
            const RegionAtlas base = g.img(el.imagen);  // copia: cargar N+1 puede mover la caché
            // Interfaz original: el exe dibuja solo la imagen del src de <img>, con o sin hover (0x4360f0 -> blit 0x4991e0 con
            // [img + 0x18]; sin N+1, sin resaltado). La suposición de N+1 del mismo tamaño es de la interfaz simple.
            const RegionAtlas hover = (encima && simple) ? g.img(el.imagen + 1) : base;
            const bool use_hover = hover.valida() && hover.w == base.w && hover.h == base.h;
            RegionAtlas r = use_hover ? hover : base;
            const int cut_top = simple ? std::max(0, view_top - y) : 0;  // solo interfaz simple: el original nunca recorta
            if (cut_top) {  // recorta la región (rect en texels; w/h/oy son lógicos)
                const int sc = static_cast<int>(r.escala_texel());
                r.y = static_cast<u16>(r.y + cut_top * sc);
                r.h = static_cast<u16>(std::max(0, r.h - cut_top));
                r.oy = static_cast<i16>(r.oy + cut_top);
            }
            g.gpu().dibujar(r, x, y, ki);
        } else {
            if (orig_text) {  // sin subrayado: el cliente nunca activa el flag de subrayado del estilo del tramo
                const bool verde = el.vinculo_len && static_cast<int>(el.vinculo_ofs) == sobre_vinculo;
                g.texto_html(p.texto(el), static_cast<float>(x), static_cast<float>(y), kt, verde ? COLOR_SOBRE_HTML_ORIGINAL : el.color, el.negrita);
            } else {
                g.texto(p.texto(el), static_cast<float>(x), static_cast<float>(y), kt, encima ? rgba(255, 255, 254) : el.color);
                if (el.vinculo_len) g.gpu().rectangulo(x, y + ALTO_GLIFO, el.w, 1, ki, el.color);
            }
        }
    }
    // Cursor original sobre la página (vista HTML 0x436d60, prueba de acierto 0x436fd0): 3 sobre un enlace / botón / control embebido, si no 1.
    if (!simple && ui::sobre(e, x0 + left, y0 + top, NPC_TEXT_W, view_h)) a.cursor_html = bajo ? 3 : 1;
    if (simple && max_desp > 0) {  // barra de desplazamiento
        int h = ALTO_PAGINA * ALTO_PAGINA / p.alto, yb = y0 + ARRIBA + (ALTO_PAGINA - h) * a.npc_desplazamiento / max_desp;
        g.gpu().rectangulo(x0 + ui::VENTANA_ANCHO - 8, yb, 3, h, ki, rgba(160, 130, 80, 200));
    }
    if (bajo) {
        // Las etiquetas de página de S3EP1U no tienen atributo `tooltip` (tabla de etiquetas 0x503604: action, link, src, bg, fg, ...); el original no muestra ninguno.
        std::string_view ayuda = simple ? p.ayuda(*bajo) : std::string_view{};
        if (!ayuda.empty()) {
            int w = ancho_texto(ayuda.data(), ayuda.size());
            g.gpu().rectangulo(e.mx + 10, e.my + 14, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
            g.texto(ayuda, static_cast<float>(e.mx + 14), static_cast<float>(e.my + 17), clave_orden(CAPA_UI_TEXTO, 30));
        }
        if (e.clic) activar(a, p.vinculo(*bajo), bajo->es_enlace);
    }
}

// Manejo del clic en una fila de tienda, común a la lista simple y a la grilla original: reparar, bolsa de mascota, el diálogo
// de cantidad para pilas y la selección simple +1 / -1. `hover` indica si el puntero está sobre la fila.
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


// Ventana de lista original del cliente US 2009 (S3EP1U): la tienda del NPC (compra y venta), los depósitos, la bolsa de mascota, la
// lista de hechizos y la lista de reparación de armas son todas una clase de ventana (0x4f55d8, ctor 0x4bef40, dibujo 0x4bf690) cuyo
// tipo (0x4d1000) solo cambia el arte de los botones y los textos. La ventana anterior con grilla de 4x2 (marco 1284, ctor 0x42dc30)
// es la tienda privada ("Selling item" / "Buying item"), no la tienda del NPC.
//  - fondo 1099 en (8,11), marco 1100, adornos 1101 en (2,11), cerrar (225,15), etiqueta "Adena:" 1106 en (29,31),
//    su número en (80,31);
//  - panel (24,44)-(225,304): 7 filas de 36 px desde (25,51) (0x4be220); barra de scroll 1354 (arriba 974 en (225,47), riel en y 55,
//    abajo 972 en (226,293), botón 976);
//  - fila (0x4bd250): ícono en el origen de la fila, nombre en (+38,+3), cantidad elegida alineada a la derecha hasta +176 en y +19 entre las
//    flechas 556 (izquierda) y 554 (derecha, en +180), el precio a la izquierda de la flecha izquierda con la moneda de adena 325;
//  - botones en y 306: Comprar 1109 / Vender 1107 / OK 1114 en x 79, Cancelar 1111 en x 131 (arte presionado = id + 1).
constexpr u32 IMG_LIST_BG = 1099, IMG_LIST_FRAME = 1100, IMG_LIST_ORNAMENT = 1101, IMG_LIST_LABEL = 1106, IMG_LIST_TRACK = 1354;
constexpr u32 IMG_BUY = 1109, IMG_SELL = 1107, IMG_OK = 1114, IMG_CANCEL = 1111, IMG_COIN = 325, IMG_ROW_MARK = 536;
constexpr u32 IMG_ARROW_RIGHT = 554, IMG_ARROW_LEFT = 556;  // +1: pressed
constexpr int LIST_CLOSE_X = 225, LIST_CLOSE_Y = 15, LIST_ROWS = 7, LIST_ROW_H = 36, LIST_ROW_X = 25, LIST_ROW_Y = 51;
constexpr int LIST_PANE_X = 24, LIST_PANE_Y = 44, LIST_PANE_W = 201, LIST_PANE_H = 260;
constexpr int LIST_SCROLL_X = 225, LIST_SCROLL_Y = 47, LIST_SCROLL_H = 258;
constexpr int LIST_BTN_Y = 306, LIST_BTN_X = 79, LIST_CANCEL_X = 131;
constexpr int ARROW_SIDE = 12;  // lado cliqueable de una flecha de cantidad (0x4be0..: rect de 12 x 12)

// Text colours 0x8f0f34[]: 0 blessed (255,246,180), 1 normal white, 2 cursed red, 3 unidentified grey; set at 0x4d16bc.
u32 state_color(u8 state) {
    static constexpr u16 PALETTE[] = {0x7fd6, 0x7fff, 0x5ca7, 0x5ef7};
    return rgb555(PALETTE[state & 3]);
}

// Texto original de un número: miles separados (0x44f4f0).
const char* digits(u64 v, char* buf, size_t n) { return con_miles(v, buf, n); }

// Flechas de cantidad, como 0x4bd840 (izquierda) y 0x4bd8a0 (derecha): la cantidad da la vuelta entre 0 y el tamaño de la pila (una fila
// de tienda no tiene límite de pila sino 1000 por serie de clics, el tope del tipo 0 que se empuja en 0x4bddd1), y un precio de
// 10 000 000 o más permite solo uno.
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

// El marco de dos tonos y el oscurecido del 50 % del tooltip de la lista (0x4be41e..0x4be576): 142 px de ancho en la x del panel
// + 10 (los umbrales de y son coordenadas de pantalla), líneas del nombre en +5 / +5 + 12 k.
// La caja de hover de las listas (0x49a440 oscurece a la mitad los píxeles debajo, después ocho líneas de 0x498720: un bisel claro de 2 px (color
// [0x50050c] = 189) arriba y a la izquierda, uno oscuro ([0x500514] = 123) abajo y a la derecha, cada uno empezando un píxel
// más adentro que el anterior). `details`: las líneas de propiedades del objeto (bytes de estado de S_ShopSellList).
void draw_list_tooltip(App& a, int x0, int row_y, const char* line1, const char* line2, u32 color, std::string_view details = {}) {
    Graficos& g = *a.g;
    char det[1024];
    const size_t nd = describir_atributos(details, det, sizeof det);
    int lines = line2[0] ? 2 : 1;
    for (std::string_view rest(det, nd); !rest.empty(); ++lines) rest.remove_prefix(std::min(std::min(rest.find('\n'), rest.size()) + 1, rest.size()));
    const int h = 12 * lines + 10, w = 142, x = x0 + 34;
    int top = row_y + 32 > 300 ? row_y - 12 : row_y + 32;
    if (top > 157) top = std::max(10, top - 12 * (lines - 1) - 22);
    else top = std::min(top - 33, 330 - h);
    draw_original_hover_box(a, x, top, w, h);
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 30);
    g.texto_sombra(line1, static_cast<float>(x + 5), static_cast<float>(top + 5), kt, color);
    int y = top + 17;
    if (line2[0]) g.texto_sombra(line2, static_cast<float>(x + 5), static_cast<float>(y), kt, color), y += 12;
    for (std::string_view rest(det, nd); !rest.empty();) {
        const size_t end = std::min(rest.find('\n'), rest.size());
        g.texto_sombra(rest.substr(0, end), static_cast<float>(x + 5), static_cast<float>(y), kt, rgba(255, 255, 254));
        y += 12;
        rest.remove_prefix(std::min(end + 1, rest.size()));
    }
}

// Un nombre en una o dos líneas: la segunda empieza en el último espacio que deja la primera dentro de `width`.
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
    // Totales (0x4be800 suma precio x cantidad, 0x4be830 cuenta las filas con cantidad).
    u64 total_price = 0;
    u32 chosen_rows = 0;
    for (const FilaTienda& f : a.tienda_filas) {
        total_price += static_cast<u64>(f.precio) * f.elegido;
        chosen_rows += f.elegido ? 1u : 0u;
    }
    const u64 adena = adena_inventario(a);
    const bool fee = a.tienda == TipoTienda::RETIRO;
    const u64 amount = fee ? static_cast<u64>(chosen_rows) * PRECIO_RETIRO : total_price;
    // Filas.
    const bool show_price = true;  // flag [+0x95] de 0x4bd250: lo fija el constructor de la lista (0x4bd980), se apaga solo para el tipo 0x10
    int hover_row = -1;
    bool hover_arrow = false;
    char tip1[160] = "", tip2[160] = "";
    u32 tip_color = state_color(1);
    int tip_y = 0;
    std::string tip_details;
    const int first = static_cast<int>(a.tienda_desplazamiento);
    // Fila bajo el puntero (manejador de movimiento 0x4be19e): el exe divide la y del puntero desde la primera fila por 34 mientras dibuja las filas
    // a 36 de distancia, así que la fila del tooltip se corre de la dibujada (se conserva); necesita la columna del ícono (x 13..48 de la fila).
    int hover_idx = -1;
    if (free_pointer && e.mx >= x0 + LIST_ROW_X && e.mx < x0 + LIST_ROW_X + 36 && e.my >= y0 + LIST_ROW_Y) {
        const int k = (e.my - (y0 + LIST_ROW_Y)) / 34;
        if (k < LIST_ROWS && first + k < n) hover_idx = first + k;
    }
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
        // 0x498d90 con la tabla 0x8f133c (armada en 0x4d1649: azul -> (2 * azul + 31) / 3 por píxel, rojo y verde intactos): una mezcla
        // screen de (0,0,85) da exactamente eso en los canales de 8 bits.
        if (f.elegido) g.gpu().rectangulo(x0 + 24, ry - 2, 192, 34, km, rgba(0, 0, 85), Mezcla::PANTALLA);
        if (idx == a.tienda_marcada) g.gpu().dibujar(g.img(IMG_ROW_MARK), rx - 2, ry - 2, km);
        draw_item_icon(g.gpu(), g.icono(f.gfx), rx, ry, ki, f.bendicion);
        char name[256];
        const size_t m = nombre_fila(a, f, name, sizeof name);
        char l1[160], l2[160];
        split_name({name, m}, 140, l1, l2, sizeof l1);
        g.texto_sombra(l1, static_cast<float>(rx + 38), static_cast<float>(ry + 3), kt, color);
        // La segunda línea comparte su altura con el precio: se detiene antes de la moneda (16 caracteres como mucho, 0x4bd808).
        size_t n2 = std::strlen(l2);
        const int limit = (show_price ? left_x - 4 - ancho_texto(digits(f.precio, b2, sizeof b2), std::strlen(b2)) - 4 - 13 : left_x) - (rx + 38);
        while (n2 > 0 && ancho_texto(l2, n2) > limit) --n2;
        l2[n2] = 0;
        if (l2[0]) g.texto_sombra(l2, static_cast<float>(rx + 38), static_cast<float>(ry + 17), kt, color);
        // Cantidad y flechas.
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
            if (a.tienda == TipoTienda::REPARAR) {  // los tipos 0xd / 0xe colorean el precio según su magnitud (0x4bd637)
                static constexpr u16 STEPS[] = {0x7fd6, 0x63b6, 0x5a36, 0x5fbc, 0x509e, 0x5e8d};
                static constexpr u64 LIMITS[] = {10000, 100000, 1000000, 10000000, 100000000};
                int s = 0;
                while (s < 5 && price >= LIMITS[s]) ++s;
                price_color = s == 0 ? color : rgb555(STEPS[s]);
            }
            g.texto_sombra(b2, static_cast<float>(price_x), static_cast<float>(ay), kt, price_color);
            g.gpu().dibujar(g.img(IMG_COIN), price_x - 13, ay, kc);
        }
        // Puntero: la columna del ícono muestra el tooltip; presionar una flecha empieza una flecha mantenida; presionar la fila la marca.
        if (over_row && (e.pulsado || e.clic)) a.tienda_digitos = 0;  // un evento del mouse en el panel borra la cantidad escrita (0x4be177)
        if (over_row && e.pulsado) a.tienda_marcada = idx;
        if (e.pulsado && over_left) a.tienda_flecha = idx * 2;
        if (e.pulsado && over_right) a.tienda_flecha = idx * 2 + 1;
        // Actúa el soltar sobre la flecha (0x4bdd9d); un clic inyectado solo como soltar (sin presión vista) también cuenta.
        if (e.clic && (a.tienda_flecha == idx * 2 || a.tienda_flecha < 0) && over_left) step_amount(a, f, false);
        if (e.clic && (a.tienda_flecha == idx * 2 + 1 || a.tienda_flecha < 0) && over_right) step_amount(a, f, true);
        if (over_left || over_right) hover_arrow = true;
        if (free_pointer && idx == hover_idx) {
            hover_row = idx;
            tip_details = f.atributos;
            std::snprintf(tip1, sizeof tip1, "%s", l1);
            std::snprintf(tip2, sizeof tip2, "%s", l2);
            tip_color = color;
            tip_y = ry;
        }
        // La bolsa de mascota y la lista de reparación actúan con un clic simple en la fila (flujo del fork: las flechas no les aplican).
        if (over_row && !over_left && !over_right && e.clic && (a.tienda == TipoTienda::MASCOTA || a.tienda == TipoTienda::REPARAR))
            row_click(a, idx, true, {name, m});
    }
    if (e.clic || !e.presionado) a.tienda_flecha = -1;
    // Etiqueta de adena y el monto (0x4bf8e0..0x4bfc5d): "total/adena", en rojo si la adena no alcanza.
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
    if (hover_row >= 0 && !hover_arrow && tip1[0]) draw_list_tooltip(a, x0, tip_y, tip1, tip2, tip_color, tip_details);
    // Botones: arte presionado mientras se mantiene el botón (los botones de 0x47c2d0 no tienen arte de hover).
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

}  // namespace

// Teclas del panel de lista (0x4be860, se llega por la casilla 10 de la ventana en 0x4bf630): con una fila marcada, los dígitos arman la
// cantidad (acc * 10 + d) y Backspace quita el último (acc / 10); Enter se descarta. La cantidad se satura en el límite de la
// fila: el tamaño de la pila, si no el tope de la lista (1000 al comprar), el menor de ambos, y 10 para una fila de compra de 1 000 000
// o más (0x4be8cb..0x4be937). Los dígitos no llegan a la barra de atajos ni a la línea de chat.
void teclas_lista_tienda(App& a) {
    if (a.cfg.ui_simple || a.tienda == TipoTienda::NINGUNA || a.tienda_marcada < 0 ||
        a.tienda_marcada >= static_cast<int>(a.tienda_filas.size()) || a.cantidad.abierto)
        return;
    EntradaFrame& e = a.entrada;
    FilaTienda& f = a.tienda_filas[static_cast<size_t>(a.tienda_marcada)];
    const bool buying = a.tienda == TipoTienda::COMPRA;
    const u32 stack = buying ? 0u : f.maximo, cap = buying ? 1000u : 0u;
    u32 limite = stack ? (cap ? std::min(stack, cap) : stack) : cap;
    if (buying && f.precio >= 1000000 && limite > 10) limite = 10;
    bool cambio = false;
    int w = 0;
    for (int i = 0; i < e.ntexto; ++i) {
        const char c = e.texto[i];
        if (c >= '0' && c <= '9') {
            a.tienda_digitos = a.tienda_digitos * 10 + static_cast<u32>(c - '0');
            if (limite && a.tienda_digitos > limite) a.tienda_digitos = limite;
            if (a.tienda_digitos > 99999999u) a.tienda_digitos = 99999999u;
            cambio = true;
        } else {
            e.texto[w++] = c;
        }
    }
    e.ntexto = w;
    e.texto[w] = 0;
    if (e.retroceso) {
        a.tienda_digitos /= 10;
        e.retroceso = false;
        cambio = true;
    }
    if (e.enter) e.enter = false;
    e.numero = 0;
    if (cambio) f.elegido = a.tienda_digitos;
}

namespace {

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
        const u8 b = bless_kind(f.bendicion);
        u32 color = b == BLESS_BLESSED ? rgba(255, 230, 120) : b == BLESS_CURSED ? rgba(255, 110, 100) : rgba(255, 255, 254);
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
        if (!a.cfg.ui_simple && es_pagina_lista(e.s[0])) {  // party / pledge / buddy: sus propias ventanas (0x497240, 0x4b92f0, 0x419040)
            abrir_lista_orig(a, e.s[0], e.s[1]);
            break;
        }
        if (e.s[0] == "monlist" && a.cfg.ui_simple) break;  // Shape Change, simple UI: forma_ui.cpp
        if (e.s[0].empty()) {
            if (a.npc == e.id) cerrar(a);
            break;
        }
        abrir_pagina(a, e.id, e.s[0], e.s[1]);
        a.npc_casa = static_cast<u32>(e.v[HT_CASA]);
        if (e.s[0] == "monlist" && e.id == a.mundo.jugador.id) {  // Shape Change: la respuesta es C_NPCAction con el id propio
            a.npc_poly = 1;
            a.forma_item = 0;
        }
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
            f.atributos = atributos_item(a.mundo.inventario, s);
        } else {
            f.gfx = static_cast<u16>(e.v[IT_GFX]);
            f.maximo = a.tienda == TipoTienda::COMPRA ? 9999 : static_cast<u32>(e.v[IT_CANTIDAD]);
            f.bendicion = a.tienda == TipoTienda::COMPRA ? u8{1} : static_cast<u8>(e.v[IT_BENDICION]);  // the shop sends no state: normal
            f.nombre = e.s[0];
            f.atributos = e.s[1];
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

void procesar_npc(App& a, u64 /*ahora*/) {
    if (a.npc_local) return;
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
    // La tienda original dibuja su propio marco; las otras ventanas todavía usan el genérico.
    if (!a.cfg.ui_simple) {
    } else if (ui::ventana(g, a.entrada, a.cfg.ui_simple, rv.x, rv.y, titulo, opacidad_ventanas(a))) {
        cerrar(a);
        return;
    }
    if (a.tienda != TipoTienda::NINGUNA) dibujar_tienda(a, rv.x, rv.y);
    else dibujar_pagina(a, rv.x, rv.y);
}

void draw_original_hover_box(App& a, int x, int top, int w, int h) {
    Graficos& g = *a.g;
    const u64 kb = clave_orden(CAPA_UI_TEXTO, 28), kl = clave_orden(CAPA_UI_TEXTO, 29);
    g.gpu().rectangulo(x, top, w, h, kb, rgba(0, 0, 0, 200));  // sobre los textos de las filas de abajo (el exe oscurece a la mitad los píxeles, texto incluido)
    const u32 light = rgba(189, 189, 189), dark = rgba(123, 123, 123);
    g.gpu().rectangulo(x, top, w - 1, 1, kl, light);
    g.gpu().rectangulo(x + 1, top + 1, w - 2, 1, kl, light);
    g.gpu().rectangulo(x, top + 1, 1, h - 1, kl, light);
    g.gpu().rectangulo(x + 1, top + 2, 1, h - 2, kl, light);
    g.gpu().rectangulo(x + 1, top + h - 1, w - 1, 1, kl, dark);
    g.gpu().rectangulo(x + 2, top + h - 2, w - 2, 1, kl, dark);
    g.gpu().rectangulo(x + w - 1, top, 1, h - 1, kl, dark);
    g.gpu().rectangulo(x + w - 2, top + 1, 1, h - 2, kl, dark);
}

u32 original_item_color(u8 state) { return state_color(state); }

}  // namespace lin

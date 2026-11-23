// Entregable 6: ventana de inventario (Alt+I). Grilla de íconos originales (N.tbt), nombre al
// pasar el mouse, ítems equipados marcados, peso; doble clic usa (equipa, bebe...) y arrastrar
// un ítem fuera de la ventana lo suelta al suelo. Arrastrarlo al basurero (abajo a la derecha) lo
// elimina, para lo que no se puede soltar ni vender (el servidor rechaza lo equipado y lo que no se
// puede borrar, con su mensaje).
// Ordenar: el botón "Sort" (equipado, tipo de uso y nombre) o arrastrar un ítem sobre otra celda. El
// servidor no guarda posiciones: el orden queda en settings.ini por personaje y vuelve tras S_InvList.
// Shift+clic marca varios ítems; arrastrar uno marcado lleva todos (suelo, basurero, intercambio, NPC
// u otra celda).
#include <algorithm>
#include <charconv>
#include <cstring>
#include <string>
#include <tuple>
#include "app/app.h"
#include "app/item_icon.h"

namespace lin {

namespace {

// Grilla de objetos dentro de la ventana: las 6 × 7 celdas de 36 px de la interfaz simple, o las 4 × 6 de 34 del marco original
// (las líneas de la imagen 997), con el tacho, las pestañas y la barra de scroll donde las pone el cliente 2009.
struct Grid {
    int cols, rows, cell, x, y;    // celdas y la esquina de la grilla
    int trash_x, trash_y;          // tacho de basura
    int scroll_x;                  // scroll bar
};
constexpr Grid SIMPLE_GRID = {6, 7, 36, 14, 30, 14 + 6 * 36 - 22, 30 + 7 * 36 + 2, 14 + 6 * 36 + 2};
constexpr Grid ORIGINAL_GRID = {4, 6, 34, 20, 41, 163, 252, 162};
constexpr int BASURA_W = 20, BASURA_H = 18;
constexpr u64 MS_DOBLE_CLIC = 400;
constexpr u32 ICON_ARMED_A = 2700, ICON_ARMED_B = 2701, ICON_SEALED = 2702;  // icon-table overlays (0x44e131..0x44e39a)
constexpr u32 IMG_EQUIPPED_MARK = 536;  // behind an equipped icon at (x-1, y-1) (0x44e010)
constexpr int DRAG_START_PX = 25;       // el original empieza a arrastrar cuando el puntero se movió 25 px (0x44f240)
constexpr u32 IMG_INV_BACKGROUND = 996, IMG_INV_FRAME = 997, IMG_INV_ORNAMENT = 998, IMG_INV_TABS = 999;
constexpr int TABS_X = 17, TABS_HIT_X = 19, TABS_Y = 249, TAB_W = 35, TAB_H = 17, N_TABS = 4;
constexpr const char* TAB_NAMES[N_TABS] = {"All", "Equipment", "Quest", "Miscellaneous"};

// Filtros de pestaña de la interfaz original (0x44de10 todo, 0x44dd90 equipo, 0x44de40 misión, 0x44df00 varios). Equipo es el
// tipo de uso (byte en item+0x68, "uso" de S_AddItem) en {1,2,10,18..25,37,40}; los objetos de misión son los pares (gfx, entrada de
// desc-e.tbl) de itemquest.tbl cuyo nombre coincide; varios es todo lo que no es ninguno de los dos.
bool es_equipo(u8 use) {
    return use == 1 || use == 2 || use == 10 || (use >= 18 && use <= 25) || use == 37 || use == 40;
}
}  // namespace

namespace {
int tab_of(const App& a, u8 use, u16 gfx, std::string_view name) {
    if (es_equipo(use)) return 1;
    return item_de_mision(a.item_quest, a.desc, gfx, name) ? 2 : 3;
}

// El basurero del original (1003; 1004 resaltado, con un ítem encima); si no está, con rectángulos.
void dibujar_basurero(Graficos& g, int x, int y, bool encima) {
    const u64 k = clave_orden(CAPA_UI, 13);
    if (const RegionAtlas& r = g.img(encima ? 1004 : 1003); r.valida()) {
        g.gpu().dibujar(r, x + 3, y, k);
        return;
    }
    const u32 c = encima ? rgba(230, 110, 90) : rgba(170, 160, 140), oscuro = rgba(40, 32, 22, 255);
    g.gpu().rectangulo(x + 7, y, 6, 2, k, c);
    g.gpu().rectangulo(x + 1, y + 2, 18, 3, k, c);
    g.gpu().rectangulo(x + 3, y + 6, 14, 12, k, c);
    for (int i = 0; i < 3; ++i) g.gpu().rectangulo(x + 6 + i * 3, y + 8, 1, 8, clave_orden(CAPA_UI, 14), oscuro);
}

constexpr size_t MAX_ORDEN = 256;

// Posición (ya desplazada) de la celda bajo el puntero entre los objetos `shown`, o -1.
int celda_bajo_puntero(const App& a, const Grid& gr, int x0, int y0, size_t shown) {
    const EntradaFrame& e = a.entrada;
    int cx = e.mx - (x0 + gr.x), cy = e.my - (y0 + gr.y);
    if (cx < 0 || cy < 0 || cx >= gr.cols * gr.cell || cy >= gr.rows * gr.cell) return -1;
    int i = static_cast<int>(a.inventario_desplazamiento) * gr.cols + (cy / gr.cell) * gr.cols + cx / gr.cell;
    return i < static_cast<int>(shown) ? i : -1;
}

}  // namespace

// Primer ítem del inventario cuyo nombre (con "$N" resueltos) contenga `texto`; 0 si no hay.
ObjId buscar_item(const App& a, std::string_view texto) {
    const TablaItems& t = a.mundo.inventario.items;
    char buf[256];
    for (Slot s = 0; s < t.tamano(); ++s) {
        size_t n = expandir_desc(a.desc, nombre_item(a.mundo.inventario, s), buf, sizeof buf);
        if (std::string_view(buf, n).find(texto) != std::string_view::npos) return t.ids()[s];
    }
    return 0;
}

namespace {

// El orden en settings.ini (inventario_orden), por personaje: se guarda al cambiarlo a mano.
void guardar_orden(App& a) {
    std::string v;
    for (ObjId id : a.mundo.inventario.items.ids()) (v += std::to_string(id)) += ',';
    if (!v.empty()) v.pop_back();
    poner_valor_de_personaje(a.cfg.inventario_orden, nombre_personaje(a), v);
    guardar_config(a.cfg);
}

// Tras S_InvList (al entrar), el orden guardado; los ítems nuevos quedan al final.
void cargar_orden(App& a) {
    Inventario& inv = a.mundo.inventario;
    if (!inv.recien_cargado) return;
    inv.recien_cargado = false;
    std::vector<ObjId> ids;
    for (std::string_view v = valor_de_personaje(a.cfg.inventario_orden, nombre_personaje(a)); !v.empty();) {
        const std::string_view t = v.substr(0, v.find(','));
        v = t.size() < v.size() ? v.substr(t.size() + 1) : std::string_view{};
        ObjId id = 0;
        std::from_chars(t.data(), t.data() + t.size(), id);
        if (id) ids.push_back(id);
    }
    if (!ids.empty()) ordenar_por_ids(inv, ids);
}

// Equipado primero, después por tipo de uso y por nombre (con los "$N" resueltos).
void ordenar_inventario(App& a) {
    Inventario& inv = a.mundo.inventario;
    const size_t n = inv.items.tamano();
    std::vector<std::string> nombres(n);
    std::vector<Slot> orden(n);
    char buf[256];
    for (Slot s = 0; s < n; ++s) {
        nombres[s].assign(buf, expandir_desc(a.desc, nombre_item(inv, s), buf, sizeof buf));
        orden[s] = s;
    }
    auto clave = [&](Slot s) {
        return std::tuple(!item_equipado(nombre_item(inv, s)), inv.items.col<I_USO>()[s], std::string_view(nombres[s]));
    };
    std::stable_sort(orden.begin(), orden.end(), [&](Slot x, Slot y) { return clave(x) < clave(y); });
    inv.items.reordenar(orden);
    guardar_orden(a);
}

bool seleccionado(const App& a, ObjId id) {
    return std::find(a.inventario_seleccion.begin(), a.inventario_seleccion.end(), id) != a.inventario_seleccion.end();
}

}  // namespace

bool sobre_inventario(const App& a) { return sobre_ventana(a, IdVentana::INVENTARIO); }

// Uso 16 (pergamino de polymorph): primero se elige el monstruo (forma_ui.cpp).
constexpr u8 USO_POLYMORPH = 16, USO_TELEPORT = 6, USO_TELEPORT_BENDITO = 29;  // ItemTable (useType)
// Se usan sobre otro ítem (C_UseItem lee el objetivo): identificar, encantar arma y armadura...
bool se_usa_sobre_item(u8 uso) { return uso == 7 || uso == 14 || uso == 26 || uso == 27; }

// Aimed items (ItemTable useType spell_long, spell_short, spell_point): C_UseItem reads d target, h x, h y.
bool se_usa_apuntando(u8 uso) { return uso == 5 || uso == 17 || uso == 39; }

void procesar_item_apuntado(App& a) {
    if (!a.item_apuntando) return;
    EntradaFrame& e = a.entrada;
    if (a.mundo.inventario.items.buscar(a.item_apuntando) == SIN_SLOT || e.clic_derecho || e.escape) {
        a.item_apuntando = 0;
        e.escape = false;  // Esc cancela solo esto
        return;
    }
    if (e.pulsado && !sobre_alguna_ventana(a) && !sobre_barra(a)) {
        a.clic_consumido = true;
        const ObjId obj = objeto_bajo_puntero(a, true);
        const Slot s = obj ? a.mundo.objetos.buscar(obj) : SIN_SLOT;
        if (s != SIN_SLOT)
            enviar(a, cmd::usar_item_apuntado(a.op, a.item_apuntando, obj, a.mundo.objetos.col<O_X>()[s], a.mundo.objetos.col<O_Y>()[s]));
        a.item_apuntando = 0;  // un clic en el suelo cancela
    }
}

void usar_item(App& a, ObjId item) {
    Slot s = a.mundo.inventario.items.buscar(item);
    if (s != SIN_SLOT) {
        a.gfx_item_usado = a.mundo.inventario.items.col<I_GFX>()[s];
        if (a.gfx_item_usado >= 2498 && a.gfx_item_usado <= 2502) a.cocina.pan = item;  // cook book (0x449a70 -> [0x8f2fb8])
        a.t_item_usado = a.entrada.ahora_ms;
    }
    if (s != SIN_SLOT && se_usa_sobre_item(a.mundo.inventario.items.col<I_USO>()[s])) {
        a.usar_sobre = item;  // el próximo clic en el inventario elige el objetivo
        a.usar_sobre_armado = false;
        a.ventana_inventario = true;
        return;
    }
    if (s != SIN_SLOT && se_usa_apuntando(a.mundo.inventario.items.col<I_USO>()[s])) {
        a.item_apuntando = item;  // el próximo clic en el mundo elige el objetivo (uno mismo incluido)
        return;
    }
    if (s != SIN_SLOT && a.mundo.inventario.items.col<I_USO>()[s] == USO_POLYMORPH) {
        if (a.cfg.ui_simple) abrir_forma(a, item);
        else abrir_polimorfia(a, item);
        return;
    }
    // Pergamino de teleport: el bendito deja elegir un marcador guardado; el normal va siempre al azar.
    const u8 uso = s != SIN_SLOT ? a.mundo.inventario.items.col<I_USO>()[s] : 0;
    if (uso == USO_TELEPORT || uso == USO_TELEPORT_BENDITO) {
        if (uso == USO_TELEPORT || !elegir_destino(a, item, 0)) enviar(a, cmd::usar_teleport(a.op, item, 0, 0));
        return;
    }
    enviar(a, cmd::usar_item(a.op, item));
}

// Al suelo, en la celda propia y con toda la cantidad.
void soltar_item(App& a, ObjId item, bool preguntar) {
    Slot yo = a.mundo.objetos.buscar(a.mundo.jugador.id);
    Slot s = a.mundo.inventario.items.buscar(item);
    if (yo == SIN_SLOT || s == SIN_SLOT) return;
    if (preguntar && !a.cfg.ui_simple && a.mundo.inventario.items.col<I_CANTIDAD>()[s] > 1) return pedir_soltar(a, item, a.mundo.inventario.items.col<I_CANTIDAD>()[s]);
    enviar(a, cmd::soltar(a.op, a.mundo.objetos.col<O_X>()[yo], a.mundo.objetos.col<O_Y>()[yo], item,
                          a.mundo.inventario.items.col<I_CANTIDAD>()[s]));
}

void dibujar_inventario(App& a) {
    cargar_orden(a);  // el orden solo se ve aquí: basta con aplicarlo al abrir la ventana
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const Inventario& inv = a.mundo.inventario;
    const TablaItems& t = inv.items;
    arrastrar_ventana(a, IdVentana::INVENTARIO);
    std::erase_if(a.inventario_seleccion, [&](ObjId id) { return t.buscar(id) == SIN_SLOT; });  // gastados o soltados
    // Lo que se arrastra: la selección si el ítem está en ella; si no, solo él.
    const std::span<const ObjId> arrastrados =
        a.arrastrando && seleccionado(a, a.arrastrando) ? std::span<const ObjId>(a.inventario_seleccion)
                                                        : std::span<const ObjId>(&a.arrastrando, a.arrastrando ? 1 : 0);
    auto arrastrado = [&](ObjId id) { return std::find(arrastrados.begin(), arrastrados.end(), id) != arrastrados.end(); };
    const Rect rv = rect_ventana(a, IdVentana::INVENTARIO);
    const int x0 = rv.x, y0 = rv.y;
    const bool original = uses_original_frame(a, IdVentana::INVENTARIO);
    const Grid& gr = original ? ORIGINAL_GRID : SIMPLE_GRID;
    if (original) {
        if (draw_original_frame(a, x0, y0, IMG_INV_BACKGROUND, 6, 8, IMG_INV_FRAME, 168, 18)) a.ventana_inventario = false;
        draw_original_ornament(a, x0, y0, IMG_INV_ORNAMENT);
        // Pestañas de categoría: una imagen por pestaña elegida.
        g.gpu().dibujar(g.img(IMG_INV_TABS + a.inventory_tab), x0 + TABS_X, y0 + TABS_Y, clave_orden(CAPA_UI, 12));
        for (int i = 0; i < N_TABS; ++i) {
            const int tx = x0 + TABS_HIT_X + i * TAB_W, ty = y0 + TABS_Y;
            if (!ui::sobre(e, tx, ty, TAB_W, TAB_H)) continue;
            if (e.clic && !a.arrastrando && a.inventory_tab != i) {
                a.inventory_tab = static_cast<u8>(i);
                a.inventario_desplazamiento = 0;
            }
            if (!a.arrastrando) {
                const int w = ancho_texto(TAB_NAMES[i], std::strlen(TAB_NAMES[i]));
                g.gpu().rectangulo(e.mx + 8, e.my + 14, w + 8, ALTO_GLIFO + 4, clave_orden(CAPA_UI_TEXTO, 29), rgba(0, 0, 0, 220));
                g.texto(TAB_NAMES[i], e.mx + 12, e.my + 16, clave_orden(CAPA_UI_TEXTO, 30), rgba(255, 255, 254));
            }
        }
    } else if (ui::ventana(g, e, a.cfg.ui_simple, x0, y0, "Inventory", opacidad_ventanas(a))) {
        a.ventana_inventario = false;
    }

    // Qué muestra la grilla: en la interfaz original, la pestaña elegida con los objetos puestos primero; en la interfaz
    // simple, todo en el orden del servidor.
    Slot orden[MAX_ORDEN];
    size_t n_orden = 0;
    if (original) {
        Slot todos[MAX_ORDEN];
        const size_t n = orden_inventario(inv, todos, MAX_ORDEN);
        for (size_t i = 0; i < n; ++i)
            if (a.inventory_tab == 0 || tab_of(a, t.col<I_USO>()[todos[i]], t.col<I_GFX>()[todos[i]], nombre_item(inv, todos[i])) == a.inventory_tab) orden[n_orden++] = todos[i];
    } else {
        for (Slot i = 0; i < t.tamano() && n_orden < MAX_ORDEN; ++i) orden[n_orden++] = i;
    }

    // Barra y rueda si hay más ítems que celdas.
    const u32 filas_total = static_cast<u32>((n_orden + gr.cols - 1) / gr.cols);
    if (original)
        original_scrollbar(a, a.scroll.inventario, x0 + gr.scroll_x, y0 + gr.y, gr.rows * gr.cell, filas_total, gr.rows,
                           a.inventario_desplazamiento, sobre_inventario(a));
    else
        barra_scroll_ventana(a, a.scroll.inventario, x0 + gr.scroll_x, y0 + gr.y, gr.rows * gr.cell, filas_total, gr.rows,
                             a.inventario_desplazamiento, clave_orden(CAPA_UI, 14), sobre_inventario(a));
    const u64 kf = clave_orden(CAPA_UI, 12), ki = clave_orden(CAPA_UI, 13), kt = clave_orden(CAPA_UI_TEXTO, 12);
    const int bajo = celda_bajo_puntero(a, gr, x0, y0, n_orden);
    const Slot slot_bajo = bajo >= 0 ? orden[bajo] : SIN_SLOT;
    const size_t primero = static_cast<size_t>(a.inventario_desplazamiento) * gr.cols;
    const int CELDA = gr.cell;
    for (int c = 0; c < gr.cols * gr.rows; ++c) {
        int x = x0 + gr.x + (c % gr.cols) * CELDA, y = y0 + gr.y + (c / gr.cols) * CELDA;
        size_t i = primero + static_cast<size_t>(c);
        bool hay = i < n_orden;
        const Slot s = hay ? orden[i] : SIN_SLOT;
        bool equipado = hay && item_equipado(nombre_item(inv, s));
        if (original) {
            ++x;  // dentro de las líneas de la grilla de la imagen
            ++y;
            // 2700 es un fondo RLE opaco que se dibuja primero (debajo del tinte de hover, la marca de equipado y el ícono).
            if (equipado && t.col<I_GFX>()[s] != 0x1d1 && t.col<I_GFX>()[s] != 0x1d2) g.gpu().dibujar(g.icono(ICON_ARMED_A), x, y, kf - 1);
            if (static_cast<int>(i) == bajo) g.gpu().rectangulo(x, y, CELDA - 1, CELDA - 1, kf, rgba(90, 80, 50, 120));
            else if (equipado && !g.img(IMG_EQUIPPED_MARK).valida()) g.gpu().rectangulo(x, y, CELDA - 1, CELDA - 1, kf, rgba(74, 70, 26, 150));
            if (equipado) g.gpu().dibujar(g.img(IMG_EQUIPPED_MARK), x - 1, y - 1, kf);
        } else {
            u32 fondo = static_cast<int>(i) == bajo ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230);
            g.gpu().rectangulo(x, y, CELDA - 2, CELDA - 2, kf, equipado ? rgba(74, 70, 26, 240) : fondo);
        }
        if (!hay) continue;
        if (!original && seleccionado(a, t.ids()[s])) {  // marco celeste (solo interfaz simple)
            const u32 celeste = rgba(120, 190, 255);
            g.gpu().rectangulo(x, y, CELDA - 2, 2, ki, celeste);
            g.gpu().rectangulo(x, y + CELDA - 4, CELDA - 2, 2, ki, celeste);
            g.gpu().rectangulo(x, y, 2, CELDA - 2, ki, celeste);
            g.gpu().rectangulo(x + CELDA - 4, y, 2, CELDA - 2, ki, celeste);
        }
        if (!arrastrado(t.ids()[s]))
            draw_item_icon(g.gpu(), g.icono(t.col<I_GFX>()[s]), x + 1, y + 1, ki, t.col<I_BENDICION>()[s], t.col<I_IDENTIFICADO>()[s] != 0);
        if (original) {
            // Capas del inventario 2009: los objetos puestos / armados llevan 2700 bajo el ícono (arriba) y luego 2701 en (+1,+1) (no los íconos 0x1d1 / 0x1d2);
            // un objeto sellado (bit de estado 0x80) lleva 2702 en (+24,+24).
            const u32 gfx = t.col<I_GFX>()[s];
            if (equipado && gfx != 0x1d1 && gfx != 0x1d2) {
                g.gpu().dibujar(g.icono(ICON_ARMED_B), x + 1, y + 1, ki + 5);
            }
            if (t.col<I_BENDICION>()[s] & BLESS_SEALED_FLAG) g.gpu().dibujar(g.icono(ICON_SEALED), x + 24, y + 24, ki + 6);
        }
        if (equipado && !original) g.texto("E", x + 1, y + 1, kt, rgba(255, 230, 90));
        if (u32 n = t.col<I_CANTIDAD>()[s]; n > 1) {
            char cant[16];
            std::snprintf(cant, sizeof cant, "%u", n);
            g.texto_sombra(cant, x + CELDA - 3 - ancho_texto(cant, std::strlen(cant)), y + CELDA - 2 - ALTO_GLIFO, kt,
                           rgba(255, 255, 254));
        }
    }
    if (original) {
        // Sin pie en el marco original: la pregunta del objetivo va arriba de la ventana.
        if (a.usar_sobre) g.texto_sombra("Click the target item (Esc cancels)", x0 + 8, y0 - ALTO_GLIFO - 2, kt, rgba(255, 230, 120));
    } else {
        char pie[48];
        std::snprintf(pie, sizeof pie, "Weight %d%%   %zu items", a.mundo.jugador.peso * 100 / 240, t.tamano());
        const int pie_y = y0 + gr.y + gr.rows * CELDA + 6;
        if (a.usar_sobre) g.texto("Click the target item (Esc cancels)", x0 + gr.x, pie_y, kt, rgba(255, 230, 120));
        else g.texto(pie, x0 + gr.x, pie_y, kt, rgba(214, 190, 140));
    }
    // Ordenar es una función del fork (el inventario 2009 no lo tiene; docs/fork/AUDIT_ORIGINAL_UI.md sección 8),
    // pero se deja en las dos interfaces. En la original, junto a la X: al lado del basurero están las pestañas.
    const int sort_x = original ? x0 + 122 : x0 + gr.trash_x - 44, sort_y = original ? y0 + 21 : y0 + gr.trash_y;
    if (!a.usar_sobre && !a.arrastrando && ui::boton_texto(g, e, "Sort", sort_x, sort_y, 40, BASURA_H))
        ordenar_inventario(a);
    const bool sobre_basura = ui::sobre(e, x0 + gr.trash_x, y0 + gr.trash_y, BASURA_W, BASURA_H);
    dibujar_basurero(g, x0 + gr.trash_x - (original ? 3 : 0), y0 + gr.trash_y, sobre_basura && a.arrastrando);
    if (sobre_basura && !a.arrastrando) {
        constexpr std::string_view AYUDA = "Drag an item here to delete it";
        const int w = ancho_texto(AYUDA.data(), AYUDA.size());
        const int tx = std::min(e.mx + 12, a.gpu.ancho() - w - 8), ty = e.my + 16;
        g.gpu().rectangulo(tx - 4, ty - 3, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto(AYUDA, tx, ty, clave_orden(CAPA_UI_TEXTO, 30), rgba(255, 255, 254));
    }

    // Texto de hover del inventario original (dibujo 0x44e45b). Solo un objeto equipado con líneas de atributos recibe la
    // caja biselada (nombre ajustado a 23 columnas, atributos a 22, todo en el color del objeto); cualquier otro muestra solo
    // su nombre, con sombra, bajo la celda. La interfaz simple del fork conserva su caja lisa.
    if (bajo >= 0 && !a.arrastrando) {
        const Slot s = slot_bajo;
        char buf[256], det[1024];
        const size_t n = expandir_desc(a.desc, nombre_item(inv, s), buf, sizeof buf);
        const size_t nd = describir_atributos(atributos_item(inv, s), det, sizeof det);
        if (original) {
            const bool equipado = item_equipado(nombre_item(inv, s));
            const u32 color = original_item_color(t.col<I_IDENTIFICADO>()[s] ? (t.col<I_BENDICION>()[s] & 3) : 3);
            LineasTip tip;
            armar_tooltip_item({buf, n}, equipado ? std::string_view(det, nd) : std::string_view{}, tip);
            const int fila = bajo / gr.cols - static_cast<int>(a.inventario_desplazamiento), col = bajo % gr.cols;
            const int celda_x = x0 + gr.x + 1 + col * CELDA, celda_y = y0 + gr.y + 1 + fila * CELDA;
            const u64 kb = clave_orden(CAPA_UI_TEXTO, 30);
            if (tip.n == tip.nombre) {
                // Solo el nombre: bajo la celda, dentro de la pantalla (0x44e3c6..0x44e407).
                const int w = ancho_texto(tip.texto[0], std::strlen(tip.texto[0]));
                const int tx = std::min(celda_x, a.gpu.ancho() - 4 - w);
                const int ty = celda_y + 32 + 12 > a.gpu.alto() ? celda_y - 12 : celda_y + 32;
                for (int i = 0; i < tip.n; ++i) g.texto_sombra(tip.texto[i], tx, ty + 12 * i, kb, color);
            } else {
                const int cnt = tip.n - tip.nombre, extra = tip.nombre == 2 ? 12 : 0;
                const int h = 12 + extra + 12 * cnt + 10, gx = x0 + gr.x + 1, gy = y0 + gr.y + 1;
                int top = celda_y + 32;
                if (top > gy + 161) top = std::max(gy + 10, top - 12 * cnt - extra - 52);
                else if (top + h > gy + 330) top = gy + 330 - h;
                draw_original_hover_box(a, gx, top, 142, h);
                for (int i = 0; i < tip.n; ++i) g.texto_sombra(tip.texto[i], gx + 5, top + 5 + 12 * i, kb, color);
            }
        } else {
            size_t nd2 = nd;
            if (nd2 == 0 && !t.col<I_IDENTIFICADO>()[s]) nd2 = static_cast<size_t>(std::snprintf(det, sizeof det, "Unidentified"));
            const u32 color = rgba(255, 255, 254);
            int w = ancho_texto(buf, n), lineas = 0;
            for (std::string_view resto(det, nd2); !resto.empty(); ++lineas) {
                const size_t fin = std::min(resto.find('\n'), resto.size());
                w = std::max(w, ancho_texto(resto.data(), fin));
                resto.remove_prefix(std::min(fin + 1, resto.size()));
            }
            const int alto_linea = ALTO_GLIFO + 3;
            int tx = std::min(e.mx + 12, a.gpu.ancho() - w - 8);
            int ty = std::min(e.my + 16, a.gpu.alto() - (lineas + 1) * alto_linea - 8);
            g.gpu().rectangulo(tx - 4, ty - 3, w + 8, (lineas + 1) * alto_linea + 4, clave_orden(CAPA_UI_TEXTO, 29), rgba(0, 0, 0, 230));
            g.texto({buf, n}, tx, ty, clave_orden(CAPA_UI_TEXTO, 30), color);
            int y = ty + alto_linea;
            for (std::string_view resto(det, nd2); !resto.empty(); y += alto_linea) {
                const size_t fin = std::min(resto.find('\n'), resto.size());
                g.texto(resto.substr(0, fin), tx, y, clave_orden(CAPA_UI_TEXTO, 30), rgba(200, 190, 160));
                resto.remove_prefix(std::min(fin + 1, resto.size()));
            }
        }
    }

    // Arrastrar: empieza al apretar sobre un ítem; al soltarlo sobre el mundo (fuera de toda
    // ventana) va al suelo.
    // Pergamino esperando su objetivo: el clic en un ítem lo usa sobre él; Esc o un clic fuera cancela.
    if (a.usar_sobre) {
        if (t.buscar(a.usar_sobre) == SIN_SLOT) a.usar_sobre = 0;  // se gastó o desapareció
        if (e.escape) {
            a.usar_sobre = 0;
            e.escape = false;
        } else if (e.pulsado) {
            a.usar_sobre_armado = true;
        }
        // Solo un clic que empezó después de usar el pergamino elige el objetivo: si no, el soltar del doble clic que
        // lo usó elegiría el pergamino mismo.
        if (a.usar_sobre && e.clic && a.usar_sobre_armado) {
            if (bajo >= 0) enviar(a, cmd::usar_item_sobre(a.op, a.usar_sobre, t.ids()[slot_bajo]));
            a.usar_sobre = 0;
            a.arrastrando = 0;
            return;
        }
        return;  // mientras tanto no se arrastra ni se usa otra cosa
    }
    if (e.clic && bajo >= 0 && clic_item_tienda(a, t.ids()[slot_bajo])) return;  // armando la tienda
    if (original) {
        // Manejo original del mouse (0x44f240): un doble clic izquierdo usa el objeto; una presión solo arma un arrastre que empieza
        // cuando el puntero se aleja 25 px; no hay acciones con clic derecho en la grilla.
        if (e.pulsado && bajo >= 0) {
            const ObjId id = t.ids()[slot_bajo];
            if (e.doble_izquierdo) {
                if (a.intercambio.abierto) ofrecer_en_intercambio(a, id);  // en un intercambio se ofrece
                else usar_item(a, id);
                a.inv_press = 0;
            } else {
                a.inv_press = id;
                a.inv_press_x = e.mx;
                a.inv_press_y = e.my;
            }
        }
        if (a.inv_press && !a.arrastrando && e.presionado) {
            const int dx = e.mx - a.inv_press_x, dy = e.my - a.inv_press_y;
            if (dx * dx + dy * dy >= DRAG_START_PX * DRAG_START_PX) a.arrastrando = a.inv_press;
        }
        if (!e.presionado) a.inv_press = 0;
    } else if (e.pulsado && bajo >= 0) {
        a.arrastrando = t.ids()[slot_bajo];
    }
    if (a.arrastrando && e.presionado) {
        Slot s = t.buscar(a.arrastrando);
        if (s != SIN_SLOT)
            draw_item_icon(g.gpu(), g.icono(t.col<I_GFX>()[s]), e.mx - 16, e.my - 16, clave_orden(CAPA_ARRASTRE, 0),
                           t.col<I_BENDICION>()[s], t.col<I_IDENTIFICADO>()[s] != 0);
        if (arrastrados.size() > 1) {
            char cant[16];
            std::snprintf(cant, sizeof cant, "x%zu", arrastrados.size());
            g.texto_sombra(cant, e.mx + 10, e.my + 6, clave_orden(CAPA_ARRASTRE, 1), rgba(120, 190, 255));
        }
    }
    if (e.clic) {
        if (a.arrastrando && sobre_basura) {
            borrar_items(a, arrastrados);  // pregunta antes
        } else if (a.arrastrando && sobre_ventana(a, IdVentana::INTERCAMBIO)) {
            for (ObjId id : arrastrados) ofrecer_en_intercambio(a, id);  // al intercambio
        } else if (a.arrastrando && casillero_bajo_puntero(a) >= 0) {
            // A la barra de atajos (Alt+número lo usa).
            Slot s = t.buscar(a.arrastrando);
            if (s != SIN_SLOT) poner_atajo(a, casillero_bajo_puntero(a), {TipoAtajo::ITEM, a.arrastrando, t.col<I_GFX>()[s]});
        } else if (a.arrastrando && tecla_f_bajo_puntero(a) >= 0) {
            // A una casilla F5–F12 del panel original.
            Slot s = t.buscar(a.arrastrando);
            if (s != SIN_SLOT) poner_tecla_f(a, tecla_f_bajo_puntero(a), {TipoAtajo::ITEM, a.arrastrando, t.col<I_GFX>()[s]});
        } else if (a.arrastrando && autobuff_bajo_puntero(a) >= 0) {
            poner_autobuff_item(a, autobuff_bajo_puntero(a), a.arrastrando);
        } else if (a.arrastrando && !sobre_alguna_ventana(a) && !sobre_barra(a)) {  // solo sobre el mundo
            // Sobre un NPC o una mascota se le da (C_GiveItem; el servidor decide si lo acepta); si
            // no, va al suelo.
            const ObjId obj = objeto_bajo_puntero(a);
            const Slot so = obj ? a.mundo.objetos.buscar(obj) : SIN_SLOT;
            const u8 tipo = so != SIN_SLOT ? tipo_objeto(a, so) : 0;
            if (so != SIN_SLOT && tipo != TIPO_PERSONAJE && tipo != TIPO_ITEM && tipo != TIPO_PUERTA) dar_items(a, obj, arrastrados);
            else for (ObjId id : arrastrados) soltar_item(a, id, arrastrados.size() == 1);
        } else if (!original && a.arrastrando && bajo >= 0 && slot_bajo != SIN_SLOT && t.ids()[slot_bajo] != a.arrastrando) {
            mover_items(a.mundo.inventario, arrastrados, t.ids()[slot_bajo]);  // a otra celda
            guardar_orden(a);
        } else if (!original && bajo >= 0 && e.shift) {
            const ObjId id = t.ids()[slot_bajo];
            if (seleccionado(a, id)) std::erase(a.inventario_seleccion, id);
            else a.inventario_seleccion.push_back(id);
            a.ultimo_clic_item = 0;  // Shift+clic no cuenta para el doble clic
        } else if (bajo >= 0 && !original) {
            a.inventario_seleccion.clear();
            // Doble clic: usar (equipar/quitar armas y armaduras, beber pociones...).
            ObjId id = t.ids()[slot_bajo];
            if (id == a.ultimo_clic_item && e.ahora_ms - a.t_ultimo_clic_item < MS_DOBLE_CLIC) {
                if (a.intercambio.abierto) ofrecer_en_intercambio(a, id);  // en un intercambio, se ofrece
                else usar_item(a, id);
                a.ultimo_clic_item = 0;
            } else {
                a.ultimo_clic_item = id;
                a.t_ultimo_clic_item = e.ahora_ms;
            }
        }
        a.arrastrando = 0;
    }
}

}  // namespace lin

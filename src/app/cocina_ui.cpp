// Original cooking window (S3EP1U.bin: ctor 0x4d43f0, global [0x8f2fd4]; recipe grid 0x4d3840 / draw 0x4d3d10, global [0x8f2fd0]).
//  - Opened by S_PacketBox 52 (COOK_WINDOW, handler 0x49135f) after a cook book (item gfx 2498..2502 = 0x9c2..0x9c6, 0x449a70) is used:
//    the same stage again toggles it closed. The book's object id is remembered ([0x8f2fb8], set where the item is used).
//  - Frame 1480 (190 x 292) at the window origin, translucent background 1481 at (6,8), ornaments 998 at (0,11) over the grid,
//    stage picture 1482 + stage at (112,265), close 969 at (165,18), drag handle (0,5,191,25), scroll bar at x 161 (up arrow y 48, track 56, down arrow 235; 0x47cd80).
//  - Recipe grid: rect (21,42)-(155,244), 4 columns x 6 rows of 34 x 34 cells, first cell at the rect corner. A cell is the dish icon (item
//    icon table, 32 x 32, normal state) from the static 24-row table at 0x6e52e8; stage n shows the first 8 * n rows (stage 3+ = all 24).
//    The scroll step is one row (4 recipes) up to the last row ([0x4d4d00]: `offset + 4 < count`).
//  - One click selects a cell (marker 536 at -1,-1); the hover box is the inventory's (0x4d3d10 = 0x44e45b): a selected cell with a text
//    gets the 142 px bevel box with the dish name (23 columns) and the effect line of string-e (22 columns), any other cell just its name.
//  - A double click (vtable slot 0x58, 0x4d39c0) on the selected cell cooks (0x4d3900): no cook book used -> string 1167; otherwise
//    C_UseItem `d book, c 1, c recipe` (l1j-en C_ItemUSe reads cookStatus, cookNo).
// Not modelled: the progress bar (0x4d4c40, set by a server message the l1j-en server never sends; string 1100 while it runs), the second
// recipe vector (cookNo >= 0x30), dragging a recipe to the shortcut bar (0x4d3ac0), the exe's name digit grouping.
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include "app/app.h"
#include "app/item_icon.h"
#include "mundo/inventario.h"

namespace lin {

namespace {

constexpr u32 IMG_FRAME = 1480, IMG_BACK = 1481, IMG_CLOSE = 969, IMG_ORNAMENT = 998, IMG_MARK = 536, IMG_STAGE = 1482;
constexpr int WINDOW_W = 190, WINDOW_H = 292, CLOSE_X = 165, CLOSE_Y = 18;
constexpr int GRID_X = 21, GRID_Y = 42, GRID_W = 134, GRID_H = 202, CELL = 34, COLS = 4, ROWS = 6;
constexpr int SCROLL_X = 161, SCROLL_Y = 48, SCROLL_H = 199;
constexpr int RECIPES = 24;
constexpr int MSG_NO_COOK_BOOK = 1167;  // 0x48f: no cook book has been used

struct Row {
    u16 gfx;
    u16 effect;  // string-e line of the effect text
};
// Static table at 0x6e52e8 (24 x 12 bytes: u16 gfx, u16 0, u32 string-e index, u32 tier).
constexpr Row TABLE[RECIPES] = {
    {2612, 1144}, {2578, 1145}, {2609, 1146}, {2576, 1147}, {2610, 1148}, {2611, 1149}, {2580, 1150}, {2582, 1151},
    {2723, 1211}, {2721, 1212}, {2726, 1213}, {2725, 1214}, {2720, 1215}, {2722, 1216}, {2719, 1217}, {2724, 1218},
    {2949, 1394}, {2942, 1395}, {2948, 1396}, {2943, 1397}, {2945, 1398}, {2944, 1399}, {2947, 1400}, {2946, 1401}};

const u64 KEY_BACK = clave_orden(CAPA_UI, 10), KEY_FRAME = clave_orden(CAPA_UI, 11), KEY_MARK = clave_orden(CAPA_UI, 12),
          KEY_ICON = clave_orden(CAPA_UI, 14), KEY_CTRL = clave_orden(CAPA_UI, 16), KEY_TIP = clave_orden(CAPA_UI_TEXTO, 30);

// recipe.tbl: `"name" "title" cookNo type n ("ingredient" qty)*`; only cookNo < 0x30 goes to the window (0x4d3130), in file order.
void load_recipes(App& a) {
    a.cocina.nombres.clear();
    TablaTexto t;
    if (!t.cargar(a.rec, "recipe.tbl")) return;
    for (size_t i = 0; i < t.cantidad() && a.cocina.nombres.size() < RECIPES; ++i) {
        const std::string_view line = t.linea(i);
        const size_t q1 = line.find('"');
        const size_t q2 = q1 == std::string_view::npos ? q1 : line.find('"', q1 + 1);
        const size_t q3 = q2 == std::string_view::npos ? q2 : line.find('"', q2 + 1);
        const size_t q4 = q3 == std::string_view::npos ? q3 : line.find('"', q3 + 1);
        if (q4 == std::string_view::npos) continue;
        int cook_no = -1;
        const std::string_view rest = line.substr(q4 + 1);
        const size_t d = rest.find_first_of("0123456789");
        if (d != std::string_view::npos) std::from_chars(rest.data() + d, rest.data() + rest.size(), cook_no);
        if (cook_no < 0 || cook_no >= 0x30) continue;
        a.cocina.nombres.emplace_back(line.substr(q1 + 1, q2 - q1 - 1));
    }
}

int visible_recipes(const App& a) {
    return std::min({RECIPES, 8 * static_cast<int>(a.cocina.modo), static_cast<int>(a.cocina.nombres.size())});
}

}  // namespace

bool cocina_abierta(const App& a) { return !a.cfg.ui_simple && a.cocina.abierta; }

// Default place (0x4d43f0): right-aligned in the 640 x 480 block, y 0 (the base object starts at 0 and the ctor adds 640 - frame width).
Rect rect_cocina(const App& a) { return {(a.gpu.ancho() - 640) / 2 + 640 - WINDOW_W, 0, WINDOW_W, WINDOW_H}; }

void eventos_cocina(App& a, const Evento& e) {
    if (e.tipo != Ev::COCINA || e.v[0] != 1) return;  // sub 0 makes the exe answer with C_ItemUse; l1j-en never sends it
    const u8 mode = static_cast<u8>(e.v[1] + 1);
    if (a.cocina.abierta && a.cocina.modo == mode) {
        a.cocina.abierta = false;  // the same book again closes it
        return;
    }
    a.cocina.abierta = true;
    a.cocina.modo = mode;
    a.cocina.sel = -1;
    a.cocina.desplaza = 0;
    a.cocina.barra = {};
    load_recipes(a);
}

void dibujar_cocina(App& a) {
    if (!cocina_abierta(a)) return;
    arrastrar_ventana(a, IdVentana::COCINA);
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const Rect r = rect_ventana(a, IdVentana::COCINA);
    g.gpu().dibujar(g.img(IMG_BACK), r.x + 6, r.y + 8, KEY_BACK, rgba(255, 255, 255, opacidad_ventanas(a)));
    g.gpu().dibujar(g.img(IMG_FRAME), r.x, r.y, KEY_FRAME);
    const bool over_close = ui::sobre(e, r.x + CLOSE_X, r.y + CLOSE_Y, 14, 14);
    g.gpu().dibujar(g.img(IMG_CLOSE), r.x + CLOSE_X, r.y + CLOSE_Y, KEY_CTRL, over_close ? rgba(255, 255, 255) : rgba(200, 200, 200));
    if ((over_close && e.clic) || e.escape) {
        a.cocina.abierta = false;
        e.escape = false;
        return;
    }
    const bool over_window = sobre_ventana(a, IdVentana::COCINA);
    const int count = visible_recipes(a);
    // Scroll in rows of 4: the exe allows `offset + 4 < count` over all 24 recipes (5 steps).
    original_scrollbar(a, a.cocina.barra, r.x + SCROLL_X, r.y + SCROLL_Y, SCROLL_H, (RECIPES - 1) / COLS + 1, 1, a.cocina.desplaza,
                       over_window, 0, -2);
    const int first = static_cast<int>(a.cocina.desplaza) * COLS;

    // Pointer over the grid -> hovered recipe (0x44f240: index = offset + row * 4 + column).
    const int gx = r.x + GRID_X, gy = r.y + GRID_Y;
    int hover = -1;
    if (over_window && ui::sobre(e, gx, gy, GRID_W, GRID_H)) {
        const int idx = first + (e.my - gy) / CELL * COLS + (e.mx - gx) / CELL;
        if ((e.mx - gx) / CELL < COLS && idx < count) hover = idx;
    }
    if (hover >= 0 && e.clic) a.cocina.sel = hover;
    if (hover >= 0 && hover == a.cocina.sel && e.doble_izquierdo) {
        if (a.cocina.pan == 0) {
            a.chat.agregar(a.mensajes.linea(MSG_NO_COOK_BOOK), rgba(255, 255, 255), CHAT_SISTEMA);  // 0x4d3900, white
        } else {
            enviar(a, cmd::cocinar(a.op, a.cocina.pan, static_cast<u8>(hover)));
        }
    }

    // Cells: up to 24 from the offset, packed (hidden recipes are skipped without taking a cell, and they come last anyway).
    for (int k = 0; k < RECIPES && first + k < count; ++k) {
        const int i = first + k;
        const int x = gx + k % COLS * CELL, y = gy + k / COLS * CELL;
        if (i == a.cocina.sel) g.gpu().dibujar(g.img(IMG_MARK), x - 1, y - 1, KEY_MARK);
        draw_item_icon(g.gpu(), g.icono(TABLE[i].gfx), x, y, KEY_ICON, 1, true);
    }
    g.gpu().dibujar(g.img(IMG_STAGE + a.cocina.modo), r.x + 112, r.y + 265, KEY_CTRL);  // 4d4840: image 1482 + stage at (112,265)
    draw_original_ornament(a, r.x, r.y, IMG_ORNAMENT);

    if (hover >= 0) {
        char name[160];
        const size_t n = expandir_desc(a.desc, a.cocina.nombres[static_cast<size_t>(hover)], name, sizeof name);
        const bool selected = hover == a.cocina.sel;
        const std::string_view effect = selected ? a.mensajes.linea(TABLE[hover].effect) : std::string_view{};
        LineasTip tip;
        armar_tooltip_item({name, n}, effect, tip);
        const u32 color = original_item_color(1);
        const int k = hover - first, cell_x = gx + k % COLS * CELL, cell_y = gy + k / COLS * CELL;
        if (tip.n == tip.nombre) {
            // Name only, under the cell and kept inside the screen (0x4d3d10).
            const int w = ancho_texto(tip.texto[0], std::strlen(tip.texto[0]));
            const int tx = std::min(cell_x, a.gpu.ancho() - 4 - w);
            const int ty = cell_y + 32 + 12 > a.gpu.alto() ? cell_y - 12 : cell_y + 32;
            for (int i = 0; i < tip.n; ++i) g.texto_sombra(tip.texto[i], tx, ty + 12 * i, KEY_TIP, color);
        } else {
            const int lines = tip.n - tip.nombre, extra = tip.nombre == 2 ? 12 : 0;
            const int h = 12 + extra + 12 * lines + 10;
            int top = cell_y + 32;
            if (top > gy + 161) top = std::max(gy + 10, top - 12 * lines - extra - 52);
            else if (top + h > gy + 330) top = gy + 330 - h;
            draw_original_hover_box(a, gx, top, 142, h);
            for (int i = 0; i < tip.n; ++i) g.texto_sombra(tip.texto[i], gx + 5, top + 5 + 12 * i, KEY_TIP, color);
        }
    }
}

}  // namespace lin

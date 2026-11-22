// Ventana original de cocina (S3EP1U.bin: ctor 0x4d43f0, global [0x8f2fd4]; grilla de recetas 0x4d3840 / dibujo 0x4d3d10, global [0x8f2fd0]).
//  - La abre S_PacketBox 52 (COOK_WINDOW, manejador 0x49135f) tras usar un libro de cocina (gfx de objeto 2498..2502 = 0x9c2..0x9c6, 0x449a70):
//    la misma etapa otra vez la cierra. Se recuerda el id de objeto del libro ([0x8f2fb8], se fija donde se usa el objeto).
//  - Marco 1480 (190 x 292) en el origen de la ventana, fondo translúcido 1481 en (6,8), adornos 998 en (0,11) sobre la grilla,
//    imagen de etapa 1482 + etapa en (112,265), cerrar 969 en (165,18), zona de arrastre (0,5,191,25), barra de scroll en x 161 (flecha arriba y 48, riel 56, flecha abajo 235; 0x47cd80).
//  - Grilla de recetas: rect (21,42)-(155,244), 4 columnas x 6 filas de celdas de 34 x 34, la primera en la esquina del rect. Una celda es el ícono del plato (tabla
//    de íconos de objetos, 32 x 32, estado normal) de la tabla estática de 24 filas en 0x6e52e8; la etapa n muestra las primeras 8 * n filas (etapa 3+ = las 24).
//    El paso de scroll es una fila (4 recetas) hasta la última fila ([0x4d4d00]: `offset + 4 < count`).
//  - Un clic elige una celda (marcador 536 en -1,-1); la caja de hover es la del inventario (0x4d3d10 = 0x44e45b): una celda elegida con texto
//    recibe la caja biselada de 142 px con el nombre del plato (23 columnas) y la línea de efecto de string-e (22 columnas); cualquier otra, solo su nombre.
//  - Un doble clic (slot de vtable 0x58, 0x4d39c0) en la celda elegida cocina (0x4d3900): sin libro usado -> texto 1167; si no
//    C_UseItem `d libro, c 1, c receta` (C_ItemUSe de l1j-en lee cookStatus, cookNo).
// Sin modelar: la barra de progreso (0x4d4c40, la fija un mensaje del servidor que l1j-en nunca manda; texto 1100 mientras corre), el segundo
// vector de recetas (cookNo >= 0x30), arrastrar una receta a la barra de atajos (0x4d3ac0), la agrupación de dígitos del nombre del exe.
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
constexpr int GRID_X = 21, GRID_Y = 42, GRID_W = 134, GRID_H = 202, CELL = 34, COLS = 4;  // 6 filas visibles
constexpr int SCROLL_X = 161, SCROLL_Y = 48, SCROLL_H = 199;
constexpr int RECIPES = 24;
constexpr int MSG_NO_COOK_BOOK = 1167;  // 0x48f: no cook book has been used

struct Row {
    u16 gfx;
    u16 effect;  // línea de string-e del texto de efecto
};
// Static table at 0x6e52e8 (24 x 12 bytes: u16 gfx, u16 0, u32 string-e index, u32 tier).
constexpr Row TABLE[RECIPES] = {
    {2612, 1144}, {2578, 1145}, {2609, 1146}, {2576, 1147}, {2610, 1148}, {2611, 1149}, {2580, 1150}, {2582, 1151},
    {2723, 1211}, {2721, 1212}, {2726, 1213}, {2725, 1214}, {2720, 1215}, {2722, 1216}, {2719, 1217}, {2724, 1218},
    {2949, 1394}, {2942, 1395}, {2948, 1396}, {2943, 1397}, {2945, 1398}, {2944, 1399}, {2947, 1400}, {2946, 1401}};

const u64 KEY_BACK = clave_orden(CAPA_UI, 10), KEY_FRAME = clave_orden(CAPA_UI, 11), KEY_MARK = clave_orden(CAPA_UI, 12),
          KEY_ICON = clave_orden(CAPA_UI, 14), KEY_CTRL = clave_orden(CAPA_UI, 16), KEY_TIP = clave_orden(CAPA_UI_TEXTO, 30);

// recipe.tbl: `"nombre" "título" cookNo tipo n ("ingrediente" cant)*`; solo cookNo < 0x30 va a la ventana (0x4d3130), en el orden del archivo.
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

// Lugar por omisión (0x4d43f0): alineada a la derecha en el bloque de 640 x 480, y 0 (el objeto base empieza en 0 y el ctor suma 640 - ancho del marco).
Rect rect_cocina(const App& a) { return {(a.gpu.ancho() - 640) / 2 + 640 - WINDOW_W, 0, WINDOW_W, WINDOW_H}; }

void eventos_cocina(App& a, const Evento& e) {
    if (e.tipo != Ev::COCINA || e.v[0] != 1) return;  // el sub 0 hace que el exe responda con C_ItemUse; l1j-en nunca lo manda
    const u8 mode = static_cast<u8>(e.v[1] + 1);
    if (a.cocina.abierta && a.cocina.modo == mode) {
        a.cocina.abierta = false;  // el mismo libro otra vez la cierra
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
    // Scroll en filas de 4: el exe permite `offset + 4 < count` sobre las 24 recetas (5 pasos).
    original_scrollbar(a, a.cocina.barra, r.x + SCROLL_X, r.y + SCROLL_Y, SCROLL_H, (RECIPES - 1) / COLS + 1, 1, a.cocina.desplaza,
                       over_window, 0, -2);
    const int first = static_cast<int>(a.cocina.desplaza) * COLS;

    // Puntero sobre la grilla -> receta bajo el puntero (0x44f240: índice = offset + fila * 4 + columna).
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

    // Celdas: hasta 24 desde el offset, juntas (las recetas ocultas se saltan sin ocupar celda, y de todos modos van al final).
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
            // Solo el nombre, bajo la celda y dentro de la pantalla (0x4d3d10).
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

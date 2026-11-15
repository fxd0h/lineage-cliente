// Framed windows of the original client (original UI only). Each window is a stack of Sprite.pak
// images at fixed offsets from its corner, as drawn by the 2009 US client (S3EP1U.bin):
//  - inventory (0x44d9a0): background 996 at (6,8), frame 997, category tabs 999+tab at (17,249),
//    ornaments 998 at (0,11) over the items, trash can 1003/1004 at (163,252);
//  - spellbook (0x4af3a0, see hechizos_ui.cpp): background 933 at (8,10), frame 934, ornaments 935 at (2,10);
//  - status (0x4b4a00): background 1440 (equipment) or 982 (attributes) at (6,9), frame 1441 or 983,
//    tab strip 985/986 at (22,248), ornaments 984 at (0,11).
// The scroll bar uses the track 971 with arrows 1206–1209 and thumb 1211; close is the 1062 button.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr u32 IMG_CLOSE = 1062;
constexpr u32 IMG_TRACK = 971, IMG_UP = 1206, IMG_UP_LIT = 1207, IMG_DOWN = 1208, IMG_DOWN_LIT = 1209, IMG_THUMB = 1211;
constexpr int CLOSE_X = ORIGINAL_WINDOW_W - 26, CLOSE_Y = 6, CLOSE_SIDE = 13;
constexpr int ARROW_H = 10, ARROW_DOWN_H = 12, THUMB_W = 15, THUMB_H = 16;

const u64 KEY_BACKGROUND = clave_orden(CAPA_UI, 10), KEY_FRAME = clave_orden(CAPA_UI, 11),
          KEY_ORNAMENT = clave_orden(CAPA_UI, 15), KEY_CONTROL = clave_orden(CAPA_UI, 16);

}  // namespace

bool uses_original_frame(const App& a, IdVentana v) {
    return !a.cfg.ui_simple && (v == IdVentana::INVENTARIO || v == IdVentana::ESTADO || v == IdVentana::HECHIZOS);
}

bool draw_original_frame(App& a, int x, int y, u32 background, int bg_x, int bg_y, u32 frame) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    g.gpu().dibujar(g.img(background), x + bg_x, y + bg_y, KEY_BACKGROUND, rgba(255, 255, 255, std::max<u8>(opacidad_ventanas(a), 160)));
    g.gpu().dibujar(g.img(frame), x, y, KEY_FRAME);
    const bool over = ui::sobre(e, x + CLOSE_X - 2, y + CLOSE_Y - 2, CLOSE_SIDE + 4, CLOSE_SIDE + 4);
    g.gpu().dibujar(g.img(IMG_CLOSE), x + CLOSE_X, y + CLOSE_Y, KEY_CONTROL, over ? rgba(255, 255, 255) : rgba(200, 200, 200));
    return over && e.clic;
}

void draw_original_ornament(App& a, int x, int y, u32 ornament) {
    a.g->gpu().dibujar(a.g->img(ornament), x, y + 11, KEY_ORNAMENT);
}

void original_scrollbar(App& a, ui::BarraScroll& st, int x, int y, int height, u32 total, u32 visible, u32& pos,
                        bool over_list) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    g.gpu().dibujar(g.img(IMG_TRACK), x, y + ARROW_H, KEY_FRAME);
    const int track_y = y + ARROW_H, track_h = height - ARROW_H - ARROW_DOWN_H;
    const bool over_up = ui::sobre(e, x, y, 12, ARROW_H), over_down = ui::sobre(e, x, y + height - ARROW_DOWN_H, 12, ARROW_DOWN_H);
    g.gpu().dibujar(g.img(over_up ? IMG_UP_LIT : IMG_UP), x, y, KEY_CONTROL);
    g.gpu().dibujar(g.img(over_down ? IMG_DOWN_LIT : IMG_DOWN), x, y + height - ARROW_DOWN_H, KEY_CONTROL);
    if (total <= visible) {
        st = {};
        pos = 0;
        return;
    }
    const u32 max = total - visible;
    u32 p = std::min(pos, max);
    if (over_list && e.rueda) p = static_cast<u32>(std::clamp<i64>(static_cast<i64>(p) - e.rueda, 0, max));
    if (e.clic && !st.arrastrando) {
        if (over_up && p > 0) --p;
        if (over_down && p < max) ++p;
    }
    const int free = track_h - THUMB_H;
    const int thumb_x = x + (12 - THUMB_W) / 2, thumb_y = track_y + static_cast<int>(static_cast<i64>(free) * p / max);
    if (e.pulsado && ui::sobre(e, x, track_y, 12, track_h)) {
        st.presionada = true;
        st.arrastrando = true;
        st.agarre = ui::sobre(e, thumb_x, thumb_y, THUMB_W, THUMB_H) ? e.my - thumb_y : THUMB_H / 2;
    }
    if (st.arrastrando && e.presionado && free > 0) {
        const int top = std::clamp(e.my - st.agarre - track_y, 0, free);
        p = static_cast<u32>((static_cast<i64>(top) * max + free / 2) / free);
    }
    if (!e.presionado) st = {};
    if (st.presionada) a.clic_consumido = true;
    pos = p;
    g.gpu().dibujar(g.img(IMG_THUMB), thumb_x, track_y + static_cast<int>(static_cast<i64>(free) * p / max), KEY_CONTROL);
}

namespace {

constexpr u32 IMG_EQUIPMENT = 1440, IMG_EQUIPMENT_FRAME = 1441, IMG_ATTRIBUTES = 982, IMG_ATTRIBUTES_FRAME = 983;
constexpr u32 IMG_STATUS_ORNAMENT = 984, IMG_STATUS_TABS = 985;  // 985 equipment selected, 986 attributes
constexpr int STATUS_BG_X = 6, STATUS_BG_Y = 9, STATUS_TABS_X = 22, STATUS_TABS_Y = 248, STATUS_TAB_W = 35, STATUS_TAB_H = 18;

// Magic level by class (prince to illusionist), as the server's L1*ClassFeature.getMagicLevel.
int magic_level(int cls, int level) {
    switch (cls) {
    case 0: return std::min(2, level / 10);
    case 1: return level / 50;
    case 2: return std::min(6, level / 8);
    case 3: return std::min(10, level / 4);
    case 4: return std::min(2, level / 12);
    case 5: return std::min(4, level / 9);
    case 6: return std::min(10, level / 6);
    default: return 0;
    }
}

// The attributes image (982): values in the boxes printed on it, coordinates measured on the image.
void draw_attributes(App& a, int x, int y) {
    Graficos& g = *a.g;
    const EstadoJugador& j = a.mundo.jugador;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    const u32 color = rgba(255, 255, 254);
    char t[48];
    auto at = [&](int tx, int ty) { g.texto_sombra(t, static_cast<float>(x + tx), static_cast<float>(y + ty), kt, color); };
    // Centered in the box [bx, bx + bw) whose top is by (boxes are 13 px tall).
    auto boxed = [&](int bx, int by, int bw) {
        const int w = ancho_texto(t, std::strlen(t));
        at(bx + (bw - w) / 2, by + (13 - ALTO_GLIFO) / 2 + 1);
    };
    auto num = [&](i32 v) { std::snprintf(t, sizeof t, "%d", v); };
    std::snprintf(t, sizeof t, "%d  (%.2f%%)", j.nivel, porcentaje_exp(j.nivel, j.exp));
    at(64, 35);
    std::snprintf(t, sizeof t, "%d / %d", j.hp, j.hp_max);
    at(48, 52);
    std::snprintf(t, sizeof t, "%d / %d", j.mp, j.mp_max);
    at(48, 65);
    std::snprintf(t, sizeof t, "%d%%", j.peso * 100 / 240);
    boxed(103, 83, 47);
    std::snprintf(t, sizeof t, "%d%%", j.comida * 100 / 225);
    boxed(103, 96, 47);
    const i32 left[] = {j.fue, j.des, j.con}, right[] = {j.int_, j.sab, j.car};
    for (int i = 0; i < 3; ++i) {
        num(left[i]);
        boxed(49, 115 + i * 13, 38);
        num(right[i]);
        boxed(112, 115 + i * 13, 38);
    }
    const int cls = (a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size())) ? a.pjs[a.pj_sel].v[PJ_CLASE] : -1;
    num(magic_level(cls, j.nivel));
    boxed(84, 160, 28);
    num(j.sp);
    boxed(84, 173, 28);
    const i32 elements[] = {j.tierra, j.agua, j.fuego, j.viento};  // earth, water | fire, wind
    for (int i = 0; i < 4; ++i) {
        num(elements[i]);
        boxed(i < 2 ? 56 : 123, 195 + (i % 2) * 12, 20);
    }
}

}  // namespace

void draw_original_status(App& a, int x, int y) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const bool equipment = a.pestana_estado == 1;
    if (draw_original_frame(a, x, y, equipment ? IMG_EQUIPMENT : IMG_ATTRIBUTES, STATUS_BG_X, STATUS_BG_Y,
                            equipment ? IMG_EQUIPMENT_FRAME : IMG_ATTRIBUTES_FRAME))
        a.ventana_estado = false;
    draw_original_ornament(a, x, y, IMG_STATUS_ORNAMENT);
    // Tabs: equipment first, then attributes (one strip image per selected tab).
    g.gpu().dibujar(g.img(IMG_STATUS_TABS + (equipment ? 0 : 1)), x + STATUS_TABS_X, y + STATUS_TABS_Y, KEY_CONTROL);
    for (int i = 0; i < 2; ++i)
        if (e.clic && ui::sobre(e, x + STATUS_TABS_X + i * STATUS_TAB_W, y + STATUS_TABS_Y, STATUS_TAB_W, STATUS_TAB_H))
            a.pestana_estado = i == 0 ? 1 : 0;
    if (a.pestana_estado == 1) dibujar_equipo(a, x, y);
    else draw_attributes(a, x + STATUS_BG_X, y + STATUS_BG_Y);
}

}  // namespace lin

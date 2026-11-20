// Framed windows of the original client (original UI only). Each window is a stack of Sprite.pak
// images at fixed offsets from its corner, as drawn by the 2009 US client (S3EP1U.bin):
//  - inventory (0x44d9a0): background 996 at (6,8), frame 997, category tabs 999+tab at (17,249),
//    ornaments 998 at (0,11) over the items, trash can 1003/1004 at (163,252);
//  - spellbook (0x4af3a0, see hechizos_ui.cpp): background 933 at (8,10), frame 934, ornaments 935 at (2,10);
//  - status (0x4b4a00): background 1440 (equipment) or 982 (attributes) at (6,9), frame 1441 or 1518,
//    tab strip 985/986 at (22,248), ornaments 984 at (0,11).
// The scroll bar uses the track 971 with arrows 1206–1209 and thumb 1211; close is the 1062 button.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "app/app.h"
#include "mundo/luz.h"

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

bool draw_original_frame(App& a, int x, int y, u32 background, int bg_x, int bg_y, u32 frame, int close_x, int close_y) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    g.gpu().dibujar(g.img(background), x + bg_x, y + bg_y, KEY_BACKGROUND, rgba(255, 255, 255, opacidad_ventanas(a)));
    g.gpu().dibujar(g.img(frame), x, y, KEY_FRAME);
    if (close_x < 0) close_x = CLOSE_X, close_y = CLOSE_Y;
    const bool over = ui::sobre(e, x + close_x - 2, y + close_y - 2, CLOSE_SIDE + 4, CLOSE_SIDE + 4);
    g.gpu().dibujar(g.img(IMG_CLOSE), x + close_x, y + close_y, KEY_CONTROL, over ? rgba(255, 255, 255) : rgba(200, 200, 200));
    return over && e.clic;
}

void draw_original_ornament(App& a, int x, int y, u32 ornament) {
    a.g->gpu().dibujar(a.g->img(ornament), x, y + 11, KEY_ORNAMENT);
}

void draw_original_overlay(App& a, int x, int y, u32 image) { a.g->gpu().dibujar(a.g->img(image), x, y, KEY_ORNAMENT); }

bool draw_original_button(App& a, int x, int y, u32 image, u32 hover_image) {
    const EntradaFrame& e = a.entrada;
    const RegionAtlas base = a.g->img(image);  // copy: loading the hover image may move the cache
    const bool over = ui::sobre(e, x, y, base.w, base.h) && !sobre_cantidad(a);
    a.g->gpu().dibujar(over ? a.g->img(hover_image) : base, x, y, KEY_CONTROL);
    return over && e.clic;
}

void original_scrollbar(App& a, ui::BarraScroll& st, int x, int y, int height, u32 total, u32 visible, u32& pos,
                        bool over_list, u32 track_image, int track_dy) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    g.gpu().dibujar(g.img(track_image ? track_image : IMG_TRACK), x, y + ARROW_H + track_dy, KEY_FRAME);
    const int track_y = y + ARROW_H, track_h = height - ARROW_H - ARROW_DOWN_H;
    const bool over_up = ui::sobre(e, x, y, 12, ARROW_H), over_down = ui::sobre(e, x, y + height - ARROW_DOWN_H, 12, ARROW_DOWN_H);
    g.gpu().dibujar(g.img(over_up ? IMG_UP_LIT : IMG_UP), x, y, KEY_CONTROL);
    // The down arrow (11 px wide) is right-aligned with the up arrow and the track (12 px), as in the original
    // scroll bar class (0x47cd80): NPC dialog up (226,47), down x 227.
    g.gpu().dibujar(g.img(over_down ? IMG_DOWN_LIT : IMG_DOWN), x + 1, y + height - ARROW_DOWN_H, KEY_CONTROL);
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

constexpr u32 IMG_EQUIPMENT = 1440, IMG_EQUIPMENT_FRAME = 1441, IMG_ATTRIBUTES = 982, IMG_ATTRIBUTES_FRAME = 983,
                IMG_EQUIPMENT_FEMALE = 1516, IMG_ATTRIBUTES_FRAME_EXE = 1518;
constexpr u32 IMG_STATUS_ORNAMENT = 984, IMG_STATUS_TABS = 985;  // 985 equipment selected, 986 attributes
constexpr u32 IMG_GAUGE_BOX = 1263, IMG_GAUGE_EXP = 1264, IMG_GAUGE_HP = 1265, IMG_GAUGE_MP = 1266;
const u32 HP_TEXT = rgba(246, 189, 246), MP_TEXT = rgba(189, 222, 255);  // 0x4b4199
constexpr u32 IMG_CLOCK_DAY = 990, IMG_CLOCK_NIGHT = 993;
constexpr int GAUGE_X = 69, GAUGE_EXP_Y = 46, GAUGE_HP_Y = 62, GAUGE_MP_Y = 77;
constexpr int STATUS_BG_X = 6, STATUS_BG_Y = 9, STATUS_TABS_X = 22, STATUS_TABS_Y = 248, STATUS_TAB_W = 35;
// Hit rects of the tabs (0x4b48b6): (22,245) and (57,245), 34x17.
constexpr int STATUS_TAB_HIT_Y = 245, STATUS_TAB_HIT_W = 34, STATUS_TAB_HIT_H = 17;

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

// Magic bonus from INT, as 0x4aa700 of the original client.
int magic_bonus(int in) {
    if (in <= 2) return -3;
    if (in <= 5) return -2;
    if (in <= 8) return -1;
    if (in <= 11) return 0;
    if (in <= 14) return 1;
    if (in <= 17) return 2;
    if (in <= 24) return in - 15;
    if (in <= 35) return 10;
    if (in <= 42) return 11;
    if (in <= 49) return 12;
    if (in <= 77) return 13;
    return in - 25;
}

// Evasion rating as 0x4b0790: DEX/2 - 4 + level / class divisor (equipment/buff terms are not sent by the server).
int escape_rating(int cls, int level, int dex) {
    constexpr int DIV[] = {8, 4, 6, 10, 4, 7, 9};  // prince, knight, elf, wizard, dark elf, dragon knight, illusionist
    const int d = (cls >= 0 && cls < 7) ? DIV[cls] : 8;
    return dex / 2 - 4 + level / d;
}

// The attributes image (982): values in the boxes printed on it, coordinates measured on the image.
void draw_attributes(App& a, int x, int y) {
    Graficos& g = *a.g;
    const EstadoJugador& j = a.mundo.jugador;
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    const u32 color = rgba(255, 255, 254);
    char t[48];
    const int cls = (a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size())) ? a.pjs[a.pj_sel].v[PJ_CLASE] : -1;
    // 0x4b3740 / 0x4b38b0: class 3 (wizard) draws every number one pixel higher.
    int lift = cls == 3 ? 1 : 0;
    const int wx = x - STATUS_BG_X, wy = y - STATUS_BG_Y;  // window origin; the RE coordinates are relative to it
    auto at = [&](int tx, int ty, u32 c) { g.texto_sombra(t, static_cast<float>(wx + tx), static_cast<float>(wy + ty), kt, c); };
    // Centered in the box [bx, bx + bw) whose top is by (boxes are 13 px tall), in bg-relative coordinates.
    auto boxed = [&](int bx, int by, int bw) {
        const int w = ancho_texto(t, std::strlen(t));
        g.texto_sombra(t, static_cast<float>(x + bx + (bw - w) / 2), static_cast<float>(y + by + (13 - ALTO_GLIFO) / 2 + 1 - lift),
                       kt, color);
    };
    auto num = [&](i32 v) { std::snprintf(t, sizeof t, "%d", v); };
    // Gauge box 1263 at (69,46) with the fills 1264 (EXP, y 46), 1265 (HP, 62) and 1266 (MP, 77) clipped to the value
    // (0x4b4a00); a fill is drawn only when its maximum is positive. EXP uses the floored percentage.
    const u64 kg = clave_orden(CAPA_UI, 12), kf = clave_orden(CAPA_UI, 13);
    g.gpu().dibujar(g.img(IMG_GAUGE_BOX), wx + GAUGE_X, wy + GAUGE_EXP_Y, kg);
    auto fill = [&](u32 image, int fy, i64 value, i64 max) {
        RegionAtlas r = g.img(image);
        if (max <= 0 || !r.valida()) return;
        const int w = static_cast<int>(std::clamp<i64>(r.w * value / max, 0, r.w));
        if (w <= 0) return;
        r.w = static_cast<u16>(w);
        g.gpu().dibujar(r, wx + GAUGE_X, wy + fy, kf);
    };
    const int exp_pct = static_cast<int>(porcentaje_exp(j.nivel, j.exp));
    fill(IMG_GAUGE_EXP, GAUGE_EXP_Y, exp_pct, 100);
    fill(IMG_GAUGE_HP, GAUGE_HP_Y, j.hp, j.hp_max);
    fill(IMG_GAUGE_MP, GAUGE_MP_Y, j.mp, j.mp_max);
    num(j.nivel);
    at(100, 50, color);
    const float exp_exact = porcentaje_exp(j.nivel, j.exp);
    std::snprintf(t, sizeof t, "%02d.%02d%%", exp_pct, static_cast<int>((exp_exact - static_cast<float>(exp_pct)) * 100.0f));
    at(115, 50, color);
    std::snprintf(t, sizeof t, "%d/%d", j.hp, j.hp_max);
    at(100, 66, HP_TEXT);
    std::snprintf(t, sizeof t, "%d/%d", j.mp, j.mp_max);
    at(100, 80, MP_TEXT);
    // Weight and food percentages: centred in x 107..150 at y 94 / 107 (0x4b38b0).
    auto centred = [&](int ty) {
        const int w = ancho_texto(t, std::strlen(t));
        g.texto_sombra(t, static_cast<float>(wx + 107 + (43 - w) / 2), static_cast<float>(wy + ty - lift), kt, color);
    };
    std::snprintf(t, sizeof t, "%d%%", j.peso * 100 / 240);
    centred(94);
    std::snprintf(t, sizeof t, "%d%%", j.comida * 100 / 225);
    centred(107);
    const i32 left[] = {j.fue, j.des, j.con}, right[] = {j.int_, j.sab, j.car};
    for (int i = 0; i < 3; ++i) {
        num(left[i]);
        boxed(49, 115 + i * 13, 38);
        num(right[i]);
        boxed(112, 115 + i * 13, 38);
    }
    lift = 0;  // the pair, alignment and element numbers use other routines
    num(magic_level(cls, j.nivel));
    boxed(84, 160, 28);
    num(magic_bonus(j.int_));  // 0x4aa700: from INT only
    boxed(84, 173, 28);
    // Alignment (S3EP1U 0x4b4cb2 / 0x4b3d44): the word image 987 chaotic (< 0) / 988 (0..499) / 989 lawful (0x4b8520) at
    // (121,167) of the window, and the value in the wide box below it in the character's name colour (0x4cc140).
    g.gpu().dibujar(g.img(j.legal < 0 ? 987 : j.legal < 500 ? 988 : 989), x + 115, y + 158, KEY_CONTROL);
    num(j.legal);
    {
        const Slot yo = a.mundo.objetos.buscar(j.id);
        const bool pink = yo != SIN_SLOT && a.entrada.ahora_ms < a.mundo.objetos.col<O_ROSA>()[yo];
        const u32 name_color = pink ? rgb555(0x767d) : color_alineamiento(j.legal);
        const int w = ancho_texto(t, std::strlen(t));
        g.texto_sombra(t, static_cast<float>(x + 115 + (38 - w) / 2), static_cast<float>(y + 173 + (13 - ALTO_GLIFO) / 2 + 1), kt, name_color);
    }
    num(escape_rating(cls, j.nivel, j.des));
    boxed(43, 222, 25);
    const i32 elements[] = {j.tierra, j.agua, j.fuego, j.viento};  // earth, water | fire, wind
    for (int i = 0; i < 4; ++i) {
        num(elements[i]);
        boxed(i < 2 ? 56 : 123, 195 + (i % 2) * 12, 20);
    }
    // Day / night clock gauge (0x4b2e30): cap 990 / 993 at (76,233), bar 991 / 994 at (83,232) and the marker 992 / 995 at
    // (83 + v, 233), v = elapsed fraction of the daylight or night period * 83, from the game clock.
    {
        const i32 clock = hora_de_juego(a);
        const int seconds = static_cast<int>(((clock % 86400) + 86400) % 86400);
        const DayPhase ph = day_phase(seconds / 60, dia_del_anio(clock));
        const u32 base = ph.night ? IMG_CLOCK_NIGHT : IMG_CLOCK_DAY;
        const int v = ph.length > 0 ? std::clamp(ph.elapsed * 83 / ph.length, 0, 83) : 0;
        const u64 kc = clave_orden(CAPA_UI, 14);
        g.gpu().dibujar(g.img(base), wx + 76, wy + 233, kc);
        g.gpu().dibujar(g.img(base + 1), wx + 83, wy + 232, kc);
        g.gpu().dibujar(g.img(base + 2), wx + 83 + v, wy + 233, kc);
    }
}

}  // namespace

void draw_original_status(App& a, int x, int y) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const bool equipment = a.pestana_estado == 1;
    // 0x4b4a00: the equipment background is 1440 + 0x4c (1516) when the selected character's sex byte ([0x81b328],
    // char-list entry +0x1f) is non-zero; the attributes frame is 1518 (0x5a1 + 0x4d). Where the install lacks 1516 /
    // 1518 (U-C2) the 1440 / 983 pair stands in.
    const bool female = a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size()) && a.pjs[static_cast<size_t>(a.pj_sel)].v[PJ_SEXO] != 0;
    const u32 eq_bg = female && g.img(IMG_EQUIPMENT_FEMALE).valida() ? IMG_EQUIPMENT_FEMALE : IMG_EQUIPMENT;
    const u32 attr_frame = g.img(IMG_ATTRIBUTES_FRAME_EXE).valida() ? IMG_ATTRIBUTES_FRAME_EXE : IMG_ATTRIBUTES_FRAME;
    if (draw_original_frame(a, x, y, equipment ? eq_bg : IMG_ATTRIBUTES, STATUS_BG_X, STATUS_BG_Y,
                            equipment ? IMG_EQUIPMENT_FRAME : attr_frame))
        a.ventana_estado = false;
    draw_original_ornament(a, x, y, IMG_STATUS_ORNAMENT);
    // Tabs: equipment first, then attributes (one strip image per selected tab).
    g.gpu().dibujar(g.img(IMG_STATUS_TABS + (equipment ? 0 : 1)), x + STATUS_TABS_X, y + STATUS_TABS_Y, KEY_CONTROL);
    for (int i = 0; i < 2; ++i)
        if (e.clic && ui::sobre(e, x + STATUS_TABS_X + i * STATUS_TAB_W, y + STATUS_TAB_HIT_Y, STATUS_TAB_HIT_W, STATUS_TAB_HIT_H))
            a.pestana_estado = i == 0 ? 1 : 0;
    if (a.pestana_estado == 1) dibujar_equipo(a, x, y);
    else draw_attributes(a, x + STATUS_BG_X, y + STATUS_BG_Y);
}

}  // namespace lin

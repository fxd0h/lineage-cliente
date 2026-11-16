// World-map items (S_UseMap, opcode USEMAP). Using a map item (ids 40373..40390) makes the server send
// the item objid and a map number (1..18). The 2009 US client (S3EP1U.bin) handles it at 0x48e0b0:
// it looks the item up in the inventory (0x44c110), stores `number - 1` in the map window object
// (global 0x816b08, class vtable 0x4f1aac) and opens it (0x4d0f30). The window (0x46ba90 draws it):
//  - the picture is Sprite.pak image table[number - 1] at 0x6de444:
//    1296 1305 1307 1309 1303 1312 1311 1302 1304 1295 1293 1306 1308 1310 1301 1294 1292 1417
//    (380x230 each, except 1294 = 1478x825 and 1301 = 500x320);
//  - there is no frame: the bare picture is clipped to a 311x215 view (the size of the mask "0.alp",
//    Sprite.pak; its soft edge alpha is not reproduced, only its size and its zero pixels), drawn at the
//    window position, which starts at 0,0 of the game surface;
//  - dragging: a left drag started inside the rect (54,45) 200x125 of the window moves the window
//    (callback 0x46afc0 adds the mouse delta to the window position; no clamping);
//  - a right button double click (WM_RBUTTONDBLCLK) inside the rect (39,30)-(245,170) of the window closes
//    it (0x46c1b0); a left or right press there only brings it to the front. There is no Escape handler;
//  - when the player's map is the one the picture shows, the picture is panned to center the player (the
//    view follows him, clamped to the picture) and the marker, image 1297 + frame, is drawn. Otherwise the
//    pan is 0 and there is no marker. The frame goes 0,1,2 every 250 ms (timer 0xfa, 0x46af90). The marker is
//    drawn only when it is strictly inside the view and the mask pixel under it is non-zero. Its position:
//    world coordinates relative to a per-picture anchor (tables below, built by 0x46acc0 and 0x46ae30):
//      dx = x - A[n], dy = y - B[n]; a = dx * 30 / 64, b = dy * 30 / 64 (truncated toward zero);
//      X = C[n] + a + b, Y = D[n] + (b - a) / 2          (picture pixels)
//    drawn with its top-left at (window + X - OX[frame] - pan_x, window + Y - image height - pan_y).
#include <algorithm>
#include <vector>
#include "app/app.h"

namespace lin {

namespace {

constexpr int MAP_COUNT = 18;
// Sprite.pak image of each map number (index = number - 1), the table at 0x6de444 of the original.
constexpr u32 MAP_IMAGE[MAP_COUNT] = {1296, 1305, 1307, 1309, 1303, 1312, 1311, 1302, 1304,
                                      1295, 1293, 1306, 1308, 1310, 1301, 1294, 1292, 1417};
// World-to-picture anchors per picture index: {A, B, C, D}. A, B: world tile x, y (0x816a78, 0x816a7c, set
// by 0x46acc0); C, D: the picture pixel of that tile (0x8169e8, 0x8169ec, set by 0x46ae30).
struct Anchor {
    int a, b, c, d;
};
constexpr Anchor ANCHOR[MAP_COUNT] = {
    {32632, 32983, 221, 93},  {32560, 32726, 125, 151}, {33014, 32785, 143, 128}, {32655, 32523, 218, 182},
    {33136, 32244, 188, 93},  {32865, 33251, 252, 74},  {32865, 33251, 38, 152},  {33136, 32244, 1, 149},
    {33505, 32735, 177, 115}, {32818, 32745, 195, 110}, {32688, 32870, 220, 110}, {33443, 33493, 200, 160},
    {33714, 32498, 264, 136}, {34035, 32178, 102, 108}, {34217, 33134, 278, 138}, {32614, 32696, 284, 562},
    {32753, 32905, 191, 129}, {32280, 33066, 98, 175}};
constexpr int MARKER_IMAGE = 1297, MARKER_FRAMES = 3, MARKER_MS = 250;
constexpr int MARKER_OX[MARKER_FRAMES] = {11, 6, 7};  // ebp-0x20.. in 0x46ba90
constexpr int DEFAULT_MASK_W = 311, DEFAULT_MASK_H = 215;
// Rects relative to the window: drag (0x46b0db args) and close/hit (0x46c1b0).
constexpr int DRAG_X = 54, DRAG_Y = 45, DRAG_W = 200, DRAG_H = 125;
constexpr int HIT_X0 = 39, HIT_Y0 = 30, HIT_X1 = 245, HIT_Y1 = 170;

struct Mask {
    int w = DEFAULT_MASK_W, h = DEFAULT_MASK_H;
    std::vector<u8> px;  // empty: every pixel counts
};

const Mask& mask(const App& a) {
    static Mask m;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        const Bytes b = a.rec.sprite("0.alp");  // u16 w, u16 h, then w*h alpha bytes
        if (b.size() >= 4) {
            const int w = b[0] | b[1] << 8, h = b[2] | b[3] << 8;
            if (w > 0 && h > 0 && b.size() >= 4 + static_cast<size_t>(w) * h) {
                m.w = w;
                m.h = h;
                m.px.assign(b.begin() + 4, b.begin() + 4 + static_cast<size_t>(w) * h);
            }
        }
    }
    return m;
}

// Does the player's map show on picture `idx`? (0x46bbb3 / 0x46bf3c)
bool player_on_picture(MapId map, int idx) {
    switch (map) {
    case 4: return idx != 9 && idx != 10 && idx != 0 && idx != 16 && idx != 17;
    case 0x44: return idx == 9;
    case 0x45: return idx == 10;
    case 0x130: return idx == 16;
    case 0: return idx == 0;
    case 0x1b8: return idx == 17;
    default: return false;
    }
}

// Truncating division of 30 * v by 64, as the original's cdq / and 0x3f / add / sar 6.
int scale30(int v) {
    const int p = v * 30;
    return (p + (p < 0 ? 63 : 0)) >> 6;
}

struct Marker {
    bool on_map = false;
    int x = 0, y = 0;  // picture pixels
};

Marker marker_of(const App& a, int idx) {
    Marker r;
    if (!player_on_picture(a.mundo.mapa, idx)) return r;
    const TablaObjetos& o = a.mundo.objetos;
    const Slot s = o.buscar(a.mundo.jugador.id);
    if (s == SIN_SLOT) return r;
    const Anchor& k = ANCHOR[idx];
    const int px = scale30(o.col<O_X>()[s] - k.a), py = scale30(o.col<O_Y>()[s] - k.b);
    const int d = py - px;
    r.on_map = true;
    r.x = k.c + py + px;
    r.y = k.d + d / 2;  // C++ division truncates toward zero, like the original
    return r;
}

}  // namespace

void eventos_mapa(App& a, const Evento& e) {
    if (e.tipo != Ev::USAR_MAPA) return;
    MapaItem& m = a.mapa_item;
    const int n = e.v[0];
    if (n < 1 || n > MAP_COUNT) return;
    m = {};  // the window opens at 0,0 of the game surface
    m.numero = n;
    m.objeto = e.id;
}

bool sobre_mapa(const App& a) {
    const MapaItem& m = a.mapa_item;
    if (!m.numero) return false;
    if (m.arrastrando) return true;
    return ui::sobre(a.entrada, m.x + HIT_X0, m.y + HIT_Y0, HIT_X1 - HIT_X0, HIT_Y1 - HIT_Y0) ||
           ui::sobre(a.entrada, m.x + DRAG_X, m.y + DRAG_Y, DRAG_W, DRAG_H);
}

void dibujar_mapa_item(App& a) {
    MapaItem& m = a.mapa_item;
    if (!m.numero) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int idx = m.numero - 1;
    const RegionAtlas full = g.img(MAP_IMAGE[idx]);  // copy: other loads may move the cache
    if (!full.valida()) return;                      // still loading
    const Mask& mk = mask(a);
    const u64 kf = clave_orden(CAPA_UI, 76);

    // Right double click inside the hit rect closes it; any press there is consumed (sobre_mapa).
    if (e.doble_derecho && ui::sobre(e, m.x + HIT_X0, m.y + HIT_Y0, HIT_X1 - HIT_X0, HIT_Y1 - HIT_Y0)) {
        m = {};
        return;
    }
    // Drag: moves the window by the mouse delta (no clamping, like 0x46afc0).
    if (e.pulsado && ui::sobre(e, m.x + DRAG_X, m.y + DRAG_Y, DRAG_W, DRAG_H)) {
        m.arrastrando = true;
        m.ult_x = e.mx;
        m.ult_y = e.my;
    }
    if (m.arrastrando && e.presionado) {
        m.x += e.mx - m.ult_x;
        m.y += e.my - m.ult_y;
        m.ult_x = e.mx;
        m.ult_y = e.my;
    }
    if (!e.presionado) m.arrastrando = false;

    // Pan: centered on the player when his map is the picture's, else 0.
    const Marker pm = marker_of(a, idx);
    int pan_x = 0, pan_y = 0;
    if (pm.on_map) {
        pan_x = std::clamp(pm.x - mk.w / 2, 0, std::max(0, full.w - mk.w));
        pan_y = std::clamp(pm.y - mk.h / 2, 0, std::max(0, full.h - mk.h));
    }
    // The visible part of the picture: a sub-rectangle of the atlas region.
    RegionAtlas part = full;
    const u32 k = full.escala_texel();
    part.x = static_cast<u16>(full.x + static_cast<u32>(pan_x) * k);
    part.y = static_cast<u16>(full.y + static_cast<u32>(pan_y) * k);
    part.w = static_cast<u16>(std::min(mk.w, full.w - pan_x));
    part.h = static_cast<u16>(std::min(mk.h, full.h - pan_y));
    part.ox = part.oy = 0;
    part.aura_dx = 0;
    g.gpu().dibujar(part, m.x, m.y, kf + 1);

    if (!pm.on_map) return;
    const int vx = pm.x - pan_x, vy = pm.y - pan_y;  // marker in view coordinates
    if (vx <= 0 || vx >= mk.w || vy <= 0 || vy >= mk.h) return;
    if (!mk.px.empty() && mk.px[static_cast<size_t>(vy) * mk.w + vx] == 0) return;
    const int frame = static_cast<int>(e.ahora_ms / MARKER_MS % MARKER_FRAMES);
    RegionAtlas mr = g.img(MARKER_IMAGE + frame);
    if (!mr.valida()) return;
    mr.ox = mr.oy = 0;
    mr.aura_dx = 0;
    g.gpu().dibujar(mr, m.x + vx - MARKER_OX[frame], m.y + vy - mr.h, kf + 2);
}

}  // namespace lin

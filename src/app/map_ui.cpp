// Objetos de mapa del mundo (S_UseMap, opcode USEMAP). Usar un objeto de mapa (ids 40373..40390) hace que el servidor mande
// el objid del objeto y un número de mapa (1..18). El cliente US 2009 (S3EP1U.bin) lo maneja en 0x48e0b0:
// busca el objeto en el inventario (0x44c110), guarda `número - 1` en el objeto de la ventana del mapa
// (global 0x816b08, vtable de clase 0x4f1aac) y la abre (0x4d0f30). La ventana (la dibuja 0x46ba90):
//  - la imagen es la de Sprite.pak table[número - 1] en 0x6de444:
//    1296 1305 1307 1309 1303 1312 1311 1302 1304 1295 1293 1306 1308 1310 1301 1294 1292 1417
//    (380x230 cada una, salvo 1294 = 1478x825 y 1301 = 500x320);
//  - no hay marco: la imagen sola se recorta a una vista de 311x215 (el tamaño de la máscara "0.alp",
//    Sprite.pak; no se reproduce su alfa de borde suave, solo su tamaño y sus píxeles en cero), dibujada en la
//    posición de la ventana, que empieza en 0,0 de la superficie de juego;
//  - arrastre: un arrastre izquierdo que empieza dentro del rect (54,45) 200x125 de la ventana mueve la ventana
//    (el callback 0x46afc0 suma el delta del mouse a la posición de la ventana; sin límites);
//  - un doble clic derecho (WM_RBUTTONDBLCLK) dentro del rect (39,30)-(245,170) de la ventana la
//    cierra (0x46c1b0); una presión izquierda o derecha ahí solo la trae al frente. No hay manejador de Escape;
//  - cuando el mapa del jugador es el que muestra la imagen, la imagen se desplaza para centrar al jugador (la
//    vista lo sigue, limitada a la imagen) y se dibuja el marcador, imagen 1297 + cuadro. Si no, el
//    desplazamiento es 0 y no hay marcador. El cuadro va 0,1,2 cada 250 ms (temporizador 0xfa, 0x46af90). El marcador se
//    dibuja solo cuando está estrictamente dentro de la vista y el píxel de la máscara debajo no es cero. Su posición:
//    coordenadas del mundo relativas a un ancla por imagen (tablas de abajo, armadas por 0x46acc0 y 0x46ae30):
//      dx = x - A[n], dy = y - B[n]; a = dx * 30 / 64, b = dy * 30 / 64 (truncado hacia cero);
//      X = C[n] + a + b, Y = D[n] + (b - a) / 2          (píxeles de la imagen)
//    dibujado con su esquina superior izquierda en (ventana + X - OX[cuadro] - pan_x, ventana + Y - alto de la imagen - pan_y).
#include <algorithm>
#include <vector>
#include "app/app.h"

namespace lin {

namespace {

constexpr int MAP_COUNT = 18;
// Imagen de Sprite.pak de cada número de mapa (índice = número - 1), la tabla en 0x6de444 del original.
constexpr u32 MAP_IMAGE[MAP_COUNT] = {1296, 1305, 1307, 1309, 1303, 1312, 1311, 1302, 1304,
                                      1295, 1293, 1306, 1308, 1310, 1301, 1294, 1292, 1417};
// Anclas mundo-imagen por índice de imagen: {A, B, C, D}. A, B: celda x, y del mundo (0x816a78, 0x816a7c, las fija
// 0x46acc0); C, D: el píxel de la imagen de esa celda (0x8169e8, 0x8169ec, los fija 0x46ae30).
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
constexpr int MARKER_OX[MARKER_FRAMES] = {11, 6, 7};  // ebp-0x20.. en 0x46ba90
constexpr int DEFAULT_MASK_W = 311, DEFAULT_MASK_H = 215;
// Rects relativos a la ventana: arrastre (argumentos de 0x46b0db) y cerrar/acierto (0x46c1b0).
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
        const Bytes b = a.rec.sprite("0.alp");  // u16 w, u16 h, después w*h bytes de alfa
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

// ¿El mapa del jugador se muestra en la imagen `idx`? (0x46bbb3 / 0x46bf3c)
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

// División truncada de 30 * v por 64, como el cdq / and 0x3f / add / sar 6 del original.
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
    r.y = k.d + d / 2;  // la división de C++ trunca hacia cero, como el original
    return r;
}

}  // namespace

void eventos_mapa(App& a, const Evento& e) {
    if (e.tipo != Ev::USAR_MAPA) return;
    MapaItem& m = a.mapa_item;
    const int n = e.v[0];
    if (n < 1 || n > MAP_COUNT) return;
    m = {};  // la ventana abre en 0,0 de la superficie de juego
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
    const RegionAtlas full = g.img(MAP_IMAGE[idx]);  // copia: otras cargas pueden mover la caché
    if (!full.valida()) return;                      // todavía se está cargando
    const Mask& mk = mask(a);
    const u64 kf = clave_orden(CAPA_UI, 76);

    // Doble clic derecho dentro del rect de acierto la cierra; cualquier presión ahí se consume (sobre_mapa).
    if (e.doble_derecho && ui::sobre(e, m.x + HIT_X0, m.y + HIT_Y0, HIT_X1 - HIT_X0, HIT_Y1 - HIT_Y0)) {
        m = {};
        return;
    }
    // Arrastre: mueve la ventana según el delta del mouse (sin límites, como 0x46afc0).
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

    // Desplazamiento: centrado en el jugador cuando su mapa es el de la imagen; si no, 0.
    const Marker pm = marker_of(a, idx);
    int pan_x = 0, pan_y = 0;
    if (pm.on_map) {
        pan_x = std::clamp(pm.x - mk.w / 2, 0, std::max(0, full.w - mk.w));
        pan_y = std::clamp(pm.y - mk.h / 2, 0, std::max(0, full.h - mk.h));
    }
    // La parte visible de la imagen a través de la máscara (bordes suaves), se rehace solo cuando se mueve la vista.
    if (!mk.px.empty()) {
        const RegionAtlas& part = g.img_mascara(MAP_IMAGE[idx], pan_x, pan_y, mk.px.data(), mk.w, mk.h);
        if (!part.valida()) return;  // still building
        g.gpu().dibujar(part, m.x, m.y, kf + 1);
    } else {
        RegionAtlas part = full;  // no mask file: a plain crop
        const u32 k = full.escala_texel();
        part.x = static_cast<u16>(full.x + static_cast<u32>(pan_x) * k);
        part.y = static_cast<u16>(full.y + static_cast<u32>(pan_y) * k);
        part.w = static_cast<u16>(std::min(mk.w, full.w - pan_x));
        part.h = static_cast<u16>(std::min(mk.h, full.h - pan_y));
        part.ox = part.oy = 0;
        part.aura_dx = 0;
        g.gpu().dibujar(part, m.x, m.y, kf + 1);
    }

    if (!pm.on_map) return;
    const int vx = pm.x - pan_x, vy = pm.y - pan_y;  // marcador en coordenadas de la vista
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

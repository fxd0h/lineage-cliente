// Floating HP/MP pair window of the original client (original UI only): built at 0x4b56c0 (vtable 0x4f53cc), drawn at
// 0x4b5c60 (S3EP1U.bin), toggled by Ctrl+R (0x456d26) and the Options action `showfloathpmp` (byte [0x6ddf90], default 1).
//  - frame image 1330 + 3 * style (style 0..2, [0x8169b0]) at the top centre of the screen (x = 320 - w / 2, y = 0);
//  - HP bar 1331 + 3 * style and MP bar 1332 + 3 * style at (20, 0), cut to hp / hpmax and mp / mpmax of the width; when
//    the character is poisoned the HP bar is image 1345 + 2 * style, when frozen 1346 + 2 * style (0x4b5d2e);
//  - "%d/%d" centred on the bar at y 6 (HP) and 23 (MP): outline colours 0x5949 / 0x3214 (RGB555) one pixel around, white
//    0x7fff in the middle (0x431390 calls);
//  - button 1339 (hover 1341) at (0,7) cycles the style (0x4b5250); close 1342 (hover 1344) at (w - 16, 12) clears the
//    option (0x4b56b0); the rest of the window is a drag handle clamped to y <= 430 (0x4b51d0).
#include <cstdio>
#include <cstring>
#include "app/app.h"

namespace lin {

namespace {

constexpr u32 IMG_FRAME = 1330, IMG_STYLE_BTN = 1339, IMG_STYLE_BTN_HOVER = 1341, IMG_CLOSE = 1342, IMG_CLOSE_HOVER = 1344;
constexpr int BAR_X = 20, HP_TEXT_Y = 6, MP_TEXT_Y = 23, MAX_Y = 0x1ae;

const u64 KEY_FRAME = clave_orden(CAPA_UI, 11), KEY_BAR = clave_orden(CAPA_UI, 12), KEY_CTRL = clave_orden(CAPA_UI, 16),
          KEY_TEXT = clave_orden(CAPA_UI_TEXTO, 16);

}  // namespace

bool hpmp_flotante_abierta(const App& a) {
    return !a.cfg.ui_simple && (a.cfg.opciones_orig >> OPC_FLOATHPMP & 1) && a.pantalla == Pantalla::JUEGO &&
           a.mundo.jugador.hp_max > 0;
}

Rect rect_hpmp_flotante(const App& a) {
    const int w = a.g ? a.g->img(IMG_FRAME + 3 * a.hpmp_estilo).w : 0, h = a.g ? a.g->img(IMG_FRAME + 3 * a.hpmp_estilo).h : 0;
    return {(a.gpu.ancho() - w) / 2, 0, w, h};
}

void dibujar_hpmp_flotante(App& a) {
    if (!hpmp_flotante_abierta(a)) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const EstadoJugador& j = a.mundo.jugador;
    Rect r = rect_ventana(a, IdVentana::HPMP);
    PosVentana& p = a.pos_ventanas[static_cast<int>(IdVentana::HPMP)];
    // Drag handle: everything right of the style button (the 0x88-byte control at (22,1) of the window).
    const bool sobre_cierre = ui::sobre(e, r.x + r.w - 16, r.y + 12, 12, 12), sobre_estilo = ui::sobre(e, r.x, r.y + 7, 22, 22);
    if (e.pulsado && a.ventana_arrastrada < 0 && ui::sobre(e, r.x + 22, r.y + 1, r.w - 22, 33) && !sobre_cierre) {
        a.ventana_arrastrada = static_cast<int>(IdVentana::HPMP);
        a.arrastre_dx = e.mx - r.x;
        a.arrastre_dy = e.my - r.y;
        a.clic_consumido = true;
    }
    if (a.ventana_arrastrada == static_cast<int>(IdVentana::HPMP)) {
        if (e.presionado) {
            p.x = e.mx - a.arrastre_dx;
            p.y = std::clamp(e.my - a.arrastre_dy, 0, MAX_Y);
            p.movida = true;
            r.x = p.x, r.y = p.y;
        } else {
            a.ventana_arrastrada = -1;
            guardar_posiciones_ventanas(a);
        }
    }
    g.gpu().dibujar(g.img(IMG_FRAME + 3 * a.hpmp_estilo), r.x, r.y, KEY_FRAME);
    const Slot yo = a.mundo.objetos.buscar(j.id);
    const u8 flags = yo != SIN_SLOT ? a.mundo.objetos.col<O_FLAGS>()[yo] : 0;
    const u32 hp_img = flags & FLAG_CONGELADO ? IMG_FRAME + 16 + 2 * a.hpmp_estilo
                       : flags & FLAG_VENENO  ? IMG_FRAME + 15 + 2 * a.hpmp_estilo
                                              : IMG_FRAME + 1 + 3 * a.hpmp_estilo;
    auto bar = [&](u32 img, i32 v, i32 max, int ty, u32 outline) {
        RegionAtlas reg = g.img(img);
        if (!reg.valida()) return;
        const int full = reg.w;
        if (v > 0) {
            reg.w = static_cast<u16>(std::clamp<i64>(static_cast<i64>(v) * full / max, 0, full));  // 0x4b5dd6: value * width / max
            if (reg.w > 0) g.gpu().dibujar(reg, r.x + BAR_X, r.y, KEY_BAR);
        }
        char t[24];
        std::snprintf(t, sizeof t, "%d/%d", v, max);
        const int w = ancho_texto(t, std::strlen(t));
        const float cx = static_cast<float>(r.x + BAR_X + full / 2 - w / 2), y = static_cast<float>(r.y + ty);
        for (int d = -1; d <= 1; d += 2) {
            g.texto(t, cx + d, y, KEY_TEXT, outline);
            g.texto(t, cx, y + d, KEY_TEXT, outline);
        }
        g.texto(t, cx, y, KEY_TEXT + 1, rgb555(0x7fff));
    };
    if (j.hp_max > 0) bar(hp_img, j.hp, j.hp_max, HP_TEXT_Y, rgb555(0x5949));
    if (j.mp_max > 0) bar(IMG_FRAME + 2 + 3 * a.hpmp_estilo, j.mp, j.mp_max, MP_TEXT_Y, rgb555(0x3214));
    g.gpu().dibujar(g.img(sobre_estilo ? IMG_STYLE_BTN_HOVER : IMG_STYLE_BTN), r.x, r.y + 7, KEY_CTRL);
    g.gpu().dibujar(g.img(sobre_cierre ? IMG_CLOSE_HOVER : IMG_CLOSE), r.x + r.w - 16, r.y + 12, KEY_CTRL);
    if (e.clic && sobre_estilo) {
        a.hpmp_estilo = static_cast<u8>((a.hpmp_estilo + 1) % 3);  // 0x4b5250
        a.clic_consumido = true;
    } else if (e.clic && sobre_cierre) {
        a.cfg.opciones_orig &= ~(1 << OPC_FLOATHPMP);  // 0x4b56b0
        guardar_config(a.cfg);
        a.clic_consumido = true;
    }
}

}  // namespace lin

// Original volume window (Options page action `volume`): client class built at 0x4a9cd0, drawn at 0x4aa3b0 (S3EP1U.bin).
//  - frame image 1197 at (0,0), translucent background 1198 at (8,11), close button 969 at (182,19);
//  - four labels (strings 792, 793, 794, 1016) at x 25, y 60 / 95 / 135 / 175; the title one has its own colour;
//  - three horizontal sliders, range 0..100 and step 1 (scroll bar class 0x47cd80): bar 1269 (141 px, with the MIN / MAX
//    captions) at x 30, arrows 1270 at x 24 and 172, thumb 1272 sliding along the bar; rows at y 106 / 146 / 190, callbacks 0x4a9bf0 (music), 0x4a9af0 (effects),
//    0x4a9b70 (third value, [0x6e42b8]; the fork has no audio channel for it, it is only remembered);
//  - OK button 1274 at (82,265) closes it (0x4a9c80).
#include <algorithm>
#include "app/app.h"

namespace lin {

namespace {

constexpr u32 IMG_FRAME = 1197, IMG_BACK = 1198, IMG_CLOSE = 969, IMG_BAR = 1269, IMG_ARROW = 1270, IMG_THUMB = 1272,
              IMG_OK = 1274;
constexpr int VOL_W = 204, VOL_H = 292;
constexpr int SLIDER_Y[3] = {106, 146, 190}, THUMB_X0 = 30, ARROW_L = 24, ARROW_R = 172;
constexpr int LABEL_Y[4] = {60, 95, 135, 175}, LABEL_X = 25;
constexpr int LABEL_STR[4] = {792, 793, 794, 1016};

const u64 KEY_BACK = clave_orden(CAPA_UI, 10), KEY_FRAME = clave_orden(CAPA_UI, 11), KEY_CTRL = clave_orden(CAPA_UI, 16),
          KEY_TEXT = clave_orden(CAPA_UI_TEXTO, 16);

int& valor(App& a, int i) { return i == 0 ? a.cfg.volumen_musica : i == 1 ? a.cfg.volumen_efectos : a.volumen_tercero; }

}  // namespace

Rect rect_volumen(const App& a) { return {(a.gpu.ancho() - VOL_W) / 2, a.gpu.alto() / 4, VOL_W, VOL_H}; }

void dibujar_volumen(App& a) {
    if (!a.ventana_volumen) return;
    arrastrar_ventana(a, IdVentana::VOLUMEN);
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const Rect r = rect_ventana(a, IdVentana::VOLUMEN);
    g.gpu().dibujar(g.img(IMG_BACK), r.x + 8, r.y + 11, KEY_BACK, rgba(255, 255, 255, opacidad_ventanas(a)));
    g.gpu().dibujar(g.img(IMG_FRAME), r.x, r.y, KEY_FRAME);
    const bool sobre_cierre = ui::sobre(e, r.x + 182, r.y + 19, 14, 14);
    g.gpu().dibujar(g.img(IMG_CLOSE), r.x + 182, r.y + 19, KEY_CTRL, sobre_cierre ? rgba(255, 255, 255) : rgba(200, 200, 200));
    if (sobre_cierre && e.clic) a.ventana_volumen = false;
    for (int i = 0; i < 4; ++i) {
        const std::string_view t = a.mensajes.linea(LABEL_STR[i]);
        g.texto_sombra(t, static_cast<float>(r.x + LABEL_X), static_cast<float>(r.y + LABEL_Y[i]), KEY_TEXT,
                       i == 0 ? rgba(255, 230, 120) : rgba(255, 255, 254));
    }
    const int bar_w = g.img(IMG_BAR).w, thumb_w = g.img(IMG_THUMB).w, track_w = std::max(1, bar_w - thumb_w);
    for (int i = 0; i < 3; ++i) {
        int& v = valor(a, i);
        const int y = r.y + SLIDER_Y[i];
        g.gpu().dibujar(g.img(IMG_BAR), r.x + THUMB_X0, y, KEY_CTRL - 1);
        const bool izq = ui::sobre(e, r.x + ARROW_L, y, 6, 14), der = ui::sobre(e, r.x + ARROW_R, y, 6, 14);
        g.gpu().dibujar(g.img(IMG_ARROW), r.x + ARROW_L, y + 2, KEY_CTRL, izq ? rgba(255, 255, 255) : rgba(200, 200, 200));
        g.gpu().dibujar(g.img(IMG_ARROW), r.x + ARROW_R, y + 2, KEY_CTRL, der ? rgba(255, 255, 255) : rgba(200, 200, 200));
        const int tx = r.x + THUMB_X0 + v * track_w / 100;
        g.gpu().dibujar(g.img(IMG_THUMB), tx, y + 2, KEY_CTRL);
        const bool sobre_pista = ui::sobre(e, r.x + THUMB_X0, y, bar_w, 14);
        int nuevo = v;
        if (e.pulsado && izq) nuevo = v - 1;  // arrow buttons: step 1 (0x47d460 / 0x47d470)
        else if (e.pulsado && der) nuevo = v + 1;
        else if (e.pulsado && sobre_pista) a.volumen_arrastre = i;
        if (a.volumen_arrastre == i) {
            if (e.presionado || e.pulsado) nuevo = (e.mx - (r.x + THUMB_X0) - thumb_w / 2) * 100 / std::max(1, track_w);
            else a.volumen_arrastre = -1;
        }
        nuevo = std::clamp(nuevo, 0, 100);
        if (nuevo != v) {
            v = nuevo;
            aplicar_volumenes(a);
            a.clic_consumido = true;
        }
        if (a.volumen_arrastre == i) a.clic_consumido = true;
    }
    const RegionAtlas ok = g.img(IMG_OK);
    const bool sobre_ok = ui::sobre(e, r.x + 82, r.y + 265, ok.w, ok.h);
    g.gpu().dibujar(ok, r.x + 82, r.y + 265, KEY_CTRL, sobre_ok ? rgba(255, 255, 255) : rgba(200, 200, 200));
    if (sobre_ok && e.clic) {
        a.ventana_volumen = false;
        guardar_config(a.cfg);
    }
    if (!a.ventana_volumen) a.volumen_arrastre = -1;
}

}  // namespace lin

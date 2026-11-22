// Cálculos de la vista fija: una grilla lógica constante se escala dentro de la ventana conservando la proporción
// (bandas negras arriba y abajo / a los costados). Funciones puras, sin SDL.
#pragma once
#include <algorithm>
#include <cmath>

namespace lin {

struct RectVista {
    int x = 0, y = 0, w = 1, h = 1;  // rectángulo de destino en píxeles de la ventana
};

// Factor de ajuste de una grilla de gw x gh dentro de una ventana de pw x ph (puede ser fraccionario o menor que 1).
inline float factor_ajuste(int pw, int ph, int gw, int gh) {
    if (gw <= 0 || gh <= 0) return 1.0f;
    return std::min(static_cast<float>(pw) / static_cast<float>(gw), static_cast<float>(ph) / static_cast<float>(gh));
}

// Escala entera a la que se dibuja la grilla. `entero`: la mayor que entra (píxeles nítidos).
// Si no, la menor que cubre el ajuste (con tope 4), así la pasada final solo reduce
// menos de 2x con un filtro lineal. Siempre >= 1.
inline int escala_interna(float fit, bool entero) {
    const int s = entero ? static_cast<int>(std::floor(fit + 1e-4f)) : static_cast<int>(std::ceil(fit - 1e-4f));
    return std::clamp(s, 1, entero ? 16 : 4);
}

// Dónde cae la grilla en la ventana. `entero` con ajuste >= 1: múltiplo entero, centrado. Si no, el
// rectángulo más grande con la proporción de la grilla que entra, centrado.
inline RectVista rect_vista(int pw, int ph, int gw, int gh, bool entero) {
    const float fit = factor_ajuste(pw, ph, gw, gh);
    RectVista r;
    if (entero && fit >= 1.0f) {
        const int s = escala_interna(fit, true);
        r.w = gw * s;
        r.h = gh * s;
    } else {
        r.w = std::clamp(static_cast<int>(std::lround(static_cast<float>(gw) * fit)), 1, std::max(1, pw));
        r.h = std::clamp(static_cast<int>(std::lround(static_cast<float>(gh) * fit)), 1, std::max(1, ph));
    }
    r.x = (pw - r.w) / 2;
    r.y = (ph - r.h) / 2;
    return r;
}

// Píxel de la ventana -> celda de la grilla (piso; negativo o >= tamaño de la grilla sobre las bandas negras).
inline void pixel_a_grilla(float px, float py, const RectVista& r, int gw, int gh, int& gx, int& gy) {
    gx = static_cast<int>(std::floor((px - static_cast<float>(r.x)) * static_cast<float>(gw) / static_cast<float>(r.w)));
    gy = static_cast<int>(std::floor((py - static_cast<float>(r.y)) * static_cast<float>(gh) / static_cast<float>(r.h)));
}

}  // namespace lin

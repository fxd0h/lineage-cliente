// Fixed-view math: a constant logical grid is scaled into the window keeping the aspect ratio
// (letterbox / pillarbox). Pure functions, no SDL.
#pragma once
#include <algorithm>
#include <cmath>

namespace lin {

struct RectVista {
    int x = 0, y = 0, w = 1, h = 1;  // destination rectangle in window pixels
};

// Fit factor of a gw x gh grid inside a pw x ph window (can be fractional or below 1).
inline float factor_ajuste(int pw, int ph, int gw, int gh) {
    if (gw <= 0 || gh <= 0) return 1.0f;
    return std::min(static_cast<float>(pw) / static_cast<float>(gw), static_cast<float>(ph) / static_cast<float>(gh));
}

// Integer scale at which the grid is rendered. `entero`: the largest that fits (sharp pixels).
// Otherwise the smallest that covers the fit (capped at 4), so the final pass only ever downsamples
// by less than 2x with a linear filter. Always >= 1.
inline int escala_interna(float fit, bool entero) {
    const int s = entero ? static_cast<int>(std::floor(fit + 1e-4f)) : static_cast<int>(std::ceil(fit - 1e-4f));
    return std::clamp(s, 1, entero ? 16 : 4);
}

// Where the grid lands in the window. `entero` with fit >= 1: integer multiple, centered. Otherwise the
// largest rectangle of the grid's aspect ratio that fits, centered.
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

// Window pixel -> grid cell (floor; negative or >= grid size when over the black bars).
inline void pixel_a_grilla(float px, float py, const RectVista& r, int gw, int gh, int& gx, int& gy) {
    gx = static_cast<int>(std::floor((px - static_cast<float>(r.x)) * static_cast<float>(gw) / static_cast<float>(r.w)));
    gy = static_cast<int>(std::floor((py - static_cast<float>(r.y)) * static_cast<float>(gh) / static_cast<float>(r.h)));
}

}  // namespace lin

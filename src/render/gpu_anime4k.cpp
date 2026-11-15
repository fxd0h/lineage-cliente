// Anime4K CNN x2 (S) scaling filter (setting escalado=anime4k). Shaders: src/render/shaders/anime4k_*.frag,
// generated from https://github.com/bloc97/Anime4K (MIT License, Copyright (c) 2019-2021 bloc97).
//
// Passes, all at the logical grid size unless noted (destino_ holds each logical pixel as an escala x escala
// block, so pass 1 taps its block centres):
//   1..4  conv 3x3 (4 -> 4 -> 4 -> 4 -> 4 channels, ReLU on +/- split)   destino_ -> A -> B -> A -> B (RGBA16F)
//   5     depth-to-space + bilinear(original)                             -> 2x image (copia_ when escala == 2, else X2)
//   6     linear resample of X2 to the window-sized target (copia_), skipped when escala == 2
#include "render/gpu.h"

namespace lin {

bool Gpu::crear_anime4k() {
    if (a4k_a_ && a4k_b_ && a4k_w_ == grid_w_ && a4k_h_ == grid_h_ && (escala_ == 2 || a4k_x2_)) return true;
    SDL_ReleaseGPUTexture(dev_, a4k_a_);  // SDL frees them once the GPU is done with them
    SDL_ReleaseGPUTexture(dev_, a4k_b_);
    SDL_ReleaseGPUTexture(dev_, a4k_x2_);
    a4k_a_ = a4k_b_ = a4k_x2_ = nullptr;
    SDL_GPUTextureCreateInfo td{};
    td.type = SDL_GPU_TEXTURETYPE_2D;
    td.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    td.layer_count_or_depth = 1;
    td.num_levels = 1;
    td.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    td.width = static_cast<Uint32>(grid_w_);
    td.height = static_cast<Uint32>(grid_h_);
    a4k_a_ = SDL_CreateGPUTexture(dev_, &td);
    a4k_b_ = SDL_CreateGPUTexture(dev_, &td);
    if (escala_ != 2) {
        td.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        td.width = static_cast<Uint32>(grid_w_ * 2);
        td.height = static_cast<Uint32>(grid_h_ * 2);
        a4k_x2_ = SDL_CreateGPUTexture(dev_, &td);
    }
    if (!a4k_lineal_) {
        SDL_GPUSamplerCreateInfo si{};
        si.min_filter = si.mag_filter = SDL_GPU_FILTER_LINEAR;
        si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        a4k_lineal_ = SDL_CreateGPUSampler(dev_, &si);
    }
    if (!a4k_a_ || !a4k_b_ || (escala_ != 2 && !a4k_x2_) || !a4k_lineal_) {
        LIN_LOG("Anime4K: texturas de %dx%d: %s", grid_w_, grid_h_, SDL_GetError());
        a4k_w_ = a4k_h_ = 0;
        return false;
    }
    a4k_w_ = grid_w_;
    a4k_h_ = grid_h_;
    return true;
}

SDL_GPUTexture* Gpu::pase_anime4k(SDL_GPUCommandBuffer* cmd) {
    for (SDL_GPUGraphicsPipeline* p : a4k_pipe_)
        if (!p) return nullptr;
    if (!copia_ || !crear_anime4k()) return nullptr;
    const bool directo = escala_ == 2;  // the 2x image already has the final size
    SDL_GPUTexture* objetivos[A4K_PASES] = {a4k_a_, a4k_b_, a4k_a_, a4k_b_, directo ? copia_ : a4k_x2_, copia_};
    SDL_GPUTexture* fuentes[A4K_PASES] = {destino_, a4k_a_, a4k_b_, a4k_a_, a4k_b_, a4k_x2_};
    const int n = directo ? A4K_PASES - 1 : A4K_PASES;
    for (int k = 0; k < n; ++k) {
        SDL_GPUColorTargetInfo ot{};
        ot.texture = objetivos[k];
        ot.load_op = SDL_GPU_LOADOP_DONT_CARE;
        ot.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* fp = SDL_BeginGPURenderPass(cmd, &ot, 1, nullptr);
        if (!fp) return nullptr;
        SDL_BindGPUGraphicsPipeline(fp, a4k_pipe_[k]);
        // u.z = scale of the sampled image: destino_ blocks for pass 1 and for the scene tap of pass 5.
        const float u[4] = {static_cast<float>(grid_w_), static_cast<float>(grid_h_),
                            static_cast<float>(k == 0 || k == 4 ? escala_ : 1), 0.0f};
        SDL_GPUTextureSamplerBinding sb[2] = {{fuentes[k], k == 5 ? a4k_lineal_ : muestreo_}, {destino_, muestreo_}};
        SDL_BindGPUFragmentSamplers(fp, 0, sb, k == 4 ? 2 : 1);
        if (k != 5) SDL_PushGPUFragmentUniformData(cmd, 0, u, sizeof u);
        SDL_DrawGPUPrimitives(fp, 6, 1, 0, 0);
        SDL_EndGPURenderPass(fp);
    }
    return copia_;
}

}  // namespace lin

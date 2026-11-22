// Cursores del juego, como en el cliente original. Los originales son recursos RT_CURSOR monocromos de 32x32
// (ids 1..17) dentro del binario del juego (S3EP1U.bin, si no S3EP1.bin / Lineage.exe), leídos en
// ejecución con recursos/pe_resources y creados con SDL_CreateCursor (bits de datos/máscara, así funcionan
// los píxeles invertidos), escalados por el zoom entero del juego y recreados cuando cambia.
// Rol -> id de recurso (elegidos comparando con el video de referencia; el puntero por omisión en
// el video es la mano blanca que señala): mano 2, espada 6, círculo de objetivo 8, hablar 7 ("?"), tienda 13.
// Si falta el binario o un recurso, se usan en su lugar los cursores dibujados a mano de abajo (arte de 16x16, como
// cursores de color del sistema).
#include <algorithm>
#include <cmath>
#include "app/app.h"
#include "plataforma/archivo_mapeado.h"
#include "recursos/pe_resources.h"

namespace lin {

namespace {

// Paleta: K contorno, Y/y dorado claro/oscuro, W/G/g acero claro/medio/oscuro, B cuero, R/r rojo
// claro/oscuro, P rosa (brillo de los labios).
u32 color(char c) {
    switch (c) {
    case 'K': return rgba(24, 16, 8);
    case 'Y': return rgba(240, 204, 104);
    case 'y': return rgba(186, 136, 52);
    case 'W': return rgba(244, 244, 236);
    case 'G': return rgba(176, 182, 194);
    case 'g': return rgba(112, 118, 130);
    case 'B': return rgba(128, 76, 34);
    case 'R': return rgba(226, 54, 42);
    case 'r': return rgba(150, 28, 30);
    case 'P': return rgba(255, 160, 150);
    default: return 0;
    }
}

// Mano dorada que apunta (punto activo: la punta del índice).
constexpr const char* MANO[16] = {
    "..KK............", ".KYYK...........", ".KYyK...........", ".KYyK...........",
    ".KYyKKK.........", ".KYyKYyKKK......", ".KYyKYyKYyKK....", ".KYyyYyyYyKyK...",
    ".KYyyyyyyyyyyK..", "KKYyyyyyyyyyyK..", "KYKyyyyyyyyyyK..", "KYYyyyyyyyyyK...",
    ".KYyyyyyyyyyK...", "..KYyyyyyyyK....", "...KYyyyyyK.....", "...KKKKKKKK.....",
};
// Espada en diagonal (punto activo: la punta).
constexpr const char* ESPADA[16] = {
    "KK..............", "KWGK............", ".KWGK...........", "..KWGK..........",
    "...KWGK.........", "....KWGK........", ".....KWGK.......", "......KWGK......",
    ".......KWGK..K..", "........KWGKKYK.", ".........KWYYK..", "..........KYyK..",
    ".........KYKKBK.", "........KYK..KBK", ".........K....KB", "..............KK",
};

// Moneda con $: tienda de un jugador (sobre el vendedor; el clic camina hasta él y pide su lista).
constexpr const char* MONEDA[16] = {
    "................",
    ".......KK.......",
    "....KKKYYKKK....",
    "...KYYYYYYYYK...",
    "...KYyKKKKKyK...",
    "...KYyK.........",
    "....KYYYYK......",
    ".....KYYYYKK....",
    "......KKKYyK....",
    ".........KyYK...",
    "...KKKKKKyYK....",
    "...KYYYYYYYYK...",
    "....KKKKKKKKK...",
    "................",
    "................",
    "................",
};

// Labios (hablar con un NPC; punto activo: el centro de la boca).
constexpr const char* LABIOS[16] = {
    "................", "................", "................", "................",
    "....KKK..KKK....", "...KRRRKKRRRK...", "..KRPRRRRRRPRK..", ".KRRRRRRRRRRRRK.",
    ".KKKKKKKKKKKKKK.", ".KrRRRRRRRRRRrK.", "..KrRRPPPPRRrK..", "...KrRRRRRRrK...",
    "....KKrrrrKK....", "......KKKK......", "................", "................",
};

// Círculo de objetivo: anillo rojo con contorno, marcas en cruz y un punto al centro.
char objetivo(int x, int y) {
    const double dx = x - 7.5, dy = y - 7.5, r = std::sqrt(dx * dx + dy * dy);
    if (r > 7.6) return '.';
    if (r > 6.6) return 'K';
    if (r > 5.0) return 'R';
    if (r > 4.2) return 'K';
    if ((x == 7 || x == 8) && (y <= 4 || y >= 11)) return 'Y';
    if ((y == 7 || y == 8) && (x <= 4 || x >= 11)) return 'Y';
    if (r < 1.3) return 'R';
    return '.';
}

SDL_Surface* superficie(char (*pixel)(int, int, const char* const*), const char* const* arte, int escala) {
    SDL_Surface* s = SDL_CreateSurface(16 * escala, 16 * escala, SDL_PIXELFORMAT_RGBA32);
    if (!s) return nullptr;
    u8* px = static_cast<u8*>(s->pixels);
    for (int y = 0; y < 16 * escala; ++y)
        for (int x = 0; x < 16 * escala; ++x) {
            u32 c = color(pixel(x / escala, y / escala, arte));
            std::memcpy(px + y * s->pitch + x * 4, &c, 4);  // RGBA32 = bytes R, G, B, A (como rgba())
        }
    return s;
}

char de_arte(int x, int y, const char* const* arte) { return arte[y][x]; }
char de_objetivo(int x, int y, const char* const*) { return objetivo(x, y); }

SDL_Cursor* crear(char (*pixel)(int, int, const char* const*), const char* const* arte, int hx, int hy) {
    SDL_Surface* base = superficie(pixel, arte, 2);
    SDL_Surface* doble = superficie(pixel, arte, 4);  // pantallas Retina
    if (!base) return nullptr;
    if (doble) SDL_AddSurfaceAlternateImage(base, doble);
    SDL_Cursor* c = SDL_CreateColorCursor(base, hx * 2, hy * 2);
    SDL_DestroySurface(doble);
    SDL_DestroySurface(base);
    return c;
}

// Original cursor resource id per role (Cursor order: HAND, SWORD, TARGET, LIPS, COIN).
constexpr u32 ORIGINAL_IDS[] = {2, 6, 8, 7, 13};
static_assert(std::size(ORIGINAL_IDS) == static_cast<size_t>(Cursor::ESTADO_1), "one id per role, then the 16 states");

// Estado de cursor -> id de recurso de grupo de LoadCursor (tabla 0x6e0de8, 0x477c50); la imagen RT_CURSOR sale de su grupo.
constexpr u32 STATE_GROUPS[16] = {130, 131, 105, 127, 132, 133, 144, 136, 138, 139, 140, 141, 142, 143, 145, 150};

std::vector<PeCursor> g_originals;  // se lee una vez; vacío si no hay
bool g_originals_tried = false;
int g_scale = 0;

void load_originals(const std::string& game_dir) {
    g_originals_tried = true;
    for (const char* name : {"S3EP1U.bin", "S3EP1.bin", "Lineage.exe"}) {
        ArchivoMapeado f;
        if (!f.abrir(ruta_real(game_dir + name))) continue;
        std::vector<PeCursor> all;
        bool any = false;
        // Los 5 roles del fork (interfaz simple) y los 16 estados originales se cargan cada uno por su cuenta: S3EP1U.bin tiene los
        // cursores de estado (grupos en 0x6e0de8) pero no los ids de rol de los ejecutables más viejos, y si falta uno solo queda vacía su casilla.
        for (u32 id : ORIGINAL_IDS) {
            PeCursor c;
            if (!pe_load_cursor(f.datos(), id, c) || c.width % 8 != 0) c = {};
            any = any || c.width > 0;
            all.push_back(std::move(c));
        }
        for (u32 group : STATE_GROUPS) {
            PeCursor c;
            const u32 rt = pe_cursor_group_image(f.datos(), group);
            const bool ok = rt && pe_load_cursor(f.datos(), rt, c);
            if (!ok || c.width % 8 != 0) c = {};
            any = any || c.width > 0;
            all.push_back(std::move(c));
        }
        if (any) {
            g_originals = std::move(all);
            return;
        }
    }
}

SDL_Cursor* from_original(const PeCursor& c, int scale) {
    const int w = c.width * scale, h = c.height * scale, stride = w / 8;
    std::vector<u8> data(static_cast<size_t>(stride) * h, 0), mask(data.size(), 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const PePixel p = c.pixels[static_cast<size_t>(y / scale) * c.width + x / scale];
            const u8 bit = static_cast<u8>(0x80 >> (x & 7));
            const size_t i = static_cast<size_t>(y) * stride + x / 8;
            if (p == PePixel::BLACK || p == PePixel::INVERT) data[i] |= bit;  // inverted: data=1, mask=0
            if (p == PePixel::BLACK || p == PePixel::WHITE) mask[i] |= bit;
        }
    return SDL_CreateCursor(data.data(), mask.data(), w, h, c.hot_x * scale, c.hot_y * scale);
}

void build(App& a) {
    g_scale = std::clamp(a.gpu.escala(), 1, 4);
    for (int i = 0; i < static_cast<int>(Cursor::CANTIDAD); ++i)
        if (i < static_cast<int>(g_originals.size()) && g_originals[i].width > 0)
            a.cursores[i] = from_original(g_originals[i], g_scale);
    // Todo rol sin imagen original (ver load_originals) conserva el dibujo hecho a mano.
    auto art = [&](Cursor role, SDL_Cursor* made) {
        if (!a.cursores[static_cast<int>(role)]) a.cursores[static_cast<int>(role)] = made;
        else if (made) SDL_DestroyCursor(made);
    };
    if (!a.cursores[static_cast<int>(Cursor::MANO)]) {
        art(Cursor::MANO, crear(de_arte, MANO, 2, 0));
        art(Cursor::ESPADA, crear(de_arte, ESPADA, 0, 0));
        art(Cursor::OBJETIVO, crear(de_objetivo, nullptr, 7, 7));
        art(Cursor::LABIOS, crear(de_arte, LABIOS, 7, 8));
        art(Cursor::MONEDA, crear(de_arte, MONEDA, 7, 7));
    }
}

}  // namespace

void crear_cursores(App& a) {
    if (!g_originals_tried) load_originals(a.cfg.carpeta_juego);
    build(a);
}

void aplicar_cursor(App& a) {
    if (!g_originals.empty() && std::clamp(a.gpu.escala(), 1, 4) != g_scale) {
        SDL_Cursor* old[static_cast<int>(Cursor::CANTIDAD)];
        std::memcpy(old, a.cursores, sizeof old);
        std::memset(a.cursores, 0, sizeof old);
        build(a);
        a.cursor_aplicado = Cursor::CANTIDAD;
        if (SDL_Cursor* c = a.cursores[static_cast<int>(a.cursor)]) SDL_SetCursor(c);
        for (SDL_Cursor* c : old)
            if (c) SDL_DestroyCursor(c);
    }
    if (a.cursor == a.cursor_aplicado) return;
    if (SDL_Cursor* c = a.cursores[static_cast<int>(a.cursor)]) SDL_SetCursor(c);
    a.cursor_aplicado = a.cursor;
}

void destruir_cursores(App& a) {
    for (SDL_Cursor*& c : a.cursores) {
        if (c) SDL_DestroyCursor(c);
        c = nullptr;
    }
}

}  // namespace lin

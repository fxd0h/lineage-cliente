// Renderer 2D sobre SDL_GPU.
//
// - Todas las imágenes viven en un atlas: un texture array de páginas de 2048×2048 RGBA8. Cuando
//   se llenan, se reemplaza la página usada hace más tiempo (ver atlas.h).
// - Cada frame se arma una lista de comandos (instancias de 48 bytes + clave de orden u64), se
//   ordena con radix sort (estable) y se dibuja con UNA llamada instanciada: como todas las
//   páginas están en el mismo texture array, no hace falta cortar el lote por textura.
// - Las coordenadas son lógicas. Fuera del juego el tamaño lógico es el del original (640×480);
//   en el juego es "ampliable": ventana / escala, así una ventana más grande o un zoom menor
//   muestran más mapa (mínimo 640×480). Se dibuja en una textura de tamaño lógico × escala:
//   con muestreo "nearest" los píxeles del arte siguen nítidos, y las posiciones fraccionarias
//   (cámara) se mueven en pasos de 1/escala de píxel lógico, suaves también en Retina.
#pragma once
#include "render/vista.h"
#include <SDL3/SDL.h>
#include <string>
#include <string_view>
#include <vector>
#include "recursos/imagen.h"
#include "render/atlas.h"

namespace lin {

struct Instancia {
    float x, y, w, h;
    float u0, v0, u1, v1;
    u32 pagina;
    u32 color;  // RGBA8 multiplicativo (0xAABBGGRR)
    u32 mezcla; // Mezcla
    u32 relleno;
};
static_assert(sizeof(Instancia) == 48, "debe coincidir con el struct del shader");

// Normal (tapa según el alfa) o aditiva (suma su luz al fondo: efectos de hechizos).
// SILUETA: ignora el color de la textura, usa el color del tinte con el alfa de la textura (bit 1); la variante
// aditiva suma ese color plano al fondo.
// PANTALLA: mezcla screen, dst' = src + dst * (1 - src) por canal (los íconos de buff del cliente original);
// se dibuja en una segunda llamada después de las normales de la misma pasada.
// GRIS (bit 5): la luminancia de la textura por el tinte, para los íconos monocromos de la franja inferior del original.
// TEXTO_HD (bit 6): glifo de texto HD (la región del atlas es un campo, recursos/fuente_hd.h). Lo saltan las pasadas previas al filtro
// y lo dibuja Gpu::pase_texto después del filtro de escalado, a la resolución de salida.
enum class Mezcla : u32 { NORMAL = 0, ADITIVA = 1, SILUETA = 2, SILUETA_ADITIVA = 3, PANTALLA = 8, GRIS = 32, MULTIPLICAR = 128, TEXTO_HD = 64 };

// Capas de dibujo (byte alto de la clave de orden). CAPA_VENTANAS no se usa directo: es donde
// Gpu::nivel_ventana lleva lo que se dibuja en CAPA_UI y CAPA_UI_TEXTO mientras se dibuja una
// ventana. CAPA_ARRASTRE: lo que se arrastra con el mouse, encima de todo.
enum Capa : u8 {
    CAPA_FONDO = 0, CAPA_SUELO = 1, CAPA_MUNDO = 2, CAPA_EFECTOS = 3, CAPA_UI = 4, CAPA_UI_TEXTO = 5,
    CAPA_VENTANAS = 6, CAPA_ARRASTRE = 7,
};

inline u64 clave_orden(Capa capa, u32 profundidad) { return (static_cast<u64>(capa) << 56) | (static_cast<u64>(profundidad) << 24); }

constexpr u32 BLANCO = 0xFFFFFFFFu;
constexpr u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) { return r | (g << 8) | (b << 16) | (static_cast<u32>(a) << 24); }

constexpr int TAM_PAGINA = 2048;
constexpr int PAGINAS_ATLAS = 8;           // 8 × 16 MB; LIN_ATLAS_PAGINAS y LIN_ATLAS_TAM achican el atlas para probar el reemplazo
// 6 MB de buffer. Una vista grande (5K con zoom ×1) ya pide ~55 000 solo de suelo, más la lluvia.
constexpr u32 MAX_INSTANCIAS = 1u << 17;
// Topes por capa, porque se descarta lo que llega con la lista llena y el suelo se manda primero: el
// fondo y el suelo pueden ocupar hasta MAX − 2 × RESERVA, los objetos, personajes y efectos hasta
// MAX − RESERVA, y la interfaz todo. Así en una vista enorme se pierde suelo del borde, no personajes
// ni la interfaz.
constexpr u32 RESERVA_CAPAS = 1u << 14;
constexpr u32 TAM_SUBIDA_ATLAS = 32u << 20; // máximo de píxeles nuevos por frame

// Filtro de la presentación final ("escalado" de settings.ini): entero = blit por vecino más cercano, los demás son una pasada de pantalla completa.
enum class Escalado : int { ENTERO = 0, XBR = 1, FSR = 2, CRT = 3, ANIME4K = 4 };
inline constexpr int N_ESCALADOS = 5;
inline const char* nombre_escalado(Escalado e) {
    constexpr const char* n[N_ESCALADOS] = {"entero", "xbr", "fsr", "crt", "anime4k"};
    return n[static_cast<int>(e)];
}
inline Escalado escalado_desde(std::string_view s) {
    for (int i = 0; i < N_ESCALADOS; ++i)
        if (s == nombre_escalado(static_cast<Escalado>(i))) return static_cast<Escalado>(i);
    return Escalado::ENTERO;
}

class Gpu {
public:
    // ancho/alto lógicos mínimos (los del original).
    bool iniciar(SDL_Window* ventana, int ancho_logico, int alto_logico);
    // Al comienzo de cada frame: fija la escala y el tamaño lógico según la ventana. `zoom` es la
    // escala deseada (píxeles reales por píxel lógico; 0 = la mayor que entra, como el original).
    // Sin `ampliable`, el tamaño lógico es el mínimo y la imagen queda centrada.
    // `franja`: píxeles lógicos reservados arriba (la barra de la ventana); el contenido va debajo.
    // `solo_mundo`: el zoom cambia solo el mundo (capas por debajo de CAPA_UI); la interfaz queda con la
    // escala mayor. El mundo se sigue dibujando en sus coordenadas lógicas (ver zoom_mundo).
    void ajustar_vista(bool ampliable, int zoom, int franja = 0, bool solo_mundo = false);
    // Mayor escala con la que entra el tamaño mínimo en la ventana actual.
    int escala_maxima(int franja = 0) const;
    // Vista fija (por omisión): la grilla lógica es constante (640 x (480 + franja)) y se escala dentro de la
    // ventana conservando su proporción. Si no (vista amplia) la grilla crece con la ventana.
    void fijar_vista_fija(bool fija) { vista_fija_ = fija; }
    bool vista_fija() const { return vista_fija_; }
    void cerrar();

    // Reserva lugar en el atlas y encola la subida. false si no hay lugar este frame.
    // `escala_hd` > 1: img holds escala_hd texels per logical pixel (HD pack); the region keeps the logical size.
    bool subir(const ImagenRGBA& img, RegionAtlas& salida, u32 escala_hd = 1);
    // false si la imagen es más grande que una página del atlas: no entraría nunca.
    bool cabe_en_atlas(const ImagenRGBA& img) const { return empaquetado_.cabe(img.ancho, img.alto); }
    // Página del atlas vaciada por el último subir, o -1: sus regiones ya no valen.
    int tomar_pagina_reemplazada() { return empaquetado_.tomar_reemplazada(); }

    void dibujar(const RegionAtlas& r, int x, int y, u64 clave, u32 color = BLANCO);
    // Igual que dibujar, en una posición lógica fraccionaria (mundo con cámara suave).
    void dibujar_en(const RegionAtlas& r, float x, float y, u64 clave, u32 color = BLANCO, Mezcla mezcla = Mezcla::NORMAL);
    void dibujar_escalado(const RegionAtlas& r, float x, float y, float w, float h, u64 clave, u32 color = BLANCO,
                          Mezcla mezcla = Mezcla::NORMAL);
    void rectangulo(int x, int y, int w, int h, u64 clave, u32 color, Mezcla mezcla = Mezcla::NORMAL);
    // Este frame, lo que va por debajo de CAPA_EFECTOS (mundo, no interfaz) se multiplica por
    // `tinte`, por `brillo` (bw × bh celdas de celda × celda píxeles lógicos, 255 = sin cambio) y,
    // si se pasan, por el color de cada fila de la vista (`filas`, n_filas colores RGBA que cubren
    // la altura de la vista de arriba abajo; nullptr = blanco). Para solo tinte: brillo de 1 × 1 y una
    // celda mayor que la vista. Vale para el frame actual.
    // Deja lista la textura de brillo para bw × bh celdas, para no reservarla durante un frame.
    void reservar_brillo(int bw, int bh) { asegurar_luz(bw, bh); }
    void multiplicar_mundo(const u8* brillo, int bw, int bh, int celda, u32 tinte, const u32* filas = nullptr,
                           int n_filas = 0);
    // Este frame, el mundo se vuelve a escribir: `fase_ola` >= 0 lo desplaza por columnas (ola bajo el
    // agua; fase 0..99) y `modo_vision` lo transforma (1 elfo, 2 elfo oscuro; 0 ninguno).
    // `fase_borracho` >= 0 mece el mundo por filas (borrachera, S_Liquor 1; fase 0..99).
    void reemplazar_mundo(int fase_ola, int modo_vision, int fase_borracho = -1);
    // Mientras se dibuja una ventana (nivel 1..30 de abajo hacia arriba; 0 = ninguna): lo que va
    // en CAPA_UI y CAPA_UI_TEXTO se dibuja en CAPA_VENTANAS, todo junto a ese nivel. Así una
    // ventana tapa entera a las de abajo, su texto incluido.
    void nivel_ventana(u32 nivel) { nivel_ventana_ = nivel; }
    u32 nivel_ventana() const { return nivel_ventana_; }

    // Ordena la lista del frame (lo hace presentar si no se llamó antes; separado para medirlo
    // aparte de la espera del vsync).
    void preparar_frame();
    // Ordena si hace falta, sube y presenta el frame. Si hay una captura pedida, guarda la imagen lógica.
    void presentar();
    void pedir_captura(const std::string& ruta_bmp) { ruta_captura_ = ruta_bmp; }
    // Guarda la imagen final (después del filtro de escalado, a la resolución de la grilla de la ventana) como BMP.
    void pedir_captura_final(const std::string& ruta_bmp) { ruta_final_ = ruta_bmp; }
    void fijar_escalado(Escalado e) { escalado_ = e; }
    // Texto HD ("texto_hd" de settings.ini): glifos dibujados después del filtro de escalado a la resolución de salida. Funciona con SPIR-V y MSL
    // (fs_texto en ambos) y se apaga mientras hay una captura lógica pendiente, así
    // `captura` igual muestra el texto.
    void fijar_texto_hd(bool si) { texto_hd_ = si; }
    bool texto_hd() const { return texto_hd_ && pipe_texto_ && pipe_comp_[0] && ruta_captura_.empty(); }

    // Ventana (píxeles) -> coordenadas lógicas.
    void a_logico(float wx, float wy, int& lx, int& ly) const;
    // Ventana (píxeles) -> cuadrícula, sin descontar el origen del contenido.
    void a_grilla(float wx, float wy, int& gx, int& gy) const;
    // La cuadrícula cubre la ventana. El contenido (ancho() × alto()) va desplazado por el origen;
    // lo que va fuera del contenido (la barra) se dibuja en coordenadas negativas del contenido.
    int grid_ancho() const { return grid_w_; }
    int grid_alto() const { return grid_h_; }
    int origen_x() const { return origen_x_; }
    int origen_y() const { return origen_y_; }

    int ancho() const { return ancho_; }
    // Píxeles reales por píxel lógico (la del último frame). Para redondear la cámara a 1/escala.
    int escala() const { return escala_; }
    // Píxeles reales por píxel lógico del mundo, y el mundo respecto de la interfaz (escala_mundo /
    // escala, <= 1): un punto del mundo (x, y) se ve en (x, y) × zoom_mundo de la interfaz, así que el
    // mundo visible mide ancho() / zoom_mundo() × alto() / zoom_mundo().
    int escala_mundo() const { return escala_mundo_; }
    float zoom_mundo() const { return static_cast<float>(escala_mundo_) / static_cast<float>(escala_); }
    // Mientras está activo, lo que se dibuja en las capas del mundo ya viene en coordenadas de la interfaz
    // (textos sobre los personajes): no se le aplica el zoom del mundo.
    void sin_zoom_mundo(bool v) { sin_zoom_mundo_ = v; }
    bool sin_zoom_mundo() const { return sin_zoom_mundo_; }
    int alto() const { return alto_; }
    u32 llamadas_ultimo_frame() const { return llamadas_; }
    u32 instancias_ultimo_frame() const { return instancias_previas_; }
    u32 paginas_usadas() const { return empaquetado_.paginas_usadas(); }
    u32 reemplazos_atlas() const { return empaquetado_.reemplazos(); }

private:
    struct SubidaPendiente {
        RegionAtlas r;
        u32 offset;  // en el buffer de subida
    };
    bool crear_pipeline();
    bool crear_destino();
    void blit_vista(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* src, SDL_GPUTexture* dst);
    // Anime4K CNN x2 (S): 6 pasadas, los intermedios se crean recién al primer uso y al cambiar la grilla.
    bool crear_anime4k();
    SDL_GPUTexture* pase_anime4k(SDL_GPUCommandBuffer* cmd);  // destino_ -> copia_; nullptr if unavailable
    // Vacía la lista de dibujo y los efectos del frame (con o sin presentarlo).
    void terminar_frame(u32 instancias_dibujadas);
    bool asegurar_luz(int bw, int bh);
    void ordenar();
    // Descarga `tex` (del tamaño de destino_) y la guarda como BMP; `logica` deja un píxel por píxel lógico.
    SDL_GPUCommandBuffer* guardar_captura(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* tex, int w, int h, std::string& ruta, bool logica);

    SDL_Window* ventana_ = nullptr;
    SDL_GPUDevice* dev_ = nullptr;
    bool mailbox_ = false;          // modo de presentación MAILBOX: el bucle lo regula pace_frame()
    Uint64 next_frame_ns_ = 0;
    void pace_frame();
    bool usar_dxil_ = false;   // Windows (D3D12): DXIL compiled from shaders/*.hlsl (cmake/shaders.cmake)
    bool usar_spirv_ = false;  // Vulkan (Linux): shaders SPIR-V de render/shaders
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_pantalla_ = nullptr;  // same shaders, screen blend factors
    SDL_GPUGraphicsPipeline* pipeline_mult_ = nullptr;      // multiply blend (dst * src), see sprite shaders
    u32 n_mult_ = 0;                                        // instancias de multiplicar encoladas en este cuadro
    u32 n_pantalla_ = 0;                                    // instancias de mezcla screen encoladas en este cuadro
    SDL_GPUGraphicsPipeline* pipeline_brillo_ = nullptr;
    // Pasada de texto HD: las instancias ordenadas desde la primera de texto se dibujan en `capa_texto_` (color + cobertura) con
    // pipe_texto_ (el texto suma, las demás instancias no aditivas atenúan según su alfa), después pipe_comp_ pone
    // la capa sobre el destino: [0] para texturas R8G8B8A8, [1] para el formato del swapchain.
    SDL_GPUGraphicsPipeline* pipe_texto_ = nullptr;
    SDL_GPUGraphicsPipeline* pipe_comp_[2] = {};
    SDL_GPUTexture* capa_texto_ = nullptr;  // RGBA8: premultiplied text colour
    SDL_GPUTexture* capa_cob_ = nullptr;    // R8: text coverage
    int capa_w_ = 0, capa_h_ = 0;
    bool texto_hd_ = false;
    u32 primer_texto_ = 0;  // índice de la primera instancia TEXTO_HD en la lista ordenada (== n si no hay)
    void crear_pipelines_texto(SDL_GPUGraphicsPipelineCreateInfo pi, SDL_GPUShader* fs_texto);
    // Dibuja el texto HD sobre `destino` (`w` x `h`); `rect`: dónde cae la grilla lógica (nullptr = todo).
    void pase_texto(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* destino, int w, int h, int formato, const RectVista* rect);
    SDL_GPUGraphicsPipeline* pipeline_esc_[N_ESCALADOS] = {};  // [0] unused (nearest blit)
    Escalado escalado_ = Escalado::ENTERO;
    // Anime4K: conv1..conv4, depth-to-space, resample. a4k_a_/a4k_b_ alternan al tamaño de la grilla lógica
    // (RGBA16F); a4k_x2_ es la imagen de 2x (solo cuando la escala entera no es 2).
    static constexpr int A4K_PASES = 6;
    SDL_GPUGraphicsPipeline* a4k_pipe_[A4K_PASES] = {};
    SDL_GPUTexture* a4k_a_ = nullptr;
    SDL_GPUTexture* a4k_b_ = nullptr;
    SDL_GPUTexture* a4k_x2_ = nullptr;
    SDL_GPUSampler* a4k_lineal_ = nullptr;
    int a4k_w_ = 0, a4k_h_ = 0;
    std::string ruta_final_;
    SDL_GPUGraphicsPipeline* pipeline_mundo_ = nullptr;   // reemplazar el mundo (ola, visión)
    SDL_GPUTexture* copia_ = nullptr;                      // copia de destino_ para leerla al reemplazar
    SDL_GPUTexture* filas_tex_ = nullptr;                  // color por fila (1 × LUZ_TAM RGBA)
    SDL_GPUTransferBuffer* subida_filas_ = nullptr;
    std::vector<u32> brillo_filas_;
    bool reemplazo_activo_ = false;
    int reemplazo_fase_ = -1, reemplazo_modo_ = 0, reemplazo_fase_b_ = -1;
    SDL_GPUTexture* luz_ = nullptr;             // brillo del mundo (R8); crece con la vista
    SDL_GPUTransferBuffer* subida_luz_ = nullptr;
    std::vector<u8> brillo_buf_;                // copia del frame, reservada al iniciar
    bool brillo_activo_ = false;
    int luz_w_ = 0, luz_h_ = 0;                 // tamaño de luz_
    int brillo_w_ = 0, brillo_h_ = 0, brillo_celda_ = 1;
    u32 brillo_tinte_ = 0xFFFFFFFFu;
    u32 n_mundo_ = 0;                           // instancias de CAPA_FONDO..CAPA_MUNDO (prefijo de la lista)
    SDL_GPUTexture* atlas_ = nullptr;
    SDL_GPUTexture* destino_ = nullptr;  // resolución lógica
    SDL_GPUSampler* muestreo_lineal_ = nullptr;
    SDL_GPUSampler* muestreo_ = nullptr;
    SDL_GPUBuffer* instancias_gpu_ = nullptr;
    SDL_GPUTransferBuffer* subida_inst_ = nullptr;
    SDL_GPUTransferBuffer* subida_atlas_ = nullptr;
    int ancho_ = 0, alto_ = 0;            // tamaño lógico del frame actual
    int grid_w_ = 0, grid_h_ = 0;         // cuadrícula de la ventana (píxeles lógicos, >= el contenido)
    int origen_x_ = 0, origen_y_ = 0;     // dónde empieza el contenido dentro de la cuadrícula
    int ancho_min_ = 0, alto_min_ = 0;
    int escala_ = 1;
    bool vista_fija_ = true;
    RectVista rect_{0, 0, 1, 1};          // vista fija: dónde cae la grilla en la ventana (píxeles)
    int ventana_pw_ = 1, ventana_ph_ = 1;
    int escala_mundo_ = 1;
    bool sin_zoom_mundo_ = false;
    int destino_w_ = 0, destino_h_ = 0;   // tamaño real de destino_

    EmpaquetadoAtlas empaquetado_{TAM_PAGINA, PAGINAS_ATLAS};
    u64 frame_ = 1;
    std::vector<SubidaPendiente> pendientes_;
    std::vector<u8> staging_atlas_;
    bool atlas_lleno_avisado_ = false;
    u64 proximo_aviso_instancias_ = 0;  // frame desde el que se vuelve a avisar de la lista llena

    // Lista de dibujo del frame.
    std::vector<Instancia> inst_;
    std::vector<u64> claves_;
    u32 nivel_ventana_ = 0;
    std::vector<u32> orden_, orden_tmp_;
    std::vector<Instancia> inst_ordenadas_;
    u32 llamadas_ = 0, instancias_previas_ = 0;
    RegionAtlas blanco_;

    std::string ruta_captura_;
    bool preparado_ = false;
};

}  // namespace lin

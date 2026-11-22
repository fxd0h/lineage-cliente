// Vista del equipo (pestaña "Equipment" de la ventana de estado): la imagen del original (981 de
// Sprite.pak, 179×275: la silueta del personaje con un recuadro por parte del cuerpo y "Armor
// Class") con el ícono de lo equipado en cada recuadro. La parte de cada ítem sale de su tipo de uso
// (S_AddItem; ItemTable del servidor) y "equipado" del sufijo del nombre ($9 / $117).
// Los recuadros se midieron sobre la imagen; qué parte va en cada uno es una deducción por dónde
// caen sobre la silueta (casco en la cabeza, aro al costado, botas en los pies...). Los anillos no
// tienen recuadro: van más chicos en el espacio libre de abajo a la derecha.
// Con la UI simple no se usa la imagen: una lista de las partes (de la cabeza a los pies) con el
// ícono y el nombre de lo equipado, con el estilo de los demás paneles de texto.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "app/app.h"
#include "app/item_icon.h"

namespace lin {

namespace {

constexpr u32 IMG_EQUIPO = 981;  // 179×275

struct Recuadro {
    u8 uso;       // tipo de uso del objeto (ItemTable)
    int px, py;   // esquina superior izquierda del ícono desde la esquina de la ventana: tabla en 0x8f0818 (la llena 0x4b2f50), la usa 0x4b3220
    const char* nombre;
};
// Tipos de uso: 1 arma, 2 armadura, 18 camiseta, 19 capa (también 10), 20 guantes, 21 botas, 22 casco, 24 collar,
// 25 escudo, 37 cinturón, 40 aro. La casilla elegida por tipo es la clasificación de 0x4b30d0.
constexpr Recuadro RECUADROS[] = {
    {22, 77, 46, "Helm"},     {40, 38, 52, "Earring"}, {19, 116, 89, "Cloak"},  {24, 116, 52, "Amulet"},
    {18, 38, 89, "T-shirt"},  {2, 77, 89, "Armor"},    {20, 33, 189, "Gloves"}, {1, 33, 157, "Weapon"},
    {25, 121, 125, "Shield"}, {37, 78, 131, "Belt"},   {21, 78, 189, "Boots"},
};
constexpr u8 USO_ANILLO = 23;
// Las dos casillas de anillo (el primer anillo toma la primera).
constexpr int ANILLO_POS[2][2] = {{33, 125}, {121, 157}};
constexpr int LADO_ANILLO = 32;

}  // namespace

void dibujar_equipo(App& a, int x0, int y0) {
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    const Inventario& inv = a.mundo.inventario;
    const TablaItems& t = inv.items;
    const bool framed_ui = uses_original_frame(a, IdVentana::ESTADO);
    // En el marco original los íconos van sobre el fondo (10) y el marco (11), debajo del adorno (15).
    const u64 kf = clave_orden(CAPA_UI, 3), ki = clave_orden(CAPA_UI, framed_ui ? 14 : 5), kt = clave_orden(CAPA_UI_TEXTO, 5);
    // Qué hay equipado en cada parte.
    constexpr size_t N = std::size(RECUADROS);
    Slot en[N], anillos[4];
    std::fill(std::begin(en), std::end(en), SIN_SLOT);
    int nanillos = 0;
    for (Slot s = 0; s < t.tamano(); ++s) {
        if (!item_equipado(nombre_item(inv, s))) continue;
        const u8 uso = t.col<I_USO>()[s];
        if (uso == USO_ANILLO) {
            if (nanillos < 4) anillos[nanillos++] = s;
            continue;
        }
        for (size_t i = 0; i < N; ++i)
            if (RECUADROS[i].uso == uso) {
                en[i] = s;
                break;
            }
    }

    Slot bajo = SIN_SLOT;
    int bajo_x = 0, bajo_y = 0;  // esquina superior izquierda de la casilla bajo el puntero (la etiqueta se ubica respecto de ella)
    bool native = false;
    auto poner = [&](Slot s, int rx, int ry, int rw, int rh) {
        const RegionAtlas& ic = g.icono(t.col<I_GFX>()[s]);
        if (native) {
            // 0x4b3220 -> 0x445fd0 dibuja el ícono a su tamaño desde el origen de la casilla, como el inventario (sin estirar).
            draw_item_icon(g.gpu(), ic, rx, ry, ki, t.col<I_BENDICION>()[s], t.col<I_IDENTIFICADO>()[s] != 0);
        } else {
            const int lado = std::min({32, rw, rh});
            draw_item_icon(g.gpu(), ic, rx + (rw - lado) / 2, ry + (rh - lado) / 2, static_cast<float>(lado),
                           static_cast<float>(lado), ki, t.col<I_BENDICION>()[s], t.col<I_IDENTIFICADO>()[s] != 0);
        }
        if (ui::sobre(e, rx, ry, rw, rh)) {
            bajo = s;
            bajo_x = rx;
            bajo_y = ry;
        }
    };
    char txt[160];
    const RegionAtlas& fondo = g.img(IMG_EQUIPO);
    if (a.cfg.ui_simple || !fondo.valida()) {
        // Una fila por parte: rótulo, ícono (16×16) y nombre; los anillos, en la última fila.
        constexpr int FILA = 18, ROTULO = 58, LADO = 16;
        const int x = x0 + 14, w = ui::VENTANA_ANCHO - 28;
        int y = y0 + 28;
        const u32 rotulo = rgba(214, 190, 140), vacio = rgba(110, 100, 80);
        auto fila = [&](const char* nombre, const Slot* ss, int n) {
            g.gpu().rectangulo(x, y, w, FILA - 2, kf, n && ss[0] != SIN_SLOT ? rgba(40, 32, 22, 200) : rgba(22, 18, 14, 200));
            g.texto(nombre, static_cast<float>(x + 4), static_cast<float>(y + 2), kt, rotulo);
            const int ix = x + ROTULO;
            if (!n || ss[0] == SIN_SLOT) {
                g.texto("-", static_cast<float>(ix + 2), static_cast<float>(y + 2), kt, vacio);
            } else if (n == 1) {
                poner(ss[0], ix, y - 1, LADO, LADO + 2);
                const size_t largo = expandir_desc(a.desc, nombre_item(inv, ss[0]), txt, sizeof txt);
                std::string_view nom(txt, largo);
                if (const size_t p = nom.rfind(" ("); p != std::string_view::npos && nom.ends_with(")")) nom = nom.substr(0, p);
                const int ancho = w - ROTULO - LADO - 10;
                if (ancho_texto(nom.data(), nom.size()) > ancho) nom = nom.substr(0, bytes_que_caben(nom.data(), nom.size(), ancho));
                g.texto(nom, static_cast<float>(ix + LADO + 4), static_cast<float>(y + 2), kt, ui::COLOR_TEXTO);
                if (ui::sobre(e, x, y, w, FILA - 2)) bajo = ss[0];
            } else {
                for (int i = 0; i < n; ++i) poner(ss[i], ix + i * (LADO + 4), y - 1, LADO, LADO + 2);
            }
            y += FILA;
        };
        for (size_t i : {0, 1, 3, 2, 4, 5, 6, 7, 8, 9, 10}) fila(RECUADROS[i].nombre, &en[i], 1);  // de la cabeza a los pies
        fila("Rings", anillos, nanillos);
        std::snprintf(txt, sizeof txt, "%d", a.mundo.jugador.ca);
        g.texto("Armor Class", static_cast<float>(x + 4), static_cast<float>(y + 6), kt, rotulo);
        g.texto(txt, static_cast<float>(x + ROTULO + 30), static_cast<float>(y + 6), kt, ui::COLOR_TEXTO);
    } else {
        // En el marco original la imagen es su fondo (lo dibuja el marco, 1440 en (6,9));
        // en el marco de upstream (la interfaz simple), 981 centrada en la ventana.
        const bool framed = uses_original_frame(a, IdVentana::ESTADO);
        const int x = x0 + (framed ? 6 : 34), y = y0 + (framed ? 9 : 24);
        if (!framed) g.gpu().dibujar(fondo, x, y, kf);
        // Los íconos se ubican en coordenadas de la ventana (el origen de la imagen es 6,9 dentro del marco original).
        const int ox = x - 6, oy = y - 9;
        native = framed;
        for (size_t i = 0; i < N; ++i)
            if (en[i] != SIN_SLOT) poner(en[i], ox + RECUADROS[i].px, oy + RECUADROS[i].py, 32, 32);
        for (int i = 0; i < std::min(nanillos, 2); ++i)
            poner(anillos[i], ox + ANILLO_POS[i][0], oy + ANILLO_POS[i][1], LADO_ANILLO, LADO_ANILLO);
        // CA como 0x4b3740: blanco, centrado entre x 92 y 149 de la ventana a 6 px por carácter, tope del texto en y 231 de la ventana.
        std::snprintf(txt, sizeof txt, "%d", a.mundo.jugador.ca);
        const int ac_x = ox + 92 + (149 - 92 - 6 * static_cast<int>(std::strlen(txt)) + 1) / 2;
        g.texto(txt, static_cast<float>(ac_x), static_cast<float>(oy + 231), kt, rgba(255, 255, 255));
    }

    // Marco original: la casilla bajo el puntero ([+0xa4], la fija el manejador de movimiento del mouse 0x4b36c8) muestra solo el nombre
    // del objeto, blanco con contorno negro, en (x de la casilla - 32, y de la casilla + 34) (segundo bucle de 0x4b3220 vía 0x4334f0). El nombre no es
    // la caja de atributos de la interfaz simple.
    if (bajo != SIN_SLOT && !(a.cfg.ui_simple || !fondo.valida())) {
        const size_t n = expandir_desc(a.desc, nombre_item(inv, bajo), txt, sizeof txt);
        std::string_view nom(txt, n);
        if (const size_t p = nom.rfind(" ("); p != std::string_view::npos && nom.ends_with(")")) nom = nom.substr(0, p);
        g.texto_contorno(nom, static_cast<float>(bajo_x - 32), static_cast<float>(bajo_y + 34), clave_orden(CAPA_UI_TEXTO, 6),
                         rgba(255, 255, 255));
    } else if (bajo != SIN_SLOT) {
        // Interfaz simple: el nombre del objeto y sus atributos.
        char nombre[256], det[1024];
        const size_t n = expandir_desc(a.desc, nombre_item(inv, bajo), nombre, sizeof nombre);
        const size_t nd = describir_atributos(atributos_item(inv, bajo), det, sizeof det);
        int w = ancho_texto(nombre, n), lineas = 0;
        for (std::string_view resto(det, nd); !resto.empty(); ++lineas) {
            const size_t fin = std::min(resto.find('\n'), resto.size());
            w = std::max(w, ancho_texto(resto.data(), fin));
            resto.remove_prefix(std::min(fin + 1, resto.size()));
        }
        const int alto = ALTO_GLIFO + 3, tx = std::min(e.mx + 12, a.gpu.ancho() - w - 8),
                  ty = std::min(e.my + 16, a.gpu.alto() - (lineas + 1) * alto - 8);
        const u64 kc = clave_orden(CAPA_UI_TEXTO, 29), kn = clave_orden(CAPA_UI_TEXTO, 30);
        g.gpu().rectangulo(tx - 4, ty - 3, w + 8, (lineas + 1) * alto + 4, kc, rgba(0, 0, 0, 230));
        g.texto({nombre, n}, static_cast<float>(tx), static_cast<float>(ty), kn, rgba(255, 255, 254));
        int ly = ty + alto;
        for (std::string_view resto(det, nd); !resto.empty(); ly += alto) {
            const size_t fin = std::min(resto.find('\n'), resto.size());
            g.texto(resto.substr(0, fin), static_cast<float>(tx), static_cast<float>(ly), kn, rgba(200, 190, 160));
            resto.remove_prefix(std::min(fin + 1, resto.size()));
        }
    }
}

}  // namespace lin

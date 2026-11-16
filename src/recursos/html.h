// Páginas HTML de los NPC (Text.pak, "<id>-e.html"): un subconjunto de HTML que el cliente
// original maqueta en su ventana de diálogo. En las 6499 páginas en inglés aparecen:
//  - <font fg=rrggbb>, <p align=left|center>, <center>, <br>, &nbsp; (y otras entidades);
//  - <a action="..."> (se manda al servidor con C_NPCAction) y <a link="..."> (otra página del
//    cliente, sin pasar por el servidor; también href en una página);
//  - <img src="#N" action|link=".." tooltip=".."> (N.img; botones como comprar/vender);
//  - <var src="#N">: el dato N que manda el servidor con la página; <username>: el personaje.
// El resto (<body>, <html>, <b>, <pre>, <meta>...) se ignora. Este módulo solo maqueta (líneas,
// posiciones, colores y vínculos); dibujar y responder a los clics es de la aplicación.
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/tipos.h"

namespace lin {

struct ElementoHtml {
    enum class Tipo : u8 { TEXTO, IMAGEN };
    Tipo tipo = Tipo::TEXTO;
    bool es_enlace = false;   // el vínculo es una página local (link); si no, una acción (action)
    i16 x = 0, y = 0, w = 0, h = 0;
    u32 color = 0;            // texto: RGBA
    u32 imagen = 0;           // imagen: N.img
    u32 texto_ofs = 0, texto_len = 0;    // en PaginaHtml::textos
    u32 vinculo_ofs = 0, vinculo_len = 0;  // acción o página; vacío si no es un vínculo
    u32 ayuda_ofs = 0, ayuda_len = 0;    // tooltip
};

struct PaginaHtml {
    std::vector<ElementoHtml> elementos;
    std::string textos;
    int alto = 0;

    std::string_view texto(const ElementoHtml& e) const { return {textos.data() + e.texto_ofs, e.texto_len}; }
    std::string_view vinculo(const ElementoHtml& e) const { return {textos.data() + e.vinculo_ofs, e.vinculo_len}; }
    std::string_view ayuda(const ElementoHtml& e) const { return {textos.data() + e.ayuda_ofs, e.ayuda_len}; }
    void limpiar() {
        elementos.clear();
        textos.clear();
        alto = 0;
    }
};

// Tamaño de la imagen N.img (false si no existe: no ocupa lugar).
using MedirImagen = bool (*)(void* contexto, u32 imagen, int& w, int& h);

constexpr u32 COLOR_TEXTO_HTML = 0xFFBEDCE6u;   // rgba(230, 220, 190)
constexpr u32 COLOR_VINCULO_HTML = 0xFF78E6FFu; // rgba(255, 230, 120)
constexpr int ALTO_LINEA_HTML = 15;

// Interfaz original: el cliente de 2009 (S3EP1U) guarda el color como COLORREF y se lo pasa tal cual a SetTextColor
// (0x43d2b3), así que el texto por defecto es 0xEEEDEE (0x435cc6), el de los vínculos 0xB0FFFF (0x437830, el <a> lo
// pone encima de cualquier <font>) y el atributo fg="v" se lee como hexadecimal sin cambiar el orden de los bytes:
// rojo = byte bajo. El vínculo bajo el puntero se pinta (10, 240, 50) (0x43d2ac: 0x32F00A). Sin subrayado:
// ninguna etiqueta enciende el indicador de subrayado (+0x16 del estilo, usado en 0x43d555).
constexpr u32 COLOR_TEXTO_HTML_ORIGINAL = 0xFFEEEDEEu;    // rgba(238, 237, 238)
constexpr u32 COLOR_VINCULO_HTML_ORIGINAL = 0xFFB0FFFFu;  // rgba(255, 255, 176)
constexpr u32 COLOR_SOBRE_HTML_ORIGINAL = 0xFF32F00Au;    // rgba(10, 240, 50)

// Maqueta la página en `ancho` píxeles. `original`: texto con la cara y el alto de línea del cliente original
// (recursos/fuente_ttf.h) y sus colores; si no, la fuente de la interfaz simple. `datos`: los de S_NPCTalkReturn separados por
// `separador`; `usuario`: el nombre del personaje.
void maquetar_html(std::string_view html, std::string_view datos, char separador, std::string_view usuario, int ancho,
                   MedirImagen medir, void* contexto, PaginaHtml& salida, bool original = false);

}  // namespace lin

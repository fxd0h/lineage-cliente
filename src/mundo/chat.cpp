#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "mundo/chat.h"
#include "recursos/fuente.h"
#include "recursos/imagen.h"

namespace lin {

namespace {

constexpr u32 color(u8 r, u8 g, u8 b) { return r | (g << 8) | (b << 16) | 0xFF000000u; }

constexpr u32 COLOR_MENSAJE = color(255, 255, 255);
constexpr u32 COLOR_AZUL = color(140, 180, 255);

// Escritura acotada en un buffer de tamaño fijo.
struct Salida {
    char* p;
    size_t cap, n = 0;
    void poner(std::string_view t) {
        for (char c : t)
            if (n < cap) p[n++] = c;
    }
};

// Cantidad de bytes de `t` que entran en `cap` sin cortar un carácter CP949 de 2 bytes.
size_t recorte_cp949(std::string_view t, size_t cap) {
    size_t i = 0;
    if (es_utf8(t.data(), t.size())) {  // Big5 text of the 3.80C server arrives as UTF-8: cut on character boundaries
        while (i < t.size()) {
            u32 cp = 0;
            const size_t w = largo_utf8(t.data(), t.size(), i, cp);
            if (i + w > cap) break;
            i += w;
        }
        return i;
    }
    while (i < t.size()) {
        size_t w = static_cast<u8>(t[i]) >= 0x81 && i + 1 < t.size() ? 2 : 1;
        if (i + w > cap) break;
        i += w;
    }
    return i;
}

}  // namespace

u32 color_chat(u8 tipo) {
    switch (tipo) {
    case CHAT_GRITO: return color(255, 170, 80);
    case CHAT_GLOBAL: return color(255, 255, 130);
    case CHAT_CLAN: return color(130, 255, 255);
    case CHAT_SISTEMA: return color(190, 220, 255);
    case CHAT_GRUPO:
    case CHAT_GRUPO_CHAT: return color(130, 255, 130);
    case CHAT_COMERCIO: return color(255, 200, 130);
    case CHAT_ALIANZA: return color(170, 200, 255);
    case CHAT_SUSURRO: return color(255, 150, 220);
    case CHAT_MENSAJE: return COLOR_MENSAJE;
    default: return color(255, 255, 255);
    }
}

u32 color_chat_original(u8 tipo, bool normal_path, std::string_view texto) {
    // Text after the "Name: " prefix the server builds; S_Say tests the line start for '(' (whisper-like talk).
    if (normal_path) {
        if (tipo == CHAT_GRITO) return color(255, 230, 205);
        if (tipo == 0xf) return color(255, 131, 255);
        // The server prefixes "Name: "; the original tests the spoken text.
        const size_t p = texto.find(": ");
        const std::string_view dicho = p == std::string_view::npos ? texto : texto.substr(p + 2);
        return !dicho.empty() && dicho[0] == '(' ? color(189, 238, 230) : color(255, 255, 255);
    }
    switch (tipo) {
    case 1: return color(255, 205, 205);
    case CHAT_GLOBAL: return texto.size() > 1 && texto[1] == '*' ? color(255, 0, 0) : color(172, 238, 238);
    case CHAT_CLAN: return !texto.empty() && texto[0] == '{' ? color(255, 131, 255) : color(197, 238, 180);
    case CHAT_GRUPO: return color(205, 213, 255);
    case CHAT_COMERCIO: return color(189, 164, 106);
    default: return color(255, 246, 180);  // \fH cream
    }
}

void Chat::agregar(std::string_view t, u32 c, u8 tipo) {
    LineaChat& l = lineas[total % CAPACIDAD_CHAT];
    size_t n = recorte_cp949(t, LARGO_LINEA_CHAT);
    for (size_t i = 0; i < n; ++i) l.texto[i] = t[i];
    l.largo = static_cast<u8>(n);
    l.color = c;
    l.tipo = tipo;
    ++total;
}

std::string_view Chat::globo(ObjId id, u64 ahora, u64 duracion) const {
    if (id == 0) return {};
    for (const GloboChat& g : globos) {
        if (g.id != id || ahora - g.inicio >= duracion || total - g.linea > CAPACIDAD_CHAT) continue;
        const LineaChat& l = lineas[g.linea % CAPACIDAD_CHAT];
        return {l.texto, l.largo};
    }
    return {};
}

// Colores de \fX, RGB555, para X de '0' a 'd': tabla 0x500430 del cliente original (S3EP1U.bin).
constexpr u16 PALETA_F[0x65 - 0x30] = {
    0x0000, 0x001f, 0x03e0, 0x7c00, 0x509e, 0x3421, 0x50a5, 0x5ef7, 0x2108, 0x3def, 0x5f7f, 0x63b6, 0x7afe,
    0x7fe0, 0x7fff, 0x6318, 0x4200, 0x5ca7, 0x1577, 0x7db2, 0x4a3f, 0x1eb2, 0x02e0, 0x4a52, 0x7fd6, 0x4e74,
    0x662e, 0x6737, 0x14a5, 0x5fbc, 0x675f, 0x733f, 0x7f39, 0x7f99, 0x5e8d, 0x2e91, 0x3e8c, 0x3696, 0x525a,
    0x5e38, 0x5a36, 0x6631, 0x767d, 0x102d, 0x57bd, 0x7df0, 0x5000, 0x7e1f, 0x564e, 0x3dff, 0x0014, 0x5949,
    0x3214};

u32 color_codigo(char c, u32 normal) {
    if (c == '>') return normal;  // blanco en la tabla; en los mensajes vuelve a su color
    return es_codigo_color(c) ? rgb555(PALETA_F[c - 0x30]) : normal;
}

// Original name colour (S3EP1U 0x4cc140, RGB555 stored in the object): chaotic (< 0) fades from white to full red over
// -64 per step, 0..499 stays white, lawful (>= 500) fades from white to full blue over 256 per step.
u32 color_alineamiento(i32 legal) {
    if (legal < 0) {
        const int d = 31 - std::min((-legal >> 6) + 1, 31);
        return rgb555(static_cast<u16>(0x7c00 | (d << 5) | d));
    }
    if (legal < 500) return rgb555(0x7fff);
    const int d = 31 - std::min(((legal - 500) >> 8) + 1, 31);
    return rgb555(static_cast<u16>((d << 10) | (d << 5) | 0x1f));
}

const char* con_miles(u64 v, char* buf, size_t n) {
    char t[32];
    const int d = std::snprintf(t, sizeof t, "%llu", static_cast<unsigned long long>(v));
    size_t k = 0;
    for (int i = 0; i < d && k + 1 < n; ++i) {
        if (i && (d - i) % 3 == 0 && k + 2 < n) buf[k++] = '.';
        buf[k++] = t[i];
    }
    buf[k] = 0;
    return buf;
}

size_t expandir_desc(const TablaTexto& desc, std::string_view t, char* destino, size_t capacidad) {
    Salida s{destino, capacidad};
    for (size_t i = 0; i < t.size();) {
        if (t[i] == '$' && i + 1 < t.size() && t[i + 1] >= '0' && t[i + 1] <= '9') {
            size_t f = i + 1;
            while (f < t.size() && t[f] >= '0' && t[f] <= '9') ++f;
            std::string_view r = desc.resolver(t.substr(i, f - i));
            s.poner(r);
            i = f;
        } else {
            s.poner(t.substr(i, 1));
            ++i;
        }
    }
    return s.n;
}

size_t formatear_mensaje(const TablaTexto& mensajes, const TablaTexto& desc, const Evento& e, char* destino,
                         size_t capacidad, u32& c, bool original) {
    // Original client: the default text colour is \\fH cream (255,246,180), used by 184 call sites.
    u32 normal = e.v[MS_AZUL] ? COLOR_AZUL : original ? color(255, 246, 180) : COLOR_MENSAJE;
    c = normal;
    std::string_view plantilla = mensajes.linea(static_cast<size_t>(e.v[MS_ID]));
    Salida s{destino, capacidad};
    if (plantilla.empty()) {
        // Mensaje que esta instalación no tiene: se muestra el id y los argumentos.
        char t[24];
        std::snprintf(t, sizeof t, "(mensaje %d)", e.v[MS_ID]);
        s.poner(t);
        for (int i = 0; i < e.v[MS_NARGS]; ++i) {
            s.poner(" ");
            s.n += expandir_desc(desc, e.s[i], destino + s.n, capacidad - s.n);
        }
        return s.n;
    }
    bool con_color = false;
    for (size_t i = 0; i < plantilla.size(); ++i) {
        char ch = plantilla[i];
        if (ch == '\\' && i + 2 < plantilla.size() && plantilla[i + 1] == 'f') {
            if (!con_color) c = color_codigo(plantilla[i + 2], normal);
            con_color = true;
            i += 2;
        } else if (ch == '%' && i + 1 < plantilla.size() && (plantilla[i + 1] >= '0' && plantilla[i + 1] <= '9')) {
            int k = plantilla[i + 1] - '0';
            if (k < e.v[MS_NARGS]) s.n += expandir_desc(desc, e.s[k], destino + s.n, capacidad - s.n);
            ++i;
        } else if (ch == '%' && i + 1 < plantilla.size() && (plantilla[i + 1] == 's' || plantilla[i + 1] == 'd')) {
            if (e.v[MS_NARGS] > 0) s.n += expandir_desc(desc, e.s[0], destino + s.n, capacidad - s.n);  // "%s" and "%d" = %0
            ++i;
        } else {
            s.poner(plantilla.substr(i, 1));
        }
    }
    return s.n;
}

void aplicar_chat(Chat& c, const TablaTexto& mensajes, const TablaTexto& desc, std::span<const Evento> eventos, u64 ahora) {
    char buf[LARGO_LINEA_CHAT * 2];
    for (const Evento& e : eventos) {
        if (e.tipo == Ev::MENSAJE_SERVIDOR) {
            u32 col;
            size_t n = formatear_mensaje(mensajes, desc, e, buf, sizeof buf, col, c.original_ui);
            c.agregar({buf, n}, col, CHAT_MENSAJE);
        } else if (e.tipo == Ev::CHAT) {
            u8 tipo = static_cast<u8>(e.v[0]);
            size_t n;
            if (tipo == CHAT_SUSURRO) {
                // Llega solo el remitente y el texto. Se arma como el eco que recibe quien envía
                // ("-> (destino) texto", tipo 9) pero en sentido contrario.
                Salida s{buf, sizeof buf};
                s.poner(c.original_ui ? "(" : "<- (");  // original format "(%s) %s" (0x483970)
                s.poner(e.s[1]);
                s.poner(") ");
                s.poner(e.s[0]);
                n = s.n;
            } else {
                n = expandir_desc(desc, e.s[0], buf, sizeof buf);  // los NPC hablan como "$1422: ..."
            }
            u32 linea = c.total;
            u32 col = color_chat(tipo);
            if (c.original_ui) {
                if (tipo == CHAT_SUSURRO) col = color(255, 205, 205);
                else if (tipo == CHAT_SISTEMA) col = color(255, 230, 205);  // sent-whisper echo (0x48391d)
                else col = color_chat_original(tipo, e.origen == Srv::NORMALCHAT || e.origen == Srv::NPCSHOUT, {buf, n});
            }
            c.agregar({buf, n}, col, tipo);
            // Globo sobre la cabeza: chat normal y grito de quien está a la vista.
            if (e.id != 0 && (tipo == CHAT_NORMAL || tipo == CHAT_GRITO || (c.globo_comercio && tipo == CHAT_COMERCIO))) {
                GloboChat* destino = &c.globos[0];
                for (GloboChat& g : c.globos) {
                    if (g.id == e.id) { destino = &g; break; }
                    if (g.inicio < destino->inicio) destino = &g;  // el más viejo
                }
                *destino = {e.id, ahora, linea};
            }
        }
    }
}

bool interpretar_entrada(std::string_view linea, EntradaChat& s) {
    s = {};
    while (!linea.empty() && linea.back() == ' ') linea.remove_suffix(1);
    if (linea.empty()) return false;
    switch (linea[0]) {
    case '!': s.tipo = CHAT_GRITO; break;
    case '&': s.tipo = CHAT_GLOBAL; break;
    case '$': s.tipo = CHAT_COMERCIO; break;
    case '@': s.tipo = CHAT_CLAN; break;
    case '#': s.tipo = CHAT_GRUPO; break;
    case '%': s.tipo = CHAT_ALIANZA; break;
    case '*': s.tipo = CHAT_GRUPO_CHAT; break;
    case '"': {
        std::string_view r = linea.substr(1);
        size_t esp = r.find(' ');
        if (esp == 0 || esp == std::string_view::npos) return false;
        s.susurro = true;
        s.destino = r.substr(0, esp);
        s.texto = r.substr(esp + 1);
        while (!s.texto.empty() && s.texto[0] == ' ') s.texto.remove_prefix(1);
        return !s.texto.empty();
    }
    default:
        s.texto = linea;
        return true;
    }
    s.texto = linea.substr(1);
    return !s.texto.empty();
}

}  // namespace lin

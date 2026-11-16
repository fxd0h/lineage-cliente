// Plaintext sprite list (list.spz), the user's own copy of the game's sprite table. Never shipped.
//
//   S<count> <a> <b>                       header line, ignored
//   #<gfx> <subsprites>[=<file gfx>][$] <name>     one entry per gfx, '#' at the start of a line
//   <id>.<name>(<dir> <n>,<sub>.<frame>:<ticks>[suffix] ...)       action (id < 100 or >= 121)
//   <id>.<name>(<value> ...)                       attribute (100-120): 101 shadow, 102 type,
//                                                  104 attr, 109 "n gfx" next gfx, 110 framerate
// Suffixes glued to a frame: ! hit, [N or <N sound, ]N frame effect, ^ > { } others (ignored).
// Entry layout varies: tab/space/no indent, several actions on one line, "1 10.8.9:4" for "1 10,8.9:4".
// The parser therefore scans tokens instead of lines. Frames are listed like in sprite_table.txt (the
// sprite file's last frame first), so death rows get the same rotation.
#include <cctype>
#include "plataforma/archivo_mapeado.h"
#include "recursos/tabla_sprites.h"

namespace lin {

namespace {

bool es_digito(char c) { return c >= '0' && c <= '9'; }
bool es_espacio(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

struct Lector {
    const char* p;
    const char* fin;

    bool digito() const { return p < fin && es_digito(*p); }
    void saltar_espacios() {
        while (p < fin && es_espacio(*p)) ++p;
    }
    int numero() {
        int v = 0;
        while (p < fin && es_digito(*p)) v = v * 10 + (*p++ - '0');
        return v;
    }
    int numero_con_signo() {
        saltar_espacios();
        const bool neg = p < fin && *p == '-';
        if (neg) ++p;
        const int v = numero();
        return neg ? -v : v;
    }
};

// One gfx entry as read from the text, before it is merged.
struct Entrada {
    GfxId gfx = 0;
    GfxId archivos = 0;
    i16 sombra = -1;
    u8 tipo = 0;
    u16 atributos = 0;
    GfxId siguiente = 0;
    std::vector<std::pair<u8, RangoAccion>> acciones;  // action id -> range into `frames`
    std::vector<FrameAccion> frames;
};

// Parses the frames of one action body ("1 8,0.0:2 0.1:2<478 ..."); `r.primer` indexes `frames`.
void leer_accion(Lector c, std::vector<FrameAccion>& frames, RangoAccion& r) {
    c.saltar_espacios();
    r.direccional = static_cast<u8>(c.numero());
    c.saltar_espacios();
    c.numero();  // declared count; the frames actually present decide
    if (c.p < c.fin && !es_espacio(*c.p)) ++c.p;  // ',' (or a stray '.')
    for (;;) {
        c.saltar_espacios();
        if (!c.digito()) break;
        FrameAccion f{};
        f.sonido = -1;
        f.sub = static_cast<u16>(c.numero());
        if (c.p >= c.fin || *c.p != '.') break;
        ++c.p;
        f.frame = static_cast<u16>(c.numero());
        if (c.p >= c.fin || *c.p != ':') break;
        ++c.p;
        f.duracion = static_cast<u8>(c.numero());
        while (c.p < c.fin && !es_espacio(*c.p)) {
            const char s = *c.p++;
            const bool con_numero = s == '[' || s == '<' || s == ']' || s == '^' || s == '>' || s == '{' || s == '}';
            if (s == '!') f.impacto = true;
            else if (con_numero) {
                const bool hay = c.digito();
                const int n = c.numero();
                if (hay && (s == '[' || s == '<')) f.sonido = static_cast<i16>(n);
                else if (hay && s == ']') f.efecto = static_cast<u16>(n);
            }
        }
        frames.push_back(f);
        ++r.cantidad;
    }
}

// Tokens "<id>.<name>(<body>)" of one entry's text (everything after the header numbers).
void leer_entrada(const char* p, const char* fin, Entrada& e) {
    u8 velocidad = 24;
    while (p < fin) {
        // A token starts with digits at the start of the text or after whitespace, then '.'.
        if (!es_digito(*p) || (p[-1] != '\n' && !es_espacio(p[-1]))) { ++p; continue; }
        Lector c{p, fin};
        const int id = c.numero();
        if (c.p >= fin || *c.p != '.') { p = c.p; continue; }
        const char* q = c.p;
        while (q < fin && *q != '(' && *q != '\n') ++q;
        if (q >= fin || *q != '(') { p = c.p; continue; }
        const char* cuerpo = q + 1;
        const char* cierre = cuerpo;
        while (cierre < fin && *cierre != ')' && *cierre != '\n') ++cierre;
        Lector b{cuerpo, cierre};
        if (id < 100 || id >= 121) {
            if (id < 256) {
                RangoAccion r{static_cast<u32>(e.frames.size()), 0, 0, velocidad};
                leer_accion(b, e.frames, r);
                if (id == ACC_MORIR) poner_cadaver_al_final(e.frames, r);
                if (r.cantidad > 0) e.acciones.emplace_back(static_cast<u8>(id), r);
            }
        } else {
            switch (id) {
            case 101: e.sombra = static_cast<i16>(b.numero_con_signo()); break;
            case 102: e.tipo = static_cast<u8>(b.numero_con_signo()); break;
            case 104: e.atributos = static_cast<u16>(b.numero_con_signo()); break;
            case 109: {
                b.numero_con_signo();
                const int g = b.numero_con_signo();
                e.siguiente = static_cast<GfxId>(g > 0 && g < 0xFFFF ? g : 0);
                break;
            }
            case 110: {
                const int n = b.numero_con_signo();
                velocidad = static_cast<u8>(n > 0 && n < 256 ? n : 24);
                break;
            }
            default: break;
            }
        }
        p = cierre < fin ? cierre + 1 : fin;
    }
}

}  // namespace

bool TablaSprites::fusionar_lista(std::string_view texto, ResumenLista& resumen) {
    const char* p = texto.data();
    const char* fin = p + texto.size();
    // Entry boundaries: lines that begin with '#' and a digit.
    auto inicio_entrada = [&](const char* q) { return *q == '#' && q + 1 < fin && es_digito(q[1]); };
    while (p < fin && !inicio_entrada(p)) {
        const char* nl = static_cast<const char*>(std::memchr(p, '\n', fin - p));
        p = nl ? nl + 1 : fin;
    }
    while (p < fin) {
        const char* sig = p + 1;
        for (;;) {
            const char* nl = static_cast<const char*>(std::memchr(sig, '\n', fin - sig));
            if (!nl) { sig = fin; break; }
            sig = nl + 1;
            if (sig < fin && inicio_entrada(sig)) break;
        }
        Lector c{p + 1, sig};
        Entrada e;
        e.gfx = static_cast<GfxId>(c.numero());
        e.archivos = e.gfx;
        c.saltar_espacios();
        c.numero();  // sub-sprite count
        if (c.p < c.fin && *c.p == '=') {
            ++c.p;
            e.archivos = static_cast<GfxId>(c.numero());
        }
        leer_entrada(c.p, sig, e);
        if (e.acciones.empty()) {
            ++resumen.omitidos;
        } else {
            if (e.gfx >= alias_.size()) {
                const size_t n = static_cast<size_t>(e.gfx) + 1;
                for (size_t g = alias_.size(); g < n; ++g) alias_.push_back(static_cast<GfxId>(g));
                sombra_.resize(n, -1);
                tipo_.resize(n, 0);
                atributos_.resize(n, 0);
                siguiente_.resize(n, 0);
            }
            bool existia = false;
            for (u64 a = 0; a < 256; ++a)
                existia |= indice_.quitar((static_cast<u64>(e.gfx) << 8) | a);
            ++(existia ? resumen.reemplazados : resumen.nuevos);
            alias_[e.gfx] = e.archivos;
            sombra_[e.gfx] = e.sombra;
            tipo_[e.gfx] = e.tipo;
            atributos_[e.gfx] = e.atributos;
            siguiente_[e.gfx] = e.siguiente;
            const u32 base = static_cast<u32>(frames_.size());
            frames_.insert(frames_.end(), e.frames.begin(), e.frames.end());
            for (auto& [id, r] : e.acciones) {
                r.primer += base;
                indice_.poner((static_cast<u64>(e.gfx) << 8) | id, static_cast<u32>(rangos_.size()));
                rangos_.push_back(r);
            }
        }
        p = sig;
    }
    return resumen.nuevos + resumen.reemplazados > 0;
}

bool TablaSprites::fusionar_lista_archivo(const std::string& ruta, ResumenLista& resumen) {
    std::vector<u8> t;
    return leer_archivo(ruta, t) &&
           fusionar_lista({reinterpret_cast<const char*>(t.data()), t.size()}, resumen);
}

}  // namespace lin

// Tabla de sprites en texto plano: la del propio cliente del usuario (TW13032701.txt del cliente
// taiwanés, o list.spz descifrado). No se distribuye: se lee de la instalación (ver app.cpp).
//
//   <cantidad> <a> <b>  o  S<cantidad> <a> <b>        encabezado, se ignora
//   #<gfx> <subsprites>[=<gfx de archivos>][$] <nombre>   una entrada por gfx, '#' al principio de la línea
//   <id>.<nombre>(<dir> <n>,<sub>.<frame>:<ticks>[sufijo] ...)   acción (id < 100 o >= 121)
//   <id>.<nombre>(<valor> ...)                   atributo (100-120): 101 sombra, 102 tipo, 104 atributo,
//                                                109 "n gfx" gráfico siguiente, 110 velocidad, 111 stride,
//                                                105 "n gfx..." clothes
// Sufijos pegados a un frame: ! impacto, [N o <N sonido; ]N ^ > { } se ignoran.
// El formato varía (tabulador, espacios o nada; varias acciones en una línea; "1 10.8.9:4" por
// "1 10,8.9:4"), así que se recorren tokens y no líneas. Los frames vienen como en sprite_table.txt (el
// último frame del archivo primero), así que la muerte se rota igual (poner_cadaver_al_final).
//
// Lector tomado del fork fxd0h/lineage-cliente (lista_sprites.cpp).
#include <cstring>
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

// Una entrada (un gfx) tal como viene en el texto, antes de fusionarla.
struct Entrada {
    GfxId gfx = 0;
    GfxId archivos = 0;
    i16 sombra = -1;
    u8 tipo = 0;
    u16 atributos = 0;
    GfxId siguiente = 0;
    bool zancada = false;
    std::vector<GfxId> ropa;
    std::vector<std::pair<u8, RangoAccion>> acciones;  // id de la acción -> rango en `frames`
    std::vector<FrameAccion> frames;
};

// Los frames del cuerpo de una acción ("1 8,0.0:2 0.1:2<478 ..."); `r.primer` indexa `frames`.
void leer_accion(Lector c, std::vector<FrameAccion>& frames, RangoAccion& r) {
    c.saltar_espacios();
    r.direccional = static_cast<u8>(c.numero());
    c.saltar_espacios();
    c.numero();  // cantidad declarada: mandan los frames que hay
    if (c.p < c.fin && !es_espacio(*c.p)) ++c.p;  // ',' (o un '.' suelto)
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
            }
        }
        frames.push_back(f);
        ++r.cantidad;
    }
}

// Los tokens "<id>.<nombre>(<cuerpo>)" de una entrada (lo que sigue a los números del encabezado).
void leer_entrada(const char* p, const char* fin, Entrada& e) {
    u8 velocidad = 24;
    while (p < fin) {
        // Un token empieza con dígitos al principio del texto o después de un espacio, y sigue un '.'.
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
            case 111: e.zancada = b.numero_con_signo() != 0; break;
            case 105: {
                const int n = b.numero_con_signo();
                for (int i = 0; i < n && i < 16; ++i) {
                    const int g = b.numero_con_signo();
                    if (g > 0 && g < 0xFFFF) e.ropa.push_back(static_cast<GfxId>(g));
                }
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

// Compara una entrada con los .spr de la instalación. Elige el gfx de archivos (el alias, o el propio gfx si el
// alias no tiene archivos) y quita los frames cuya imagen falta en alguna dirección con la que se dibuja. Las
// acciones que quedan vacías se quitan. false si no existe ningún archivo de la entrada.
bool validar_entrada(Entrada& e, const ContadorFrames& contador, bool& recortada) {
    std::vector<std::pair<u32, u32>> cache;  // (gfx de archivos << 16 | sub) -> cantidad de imágenes
    auto cuenta = [&](GfxId g, u16 sub) {
        const u32 k = (static_cast<u32>(g) << 16) | sub;
        for (const auto& [kk, n] : cache)
            if (kk == k) return n;
        const u32 n = contador(g, sub);
        cache.emplace_back(k, n);
        return n;
    };
    // ¿`g` tiene algún archivo de los que usan las acciones de la entrada?
    auto tiene_archivos = [&](GfxId g) {
        for (const auto& [id, r] : e.acciones)
            for (u32 i = 0; i < r.cantidad; ++i) {
                const FrameAccion& f = e.frames[r.primer + i];
                for (u16 d = 0; d < (r.direccional ? 8 : 1); ++d)
                    if (cuenta(g, static_cast<u16>(f.sub + d)) > 0) return true;
            }
        return false;
    };
    if (!tiene_archivos(e.archivos)) {
        if (e.archivos == e.gfx || !tiene_archivos(e.gfx)) return false;
        e.archivos = e.gfx;
    }
    std::vector<FrameAccion> nuevos;
    std::vector<std::pair<u8, RangoAccion>> acciones;
    for (auto& [id, r] : e.acciones) {
        RangoAccion n = r;
        n.primer = static_cast<u32>(nuevos.size());
        n.cantidad = 0;
        for (u32 i = 0; i < r.cantidad; ++i) {
            const FrameAccion& f = e.frames[r.primer + i];
            bool ok = true, alguno = false;
            for (u16 d = 0; d < (r.direccional ? 8 : 1); ++d) {
                const u32 c = cuenta(e.archivos, static_cast<u16>(f.sub + d));
                alguno |= c > 0;
                if (c > 0 && f.frame >= c) ok = false;  // si falta el archivo de una dirección, no es culpa del frame
            }
            if (ok && alguno) {
                nuevos.push_back(f);
                ++n.cantidad;
            }
        }
        if (n.cantidad != r.cantidad) recortada = true;
        if (n.cantidad > 0) acciones.emplace_back(id, n);
    }
    e.frames = std::move(nuevos);
    e.acciones = std::move(acciones);
    return !e.acciones.empty();
}

}  // namespace

bool TablaSprites::fusionar_lista(std::string_view texto, ResumenLista& resumen, const ContadorFrames* contador) {
    const size_t gfx_tabla = alias_.size();  // los gfx que sprite_table.txt ya trae (con o sin acciones)
    const char* p = texto.data();
    const char* fin = p + texto.size();
    // Cada entrada empieza en una línea con '#' y un dígito.
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
        c.numero();  // cantidad de sub-sprites
        if (c.p < c.fin && *c.p == '=') {
            ++c.p;
            e.archivos = static_cast<GfxId>(c.numero());
        }
        leer_entrada(c.p, sig, e);
        if (e.gfx >= alias_.size()) {
            const size_t n = static_cast<size_t>(e.gfx) + 1;
            for (size_t g = alias_.size(); g < n; ++g) alias_.push_back(static_cast<GfxId>(g));
            sombra_.resize(n, -1);
            tipo_.resize(n, 0);
            atributos_.resize(n, 0);
            siguiente_.resize(n, 0);
            zancada_.resize(n, false);
            ropa_.resize(n);
        }
        if (e.acciones.empty()) {
            // Sin acciones (ítems del suelo, objetos): sus atributos, si sprite_table.txt no tiene el gfx. El tipo
            // decide, por ejemplo, que un ítem del suelo se recoja con un clic. El alias, solo si tiene archivo.
            if (e.gfx >= gfx_tabla) {
                if (!contador || (*contador)(e.archivos, 0) > 0) alias_[e.gfx] = e.archivos;
                sombra_[e.gfx] = e.sombra;
                tipo_[e.gfx] = e.tipo;
                atributos_[e.gfx] = e.atributos;
                siguiente_[e.gfx] = e.siguiente;
                zancada_[e.gfx] = e.zancada;
                ropa_[e.gfx] = e.ropa;
                ++resumen.atributos;
            } else {
                ++resumen.omitidos;
            }
        } else {
            // Solo los gfx que sprite_table.txt no tiene: la lista puede venir de un cliente más nuevo y sus filas
            // de gfx que ya tenemos pueden nombrar frames o acciones que nuestro Sprite.pak no tiene.
            bool existia = false;
            for (u64 a = 0; a < 256 && !existia; ++a) existia = indice_.buscar((static_cast<u64>(e.gfx) << 8) | a);
            if (existia) {
                ++resumen.reemplazados;  // se queda la de sprite_table.txt
                p = sig;
                continue;
            }
            if (contador) {
                bool recortada = false;
                const GfxId pedido = e.archivos;
                const bool ok = validar_entrada(e, *contador, recortada);
                if (!ok || e.archivos != pedido) ++resumen.sin_archivo;
                if (!ok) {
                    ++resumen.omitidos;
                    p = sig;
                    continue;
                }
                if (recortada) ++resumen.recortados;
            }
            ++resumen.nuevos;
            alias_[e.gfx] = e.archivos;
            sombra_[e.gfx] = e.sombra;
            tipo_[e.gfx] = e.tipo;
            atributos_[e.gfx] = e.atributos;
            siguiente_[e.gfx] = e.siguiente;
            zancada_[e.gfx] = e.zancada;
            ropa_[e.gfx] = e.ropa;
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

bool TablaSprites::fusionar_lista_archivo(const std::string& ruta, ResumenLista& resumen,
                                          const ContadorFrames* contador) {
    std::vector<u8> t;
    return leer_archivo(ruta, t) &&
           fusionar_lista({reinterpret_cast<const char*>(t.data()), t.size()}, resumen, contador);
}

}  // namespace lin

// Correo (C_Mail / S_Mail). "/mail" abre la bandeja (0 normal, 1 clan, 2 guardadas); un clic en una
// carta la lee (y queda leída en el servidor). Desde la carta: borrar o guardarla. "Write" escribe
// una a un personaje (50 adena) o a todo el clan (1000 adena por miembro). El asunto y el texto van
// en UTF-16LE; el cliente solo escribe y muestra ASCII (lo demás queda como '?').
// Una carta nueva llega como S_Mail 0x50: aviso en el chat (y se agrega si su bandeja está abierta).
// La ventana usa los gráficos del correo original (Sprite.pak): el papel (656; el 577 trae los rótulos
// impresos y se pisa con los campos), la
// marca de leída (657), cerrar (658), Write (662), Delete (664), Cancel (578) y Send (582), cada
// botón con su imagen resaltada en N + 1. El original no trae un ícono de "correo nuevo": el sobre
// arriba a la izquierda (cartas sin leer; un clic abre el correo) es un dibujo propio.
// Con la UI simple no se usa el papel: un recuadro oscuro, texto claro y botones de texto.
#include <algorithm>
#include <cstdio>
#include <ctime>
#include "app/app.h"

namespace lin {

int texto_ajustado(Graficos& g, std::string_view t, int x, int y, int ancho, u64 clave, u32 color, int max_lineas);
std::string_view sin_espacios(std::string_view s);

namespace {

constexpr int ANCHO = 238, ALTO = 350, FILA = 26;
constexpr u32 IMG_PAPEL = 656, IMG_LEIDA = 657, IMG_CERRAR = 658, IMG_WRITE = 662,
              IMG_DELETE = 664, IMG_CANCEL = 578, IMG_SEND = 582;
// Original layout (S3EP1U.bin, mail list window ctor 0x417a90, draw 0x417d50; compose ctor 0x455800): the list
// is paper 656 (238x350), 8 rows of 30 px from y=50, buttons placed in window coordinates.
constexpr u32 IMG_READ = 660, IMG_PAGE_UP = 666, IMG_PAGE_DOWN = 668, IMG_PAPER_READ = 671, IMG_PAPER_WRITE = 577,
              IMG_CLOSE_READ = 580;
constexpr int ROWS = 8, ROW_H = 30, ROW_Y = 50;
int selected_row = -1, page = 0;  // list state of the original window (reset on opening a mailbox)
// Los colores del texto: tinta sobre el papel, o los de la UI simple (se eligen en cada cuadro).
u32 TINTA, TINTA_SUAVE, TINTA_VIVA;
constexpr const char* BANDEJAS[3] = {"Inbox", "Clan", "Saved"};

// The original list / letter / compose windows open at the origin of the 640x480 block (0x417d50); the simple UI centres them.
int x0(const App& a) { return a.cfg.ui_simple ? (a.gpu.ancho() - ANCHO) / 2 : (a.gpu.ancho() - 640) / 2; }
int y0(const App& a) { return a.cfg.ui_simple ? 50 : 0; }

// Texto que se puede tocar (sobre el papel): true con un clic.
bool vinculo(Graficos& g, const EntradaFrame& e, std::string_view t, int x, int y, u64 clave, bool activo) {
    const int w = ancho_texto(t.data(), t.size());
    const bool encima = ui::sobre(e, x, y - 2, w, ALTO_GLIFO + 4);
    g.texto(t, static_cast<float>(x), static_cast<float>(y), clave, activo || encima ? TINTA_VIVA : TINTA_SUAVE);
    if (activo) g.gpu().rectangulo(x, y + ALTO_GLIFO + 1, w, 1, clave, TINTA_VIVA);
    return encima && e.clic;
}

int sin_leer(const Correo& c) {
    return static_cast<int>(std::count_if(c.cartas.begin(), c.cartas.end(), [](const Carta& x) { return !x.leida && !x.enviada; }));
}

void abrir_bandeja(App& a, u8 b) {
    Correo& c = a.correo;
    c.abierto = true;
    c.bandeja = b;
    c.cartas.clear();  // el servidor no manda nada si está vacía
    c.leyendo = 0;
    selected_row = -1;
    page = 0;
    enviar(a, cmd::correo(a.op, b));
}

// Copy of s clipped to a pixel width (no allocation: the caller gives the buffer).
std::string_view clip(std::string_view s, int w, char* buf, size_t cap) {
    size_t n = std::min(s.size(), cap - 1);
    while (n > 0 && ancho_texto(s.data(), n) > w) --n;
    std::copy_n(s.data(), n, buf);
    buf[n] = 0;
    return {buf, n};
}

// The original mail window (ui_simple == 0): paper 656 list with 8 rows, paper 671 reading, paper 577 writing.
void draw_original_mail(App& a) {
    Correo& c = a.correo;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = x0(a), y = y0(a);
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 72), kf = clave_orden(CAPA_UI, 0);
    TINTA = rgba(65, 65, 65), TINTA_SUAVE = rgba(105, 98, 88), TINTA_VIVA = rgba(150, 40, 20);  // ink 0x500510 = RGB555 0x2108
    char buf[96];
    char txt[200];
    auto text = [&](std::string_view t, int tx, int ty, u32 col) {
        g.texto(t, static_cast<float>(x + tx), static_cast<float>(y + ty), kt, col);
    };

    if (c.escribiendo) {
        g.gpu().dibujar(g.img(IMG_PAPER_WRITE), x, y, kf);
        // The printed labels From / To / Subject sit at y 53 / 69 / 89; the entry fields run on their right.
        text(c.al_clan ? "Pledge: 1000 adena each" : "50 adena", 85, 46, TINTA_SUAVE);  // on the From row (the exe shows the own name)
        const char* rotulos[3] = {c.al_clan ? "Pledge" : "To", "Subject", "Text"};
        (void)rotulos;
        std::string* campos[3] = {&c.destino, &c.asunto, &c.cuerpo};
        const size_t maximos[3] = {16, 40, 400};
        // Paper 577 prints From / To / Subject at y 52 / 66 / 89; the To field is the exe's second edit (85,64) 136x16 (ctor
        // 0x455800), the subject sits on the Subject row and the body fills (45,105) 192x160 (the exe has no subject edit:
        // that window sends a different packet, so the l1j-en mail needs this extra field).
        const int fy[3] = {64, 86, 105}, fx[3] = {85, 85, 45}, fw[3] = {136, 136, 192}, fh[3] = {16, 16, 160};
        for (int i = 0; i < 3; ++i) {
            if (e.clic && ui::sobre(e, x + fx[i], y + fy[i], fw[i], fh[i])) c.foco = i;
            if (i < 2) {
                g.gpu().rectangulo(x + fx[i], y + fy[i] + 15, fw[i], 1, kf + 1, c.foco == i ? TINTA_VIVA : TINTA_SUAVE);
                ui::caja_texto(g, e, *campos[i], x + fx[i] + 2, y + fy[i] + 1, fw[i] - 4, c.foco == i, false, maximos[i]);
            }
        }
        if (e.tab) c.foco = (c.foco + 1) % 3;
        if (c.foco == 2) {  // the body is a multi-line area: ASCII and Backspace
            for (int i = 0; i < e.ntexto; ++i)
                if (e.texto[i] >= 0x20 && e.texto[i] < 0x7F && c.cuerpo.size() < maximos[2]) c.cuerpo.push_back(e.texto[i]);
            if (e.retroceso && !c.cuerpo.empty()) c.cuerpo.pop_back();
        }
        texto_ajustado(g, c.cuerpo, x + 45, y + 105, 192, kt, TINTA, 10);
        if (vinculo(g, e, c.al_clan ? "To a character instead" : "To the pledge instead", x + 45, y + 276, kt, false))
            c.al_clan = !c.al_clan;
        if (!c.destino.empty() && !c.asunto.empty() && ui::boton(g, e, IMG_SEND, x + 97, y + 298)) {
            enviar(a, cmd::escribir_correo(a.op, c.al_clan, sin_espacios(c.destino), sin_espacios(c.asunto), sin_espacios(c.cuerpo)));
            c.escribiendo = false;
            c.asunto.clear();
            c.cuerpo.clear();
        }
        if (ui::boton(g, e, IMG_CANCEL, x + 145, y + 298) || e.escape) {
            c.escribiendo = false;
            e.escape = false;
        }
        e.ntexto = 0;
        e.enter = false;
        return;
    }

    if (c.leyendo) {
        g.gpu().dibujar(g.img(IMG_PAPER_READ), x, y, kf);
        const auto it = std::find_if(c.cartas.begin(), c.cartas.end(), [&](const Carta& m) { return m.id == c.leyendo; });
        if (it != c.cartas.end()) {
            std::snprintf(txt, sizeof txt, "%s %s", it->enviada ? "To" : "From", it->otro.c_str());
            text(clip(txt, 200, buf, sizeof buf), 20, 20, TINTA_SUAVE);
            texto_ajustado(g, it->asunto, x + 20, y + 36, 200, kt, TINTA, 2);
        }
        texto_ajustado(g, c.texto, x + 20, y + 70, 200, kt, TINTA, 16);
        if (ui::boton(g, e, IMG_DELETE, x + 60, y + 302)) enviar(a, cmd::correo(a.op, static_cast<u8>(0x30 + c.bandeja), c.leyendo));
        if (c.bandeja == 0 && vinculo(g, e, "Keep", x + 20, y + 312, kt, false)) enviar(a, cmd::correo(a.op, 0x40, c.leyendo));
        if (ui::boton(g, e, IMG_CLOSE_READ, x + 120, y + 299) || e.escape) {
            c.leyendo = 0;
            e.escape = false;
        }
        return;
    }

    g.gpu().dibujar(g.img(IMG_PAPEL), x, y, kf);
    // The original has one mailbox per window: the title (y 15) shows it, a click switches to the next.
    const int pages = std::max(1, (static_cast<int>(c.cartas.size()) + ROWS - 1) / ROWS);
    page = std::clamp(page, 0, pages - 1);
    std::snprintf(txt, sizeof txt, "%s  %d/%d", BANDEJAS[c.bandeja], page + 1, pages);
    text(txt, 30, 15, TINTA);
    if (e.clic && ui::sobre(e, x + 30, y + 13, 150, 14)) abrir_bandeja(a, static_cast<u8>((c.bandeja + 1) % 3));
    if (c.cartas.empty()) text("No mail.", 30, ROW_Y, TINTA_SUAVE);
    const int n = static_cast<int>(c.cartas.size());
    for (int r = 0; r < ROWS && page * ROWS + r < n; ++r) {
        const int i = page * ROWS + r, fy = ROW_Y + r * ROW_H;
        const Carta& m = c.cartas[static_cast<size_t>(i)];
        const bool over = ui::sobre(e, x + 16, y + fy - 2, 200, ROW_H - 2);
        if (over && e.clic) selected_row = i;
        const u32 col = m.leida ? TINTA_SUAVE : TINTA;
        if (selected_row == i) g.gpu().dibujar(g.img(IMG_LEIDA), x + 18, y + fy, kf + 2);
        std::snprintf(txt, sizeof txt, "%04d", static_cast<int>(m.id % 10000));  // 0x500308 "%04d"
        text(clip(txt, 24, buf, sizeof buf), 30, fy, col);
        text(clip(m.otro, 72, buf, sizeof buf), 60, fy, col);
        const std::time_t f = static_cast<std::time_t>(m.fecha);
        char fecha[32] = "";
        if (const std::tm* tm = std::localtime(&f)) std::strftime(fecha, sizeof fecha, "%y/%m/%d", tm);
        text(clip(fecha, 48, buf, sizeof buf), 135, fy, TINTA_SUAVE);  // the original keeps an 8-character yy/mm/dd string here
        text(clip(m.asunto, 160, buf, sizeof buf), 30, fy + 12, col);
    }
    if (ui::sobre(e, x, y, ANCHO, ALTO) && e.rueda) page = std::clamp(page - (e.rueda > 0 ? 1 : -1), 0, pages - 1);
    // Buttons (window coordinates from the constructor): close, Read, Write, Delete, page up / down.
    if (ui::boton(g, e, IMG_CERRAR, x + 209, y + 305) || e.escape) {
        c.abierto = false;
        e.escape = false;
        return;
    }
    const bool has_sel = selected_row >= 0 && selected_row < n;
    if (ui::boton(g, e, IMG_READ, x + 34, y + 332) && has_sel)
        enviar(a, cmd::correo(a.op, static_cast<u8>(0x10 + c.bandeja), c.cartas[static_cast<size_t>(selected_row)].id));
    if (ui::boton(g, e, IMG_WRITE, x + 139, y + 304)) {
        c.escribiendo = true;
        c.foco = c.destino.empty() ? 0 : 1;
    }
    if (ui::boton(g, e, IMG_DELETE, x + 171, y + 304) && has_sel)
        enviar(a, cmd::correo(a.op, static_cast<u8>(0x30 + c.bandeja), c.cartas[static_cast<size_t>(selected_row)].id));
    if (ui::boton(g, e, IMG_PAGE_UP, x + 64, y + 311) && page > 0) --page;
    if (ui::boton(g, e, IMG_PAGE_DOWN, x + 99, y + 311) && page + 1 < pages) ++page;
}

}  // namespace

bool comando_correo(App& a, std::string_view orden) {
    if (orden != "mail" && orden != "Mail") return false;
    abrir_bandeja(a, 0);
    return true;
}

void eventos_correo(App& a, const Evento& e) {
    if (e.tipo != Ev::CORREO) return;
    Correo& c = a.correo;
    const int t = e.v[CO_TIPO];
    if (t <= 2) {
        if (!e.v[CO_LISTA]) {
            c.bandeja = static_cast<u8>(t);
            c.cartas.clear();
            return;
        }
        c.cartas.push_back({e.id, static_cast<u32>(e.v[CO_FECHA]), e.v[CO_LEIDA] != 0, e.v[CO_ENVIADA] != 0,
                            std::string(e.s[0]), std::string(e.s[1])});
        if (t == 0) c.nuevas = sin_leer(c);  // el servidor manda la bandeja al entrar: el sobre
    } else if (t >= 0x10 && t <= 0x12) {
        c.leyendo = e.id;
        c.texto.assign(e.s[1]);
        for (Carta& x : c.cartas)
            if (x.id == e.id) x.leida = true;
        if (c.bandeja == 0) c.nuevas = sin_leer(c);
    } else if (t == 0x20 || t == 0x21) {
        a.chat.agregar(e.v[CO_OK] ? "The mail was sent." : "The mail could not be sent (the mailbox is full).",
                       rgba(255, 230, 120), CHAT_SISTEMA);
    } else if ((t >= 0x30 && t <= 0x32) || t == 0x40 || t == 0x41) {
        std::erase_if(c.cartas, [&](const Carta& x) { return x.id == e.id; });  // borrada o pasada a guardadas
        if (c.leyendo == e.id) c.leyendo = 0;
    } else if (t == 0x50 && !e.v[CO_ENVIADA]) {
        char aviso[160];
        std::snprintf(aviso, sizeof aviso, "New mail from %.*s: %.*s (/mail)", static_cast<int>(e.s[0].size()), e.s[0].data(),
                      static_cast<int>(e.s[1].size()), e.s[1].data());
        a.chat.agregar(aviso, rgba(255, 230, 120), CHAT_SISTEMA);
        ++c.nuevas;
        if (c.abierto && c.bandeja == 0) c.cartas.push_back({e.id, static_cast<u32>(std::time(nullptr)), false, false,
                                                               std::string(e.s[0]), std::string(e.s[1])});
    }
}

bool sobre_correo(const App& a) {
    const Correo& c = a.correo;
    return (c.abierto && ui::sobre(a.entrada, x0(a), y0(a), ANCHO, ALTO)) || (c.nuevas > 0 && !c.abierto && ui::sobre(a.entrada, 8, 8, 56, 20));
}

// El aviso de correo nuevo: un sobre arriba a la izquierda con la cantidad; un clic abre el correo.
void dibujar_sobre(App& a) {
    Correo& c = a.correo;
    if (c.nuevas <= 0 || c.abierto) return;
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    constexpr int X = 10, Y = 10, W = 24, H = 16;
    const u64 k = clave_orden(CAPA_UI, 40), kl = clave_orden(CAPA_UI, 41);
    const bool encima = ui::sobre(e, X - 2, Y - 2, W + 30, H + 4);
    // Que se note: titila suave las primeras veces que se ve (cada medio segundo).
    const bool fuerte = encima || (e.ahora_ms / 500) % 2 == 0;
    g.gpu().rectangulo(X - 1, Y - 1, W + 2, H + 2, k, rgba(70, 45, 20, 255));
    g.gpu().rectangulo(X, Y, W, H, k + 1, fuerte ? rgba(250, 240, 210) : rgba(225, 210, 175));
    for (int i = 0; i <= W / 2; ++i) {  // la solapa: una V de la esquina de arriba al centro
        const int dy = i * (H - 6) / (W / 2);
        g.gpu().rectangulo(X + i, Y + dy, 1, 1, kl, rgba(120, 85, 50));
        g.gpu().rectangulo(X + W - 1 - i, Y + dy, 1, 1, kl, rgba(120, 85, 50));
    }
    g.gpu().rectangulo(X + W / 2 - 2, Y + H - 7, 4, 4, kl, rgba(180, 40, 30));  // el sello
    char t[16];
    std::snprintf(t, sizeof t, "%d", c.nuevas);
    g.texto_sombra(t, static_cast<float>(X + W + 5), static_cast<float>(Y + 2), clave_orden(CAPA_UI_TEXTO, 41), rgba(255, 230, 120));
    if (encima) g.texto_sombra("New mail", static_cast<float>(X), static_cast<float>(Y + H + 4), clave_orden(CAPA_UI_TEXTO, 41),
                               ui::COLOR_TEXTO);
    if (encima && e.clic) abrir_bandeja(a, 0);
}

void dibujar_correo(App& a) {
    dibujar_sobre(a);
    Correo& c = a.correo;
    if (!c.abierto) return;
    if (!a.cfg.ui_simple && a.g->img(IMG_PAPEL).valida()) return draw_original_mail(a);
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    const int x = x0(a), y = y0(a);
    // El papel del original (o un recuadro, con la UI simple o si no está).
    const RegionAtlas& papel = g.img(IMG_PAPEL);
    const bool simple = a.cfg.ui_simple || !papel.valida();
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 72), kf = clave_orden(CAPA_UI, simple ? 3 : 0);  // sobre el recuadro (2)
    if (simple) {
        ui::recuadro(g, x, y, ANCHO, ALTO, 235);
        TINTA = ui::COLOR_TEXTO, TINTA_SUAVE = rgba(170, 160, 140), TINTA_VIVA = rgba(255, 220, 130);
    } else {
        g.gpu().dibujar(papel, x, y, kf);
        TINTA = rgba(60, 38, 20), TINTA_SUAVE = rgba(120, 95, 65), TINTA_VIVA = rgba(150, 40, 20);
    }
    // Los botones: la imagen del original o uno de texto.
    auto boton = [&](u32 img, std::string_view t, int bx, int by) {
        return simple ? ui::boton_texto(g, e, t, bx, by, 64, 20) : ui::boton(g, e, img, bx, by);
    };
    g.texto("Mail", static_cast<float>(x + 20), static_cast<float>(y + 22), kt, TINTA);
    char txt[200];

    if (c.escribiendo) {
        g.texto(c.al_clan ? "To the pledge: 1000 adena each" : "To a character: 50 adena", static_cast<float>(x + 20),
                static_cast<float>(y + 40), kt, TINTA_SUAVE);
        const char* rotulos[3] = {c.al_clan ? "Pledge" : "To", "Subject", "Text"};
        std::string* campos[3] = {&c.destino, &c.asunto, &c.cuerpo};
        const size_t maximos[3] = {16, 40, 400};
        for (int i = 0; i < 3; ++i) {
            const int fy = y + 62 + i * 26;
            g.texto(rotulos[i], static_cast<float>(x + 20), static_cast<float>(fy + 3), kt, TINTA);
            if (e.clic && ui::sobre(e, x + 72, fy, ANCHO - 92, 18)) c.foco = i;
            g.gpu().rectangulo(x + 72, fy, ANCHO - 92, 18, kf + 1, simple ? rgba(40, 32, 25, 255) : rgba(70, 50, 30, 210));
            ui::caja_texto(g, e, *campos[i], x + 75, fy + 2, ANCHO - 98, c.foco == i, false, maximos[i]);
        }
        if (e.tab) c.foco = (c.foco + 1) % 3;
        texto_ajustado(g, c.cuerpo, x + 20, y + 144, ANCHO - 40, kt, TINTA, 9);
        if (vinculo(g, e, c.al_clan ? "To a character instead" : "To the pledge instead", x + 20, y + ALTO - 70, kt, false))
            c.al_clan = !c.al_clan;
        const int by = y + ALTO - 50;
        if (!c.destino.empty() && !c.asunto.empty() && boton(IMG_SEND, "Send", x + 30, by)) {
            enviar(a, cmd::escribir_correo(a.op, c.al_clan, sin_espacios(c.destino), sin_espacios(c.asunto), sin_espacios(c.cuerpo)));
            c.escribiendo = false;
            c.asunto.clear();
            c.cuerpo.clear();
        }
        if (boton(IMG_CANCEL, "Cancel", x + ANCHO - 94, by) || e.escape) {
            c.escribiendo = false;
            e.escape = false;
        }
        e.ntexto = 0;
        e.enter = false;
        return;
    }

    // Cerrar (o volver a la lista, leyendo una carta) y las bandejas.
    if (simple) g.texto("[x]", static_cast<float>(x + ANCHO - 26), static_cast<float>(y + 6), kt, TINTA);
    if ((simple ? e.clic && ui::sobre(e, x + ANCHO - 28, y + 2, 26, 20) : ui::boton(g, e, IMG_CERRAR, x + ANCHO - 34, y + 16)) ||
        e.escape) {
        if (c.leyendo) c.leyendo = 0;
        else c.abierto = false;
        e.escape = false;
        return;
    }
    for (u8 b = 0; b < 3; ++b)
        if (vinculo(g, e, BANDEJAS[b], x + 70 + b * 50, y + 22, kt, c.bandeja == b)) abrir_bandeja(a, b);

    if (c.leyendo) {
        const auto it = std::find_if(c.cartas.begin(), c.cartas.end(), [&](const Carta& m) { return m.id == c.leyendo; });
        if (it != c.cartas.end()) {
            std::snprintf(txt, sizeof txt, "%s %s", it->enviada ? "To" : "From", it->otro.c_str());
            g.texto(txt, static_cast<float>(x + 20), static_cast<float>(y + 48), kt, TINTA_SUAVE);
            texto_ajustado(g, it->asunto, x + 20, y + 64, ANCHO - 40, kt, TINTA, 2);
        }
        texto_ajustado(g, c.texto, x + 20, y + 98, ANCHO - 40, kt, TINTA, 12);
        if (boton(IMG_DELETE, "Delete", x + 22, y + ALTO - 48)) enviar(a, cmd::correo(a.op, static_cast<u8>(0x30 + c.bandeja), c.leyendo));
        if (c.bandeja == 0 && vinculo(g, e, "Keep", x + ANCHO - 70, y + ALTO - 42, kt, false))
            enviar(a, cmd::correo(a.op, 0x40, c.leyendo));
        return;
    }

    if (c.cartas.empty()) g.texto("No mail.", static_cast<float>(x + 20), static_cast<float>(y + 50), kt, TINTA_SUAVE);
    for (size_t i = 0; i < c.cartas.size() && i < 10; ++i) {
        const Carta& m = c.cartas[i];
        const int fy = y + 46 + static_cast<int>(i) * FILA;
        const bool encima = ui::sobre(e, x + 16, fy, ANCHO - 32, FILA - 2);
        if (encima) g.gpu().rectangulo(x + 16, fy, ANCHO - 32, FILA - 2, kf + 1, simple ? rgba(60, 50, 35, 230) : rgba(150, 110, 60, 90));
        if (m.leida && !simple) g.gpu().dibujar(g.img(IMG_LEIDA), x + ANCHO - 30, fy + 2, kf + 2);
        g.texto(m.asunto, static_cast<float>(x + 20), static_cast<float>(fy + 2), kt, m.leida ? TINTA_SUAVE : TINTA);
        const std::time_t f = static_cast<std::time_t>(m.fecha);
        char fecha[32] = "";
        if (const std::tm* tm = std::localtime(&f)) std::strftime(fecha, sizeof fecha, "%m-%d %H:%M", tm);
        std::snprintf(txt, sizeof txt, "%s %s  %s", m.enviada ? "to" : "from", m.otro.c_str(), fecha);
        g.texto(txt, static_cast<float>(x + 20), static_cast<float>(fy + 13), kt, TINTA_SUAVE);
        if (encima && e.clic) enviar(a, cmd::correo(a.op, static_cast<u8>(0x10 + c.bandeja), m.id));
    }
    if (boton(IMG_WRITE, "Write", x + 22, y + ALTO - 46)) {
        c.escribiendo = true;
        c.foco = c.destino.empty() ? 0 : 1;
    }
}

}  // namespace lin

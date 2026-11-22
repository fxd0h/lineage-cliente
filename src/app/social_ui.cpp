// Entregable 9: social.
//  - Preguntas de sí o no del servidor (S_MessageYN: intercambio, grupo, entrar al clan...): un
//    cuadro con el mensaje de string-e.tbl y Yes / No (Enter / Escape); la respuesta es C_Attr.
//  - Intercambio: S_Trade abre la ventana. Arriba lo que ofrece uno (arrastrando ítems desde el
//    inventario, o doble clic en el inventario), abajo lo que ofrece el otro; OK (C_TradeOK) y
//    Cancel. S_TradeStatus la cierra (terminado o cancelado).
//  - Ventana Social (botón del apretón de manos o "Social"): grupo, intercambio y clan, y los
//    comandos de chat que hacen lo mismo (/invite, /trade, /pledge...).
//  - Nombres: el título arriba, el emblema del clan a la izquierda y el color según el alineamiento
//    (ver dibujar_personajes).
#include <charconv>
#include <cstring>
#include <filesystem>
#include <fstream>
#include "app/app.h"
#include "app/item_icon.h"

namespace lin {

namespace {

constexpr int ANCHO_PREGUNTA = 300, ALTO_PREGUNTA = 92;
constexpr int ANCHO_INTERCAMBIO = ui::VENTANA_ANCHO, ALTO_INTERCAMBIO = 300;
constexpr int CELDA = 36, COLUMNAS = 6, FILAS = 3;
// Original trade window (client ctor 0x42bf20, draw 0x42d280): background 1199 at (7,10), frame 1201, ornaments 1200 at
// (2,12), close button at (174,18), OK 1202 at (53,282), Cancel 1204 at (100,282); two 4x3 grids of 34 px cells at x 27,
// the one you offer at y 47 and the other side's at y 175.
constexpr int TRADE_W = 200, TRADE_H = 308, TRADE_CELL = 34, TRADE_COLS = 4, TRADE_ROWS = 3, TRADE_X = 27;
constexpr int TRADE_MINE_Y = 47, TRADE_OTHER_Y = 175;
constexpr int TRADE_SCROLL_X = 168, TRADE_SCROLL_H = 105;
constexpr int ANCHO_SOCIAL = 220, ALTO_SOCIAL = 230;

bool igual_sin_mayusculas(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if ((a[i] | 0x20) != (b[i] | 0x20)) return false;
    return true;
}

// Objeto a la vista con ese nombre (personajes: el nombre tal cual, sin "$N").
ObjId buscar_por_nombre(const App& a, std::string_view nombre) {
    const TablaObjetos& o = a.mundo.objetos;
    for (Slot s = 0; s < o.tamano(); ++s)
        if (igual_sin_mayusculas(texto(a.mundo, o.col<O_NOMBRE>()[s]), nombre)) return o.ids()[s];
    return 0;
}

void aviso(App& a, std::string_view t) { a.chat.agregar(t, rgba(255, 230, 120), CHAT_SISTEMA); }

}  // namespace

// Class byte of the selected character (char-list record +0x1e, [0x81b534]): 0 is the prince.
int clase_personaje(const App& a) {
    return a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size()) ? a.pjs[static_cast<size_t>(a.pj_sel)].v[PJ_CLASE] : -1;
}

namespace {

// Client string table line as a std::string (the table is the US string-e.tbl).
std::string cadena(const App& a, int id) { return std::string(a.mensajes.linea(static_cast<size_t>(id))); }

// The original input box (0x4451d0): the prompt goes to the chat log and the chat line takes over until Enter / Esc.
void abrir_prompt(App& a, std::string prompt, std::string_view accion) {
    cancelar_prompt(a);
    a.chat_prompt.activo = true;
    a.chat_prompt.accion = std::string(accion);
    aviso(a, prompt);
    a.chat_entrada.clear();
    a.escribiendo_chat = true;
}


// Clan emblem upload (action "emblem", 0x4070c0) and check (action "checkbmp", 0x406fc0). The file is a 16 x 12,
// 24-bit, uncompressed BMP (12 or 40 byte info header); it becomes 384 bytes of RGB555 (0x4247a0: R >> 3 << 10 |
// G >> 3 << 5 | B >> 3, top row first). A relative name is looked up in the game folder.
bool leer_bmp_emblema(const App& a, std::string_view nombre, u8* out) {
    namespace fs = std::filesystem;
    fs::path ruta{std::string(nombre)};
    if (ruta.is_relative() && fs::exists(fs::path(a.cfg.carpeta_juego) / ruta)) ruta = fs::path(a.cfg.carpeta_juego) / ruta;
    std::ifstream f(ruta, std::ios::binary);
    if (!f) return false;
    const std::vector<char> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto u16le = [&](size_t o) { return o + 2 <= b.size() ? static_cast<u32>(static_cast<u8>(b[o]) | static_cast<u8>(b[o + 1]) << 8) : 0u; };
    auto u32le = [&](size_t o) { return u16le(o) | u16le(o + 2) << 16; };
    if (b.size() < 26 || b[0] != 'B' || b[1] != 'M') return false;
    const u32 pixeles = u32le(10), cabecera = u32le(14);
    u32 ancho, alto, planos, bits, compresion = 0;
    if (cabecera == 12) {  // OS/2 core header: signed 16-bit sizes
        ancho = u16le(18); alto = u16le(20); planos = u16le(22); bits = u16le(24);
    } else if (cabecera == 40 && b.size() >= 54) {
        ancho = u32le(18); alto = u32le(22); planos = u16le(26); bits = u16le(28); compresion = u32le(30);
    } else {
        return false;
    }
    if (ancho != 16 || alto != 12 || planos != 1 || bits != 24 || compresion != 0) return false;
    if (size_t{pixeles} + 16 * 12 * 3 > b.size()) return false;
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 16; ++x) {
            const u8* p = reinterpret_cast<const u8*>(b.data()) + pixeles + static_cast<size_t>(11 - y) * 48 + static_cast<size_t>(x) * 3;
            const u16 c = static_cast<u16>((p[2] >> 3) << 10 | (p[1] >> 3) << 5 | (p[0] >> 3));
            out[(y * 16 + x) * 2] = static_cast<u8>(c);
            out[(y * 16 + x) * 2 + 1] = static_cast<u8>(c >> 8);
        }
    return true;
}

void subir_emblema(App& a, std::string_view nombre) {
    const u64 ahora = a.entrada.ahora_ms;
    const Slot yo = a.mundo.objetos.buscar(a.mundo.jugador.id);
    if (yo == SIN_SLOT) return aviso(a, cadena(a, 219));
    const i32 rango = a.mundo.jugador.rango_clan;
    // The clan id field is 0 without an emblem in 3.80C (it is the emblem id), so also look at the rank (0xb = none).
    if (a.mundo.objetos.col<O_CLAN>()[yo] == 0 && rango == 0xb) return aviso(a, cadena(a, 220));  // not in a pledge
    if (rango != 0 && rango != a.op.perfil.rango_principe) return aviso(a, cadena(a, 785));  // not the prince
    if (a.t_emblema_subido && ahora < a.t_emblema_subido + 59876) return aviso(a, cadena(a, 229));  // one per minute
    u8 datos[BYTES_EMBLEMA];
    if (!leer_bmp_emblema(a, nombre, datos)) return aviso(a, "Incorrect BMP File format");
    enviar(a, cmd::subir_emblema(a.op, datos));
    a.t_emblema_subido = ahora;
    a.emblemas_pedidos.clear();  // the server answers with S_Emblem to everybody
}

void ver_emblema(App& a, std::string_view nombre) {
    u8 datos[BYTES_EMBLEMA];
    if (!leer_bmp_emblema(a, nombre, datos)) return aviso(a, "Incorrect BMP File format");
    a.g->poner_emblema_vista({datos, BYTES_EMBLEMA});
    a.t_vista_emblema = a.entrada.ahora_ms + 10000;  // shown above the character for 10 s (0x407068)
}

// 'y' / 'Y' answers of the join / leave confirmations (0x405f60, 0x405fb0; the Japanese full-width forms are not typed here).
bool dijo_si(std::string_view t) { return !t.empty() && (t[0] == 'y' || t[0] == 'Y'); }

}  // namespace

// Del / Backspace on a selected bookmark row (0x418f70): the yes/no text prompt with string 0x34e; `y` deletes it.
void abrir_prompt_borrar_marcador(App& a, ObjId marcador) {
    abrir_prompt(a, cadena(a, 0x34e), "delbookmark");
    a.chat_prompt.id = marcador;
}

void abrir_prompt_marcador(App& a) { abrir_prompt(a, cadena(a, 0x246), "bookmark"); }

void cancelar_prompt(App& a) {
    a.chat_prompt = {};
}

// Callback of the input box once Enter is pressed (the callbacks at 0x40d470, 0x40d4d0, 0x406600, 0x406550, 0x405f60,
// 0x405fb0, 0x40d120, 0x40d180, 0x40d1e0, 0x40d3b0, 0x4086f0). The box always closes afterwards (0x41f1d0).
void confirmar_prompt(App& a, std::string_view texto) {
    const std::string accion = a.chat_prompt.accion;
    const PromptChat p = a.chat_prompt;
    cancelar_prompt(a);
    if (accion == "soltar" || accion == "dar" || accion == "ofrecer") {  // like pick up: empty = the whole stack, 0 or more than it = nothing
        u32 n = p.maximo;
        if (!texto.empty()) {
            long v = 0;
            std::from_chars(texto.data(), texto.data() + texto.size(), v);
            n = v > 0 ? static_cast<u32>(v) : 0;
        }
        if (n == 0 || n > p.maximo) return;
        if (accion == "ofrecer") {
            if (a.intercambio.abierto && !a.intercambio.ok && a.mundo.inventario.items.buscar(p.id) != SIN_SLOT)
                enviar(a, cmd::ofrecer_item(a.op, p.id, n));
        } else if (accion == "soltar") {
            const Slot yo = a.mundo.objetos.buscar(a.mundo.jugador.id);
            if (yo != SIN_SLOT && a.mundo.inventario.items.buscar(p.id) != SIN_SLOT)
                enviar(a, cmd::soltar(a.op, a.mundo.objetos.col<O_X>()[yo], a.mundo.objetos.col<O_Y>()[yo], p.id, n));
        } else if (const Slot so = a.mundo.objetos.buscar(p.objetivo);
                   so != SIN_SLOT && a.mundo.inventario.items.buscar(p.id) != SIN_SLOT) {
            const TablaObjetos& o = a.mundo.objetos;
            enviar(a, cmd::dar_item(a.op, p.objetivo, static_cast<u16>(o.col<O_X>()[so]), static_cast<u16>(o.col<O_Y>()[so]), p.id, n));
        }
        return;
    }
    if (accion == "recoger") {  // 0x4086f0: nothing typed = the whole stack; 0 or more than the stack = nothing
        u32 n = p.maximo;
        if (!texto.empty()) {
            long v = 0;
            std::from_chars(texto.data(), texto.data() + texto.size(), v);
            n = v > 0 ? static_cast<u32>(v) : 0;
        }
        if (n > 0 && n <= p.maximo) enviar(a, cmd::recoger(a.op, static_cast<u16>(p.x), static_cast<u16>(p.y), p.id, n));
        return;
    }
    if (accion == "join") {
        if (dijo_si(texto)) enviar(a, cmd::entrar_clan(a.op));
    } else if (accion == "leave") {
        if (dijo_si(texto)) enviar(a, cmd::dejar_clan(a.op));
    } else if (accion == "delbookmark") {  // C_DeleteBookmark by name (0x80), then the row goes away
        if (!dijo_si(texto)) return;
        auto it = std::find_if(a.marcadores.begin(), a.marcadores.end(), [&](const Marcador& m) { return m.id == p.id; });
        if (it == a.marcadores.end()) return;
        enviar(a, cmd::borrar_marcador(a.op, it->nombre));
        a.destino.sel = -1;
        a.marcadores.erase(it);
    } else if (texto.empty()) {
        return;
    } else if (accion == "bookmark") {  // 0x40d240: the typed name goes through the normal add-bookmark path
        marcar_lugar_aqui(a, texto);
    } else if (accion == "banish") {
        enviar(a, cmd::echar_del_grupo(a.op, texto));
    } else if (accion == "ban") {
        enviar(a, cmd::echar_del_clan(a.op, texto));
    } else if (accion == "addbuddy" || accion == "delbuddy") {
        enviar(a, cmd::amigo(a.op, accion == "addbuddy", texto));
    } else if (accion == "war") {
        enviar(a, cmd::guerra(a.op, cmd::GUERRA_DECLARAR, texto));
    } else if (accion == "surrender") {
        enviar(a, cmd::guerra(a.op, cmd::GUERRA_RENDIRSE, texto));
    } else if (accion == "cease") {
        enviar(a, cmd::guerra(a.op, cmd::GUERRA_TERMINAR, texto));
    } else if (accion == "create") {
        enviar(a, cmd::crear_clan(a.op, texto));
    } else if (accion == "alliancejoin") {
        enviar(a, cmd::alianza(a.op, 3, texto));
    } else if (accion == "allianceleave") {
        if (dijo_si(texto)) enviar(a, cmd::alianza(a.op, 4));
    } else if (accion == "emblem") {  // 0x40d070 -> 0x4070c0
        subir_emblema(a, texto);
    } else if (accion == "checkbmp") {  // 0x40d010 -> 0x406fc0
        ver_emblema(a, texto);
    } else if (accion == "title") {  // 0x406000: "<character> <title>"
        const size_t sp = texto.find(' ');
        if (sp == std::string_view::npos) return;
        std::string_view titulo = texto.substr(sp + 1);
        while (!titulo.empty() && titulo.front() == ' ') titulo.remove_prefix(1);
        if (!titulo.empty()) enviar(a, cmd::poner_titulo(a.op, texto.substr(0, sp), titulo));
    }
}

// Prompt text with its "%u" (or "%d") replaced by the stack size.
static std::string texto_con_maximo(const App& a, int id, u32 maximo) {
    std::string t = cadena(a, id);
    for (const char* f : {"%u", "%d"})
        if (const size_t k = t.find(f); k != std::string::npos) {
            t.replace(k, 2, std::to_string(maximo));
            break;
        }
    return t;
}

// Drop a stack (string 119) and give a stack (string 140): the chat line is the prompt, as in pick up.
void pedir_soltar(App& a, ObjId item, u32 cantidad) {
    if (a.chat_prompt.activo) return;
    abrir_prompt(a, texto_con_maximo(a, 119, cantidad), "soltar");
    a.chat_prompt.id = item, a.chat_prompt.maximo = cantidad;
}

void pedir_dar(App& a, ObjId objetivo, ObjId item, u32 cantidad) {
    if (a.chat_prompt.activo) return;
    abrir_prompt(a, texto_con_maximo(a, 140, cantidad), "dar");
    a.chat_prompt.id = item, a.chat_prompt.objetivo = objetivo, a.chat_prompt.maximo = cantidad;
}

// Pick up: a stack asks how many unless the "getall" option is on (0x408529 -> 0x408670, prompt string 0x79).
void pedir_recoger(App& a, i32 x, i32 y, ObjId id, u32 cantidad) {
    if (cantidad <= 1 || (a.cfg.opciones_orig >> OPC_GETALL & 1)) {
        enviar(a, cmd::recoger(a.op, static_cast<u16>(x), static_cast<u16>(y), id, std::max<u32>(1, cantidad)));
        return;
    }
    if (a.chat_prompt.activo) return;
    std::string t = cadena(a, 121);
    if (const size_t k = t.find("%d"); k != std::string::npos) t.replace(k, 2, std::to_string(cantidad));
    abrir_prompt(a, std::move(t), "recoger");
    a.chat_prompt.x = x, a.chat_prompt.y = y, a.chat_prompt.id = id, a.chat_prompt.maximo = cantidad;
}

// Invite: a click on a character within 6 x 3 cells (0x469903: |dx| <= 2 * 3, |dy| <= 3) sends C_CreateParty with the
// "random party" option as its type (0x4075b3) and writes string 0x3bb ("invited %0"); any click ends the mode.
void elegir_invitado(App& a) {
    a.invitando = false;
    const ObjId id = objeto_bajo_puntero(a);
    const TablaObjetos& o = a.mundo.objetos;
    const Slot s = id ? o.buscar(id) : SIN_SLOT, yo = o.buscar(a.mundo.jugador.id);
    if (s == SIN_SLOT || yo == SIN_SLOT) return;
    if (std::abs(o.col<O_X>()[s] - o.col<O_X>()[yo]) > 6 || std::abs(o.col<O_Y>()[s] - o.col<O_Y>()[yo]) > 3) return;
    std::string t = cadena(a, 955);
    if (const size_t k = t.find("%0"); k != std::string::npos) t.replace(k, 2, texto(a.mundo, o.col<O_NOMBRE>()[s]));
    aviso(a, t);
    enviar(a, cmd::invitar_grupo(a.op, id, a.cfg.opciones_orig >> OPC_RANDOMPARTY & 1));
}

// `<var src="#N">` of option11/12/13 (html var switch 0x438766..0x43a56e): a flag shows string 567 when set, 568 when
// clear (573 / 574 for accepttrade); the chat size and window transparency steps show 893..896 (client string table). The ids that only the
// newer Text.pak pages use (1032, 1037, 2026..2028) have no case in the US client: they stay empty.
std::string datos_opciones(const App& a) {
    auto bit = [&](OpcionOrig o) { return (a.cfg.opciones_orig >> o & 1) != 0; };
    auto msg = [&](int id) { return std::string(a.mensajes.linea(static_cast<size_t>(id))); };  // the client string table
    auto onoff = [&](bool v) { return msg(v ? 567 : 568); };
    std::string v[1031];
    v[1002] = onoff(bit(OPC_GETALL));
    v[1003] = onoff(bit(OPC_BUBBLE));
    v[1004] = onoff(bit(OPC_DOCHAT));
    v[1005] = onoff(bit(OPC_DOWHISPER));
    v[1007] = msg(893 + std::min<int>(a.chat_tam, 3));
    v[1014] = onoff(bit(OPC_TOOLTIP));
    v[1015] = onoff(bit(OPC_TELCON));
    v[1016] = onoff(bit(OPC_INFRACON));
    v[1019] = msg(bit(OPC_ACCEPTTRADE) ? 573 : 574);
    v[1020] = onoff(bit(OPC_DOTRADECHAT));
    v[1023] = onoff(a.cfg.barra_personaje);
    v[1025] = msg(893 + std::clamp(a.cfg.alpha_orig, 0, 3));
    v[1026] = onoff(bit(OPC_SHORTCUTSCROLL));
    v[1027] = onoff(bit(OPC_MARKETMSG));
    v[1028] = onoff(bit(OPC_RANDOMPARTY));
    v[1029] = onoff(bit(OPC_PARTYREJECT));
    v[1030] = onoff(!bit(OPC_COMBATMUSIC));  // the only inverted one (0x43a484: set -> $568)
    std::string datos;
    for (int i = 0; i <= 1030; ++i) {
        if (i) datos += SEPARADOR_DATOS;
        datos += v[i];
    }
    return datos;
}

// Option toggles of the original Options pages (dispatcher 0x40b1de..0x40c660). Each flips a byte of the client's option
// block and reopens the page. dochat / dotradechat / dowhisper also send `ccc` 0x19 (what, flag) with the new state
// (0x4069f0, 0x406af0, 0x406bf0 -> 0x406abb..0x406cc6); `volume` opens the volume window ([0x8f028c], volume_ui.cpp).
bool accion_opciones(App& a, std::string_view accion) {
    struct Fila { std::string_view nombre; OpcionOrig bit; };
    static constexpr Fila FILAS[] = {
        {"getall", OPC_GETALL}, {"bubbleon", OPC_BUBBLE}, {"dochat", OPC_DOCHAT}, {"dotradechat", OPC_DOTRADECHAT},
        {"dowhisper", OPC_DOWHISPER}, {"showMargetMsg", OPC_MARKETMSG}, {"randomParty", OPC_RANDOMPARTY},
        {"cPartyReject", OPC_PARTYREJECT}, {"combatMusic", OPC_COMBATMUSIC}, {"shortcutscroll", OPC_SHORTCUTSCROLL},
        {"tooltip", OPC_TOOLTIP}, {"accepttrade", OPC_ACCEPTTRADE}, {"showfloathpmp", OPC_FLOATHPMP},
    };
    bool hecho = false;
    for (const Fila& f : FILAS)
        if (accion == f.nombre) a.cfg.opciones_orig ^= 1 << f.bit, hecho = true;
    // Chat toggles tell the server: 0 world chat, 6 trade chat, 2 whisper.
    auto avisar = [&](std::string_view nombre, OpcionOrig bit, u8 que) {
        if (accion == nombre && a.op.tiene(Cli::LOGINTOSERVEROK))
            enviar(a, cmd::opcion_chat(a.op, que, a.cfg.opciones_orig >> bit & 1));
    };
    avisar("dochat", OPC_DOCHAT, 0);
    avisar("dotradechat", OPC_DOTRADECHAT, 6);
    avisar("dowhisper", OPC_DOWHISPER, 2);
    const int clase = a.pj_sel >= 0 && a.pj_sel < static_cast<int>(a.pjs.size()) ? a.pjs[static_cast<size_t>(a.pj_sel)].v[PJ_CLASE] : -1;
    const bool telcon = a.cfg.opciones_orig >> OPC_TELCON & 1;
    if (hecho) {
    } else if (accion == "showhpmp") {
        a.cfg.barra_personaje = !a.cfg.barra_personaje, hecho = true;
    } else if (accion == "alpha") {
        a.cfg.alpha_orig = (a.cfg.alpha_orig + 1) % 4, hecho = true;
    } else if (accion == "chatstate") {
        a.chat_tam = static_cast<u8>((a.chat_tam + 1) % 4), hecho = true;
    } else if (accion == "telcon") {  // elves of level 50+ toggle it; anyone else can only switch it off (0x40c566)
        if (clase == 2 && a.mundo.jugador.nivel >= 50) a.cfg.opciones_orig ^= 1 << OPC_TELCON;
        else if (telcon) a.cfg.opciones_orig &= ~(1 << OPC_TELCON);
        else return true;
        hecho = true;
    } else if (accion == "infracon") {  // elves and dark elves toggle it; the others can only clear it when telcon is on (0x40c5dc)
        if (clase == 2 || clase == 4) a.cfg.opciones_orig ^= 1 << OPC_INFRACON;
        else if (telcon) a.cfg.opciones_orig &= ~(1 << OPC_INFRACON);
        else return true;
        hecho = true;
    } else if (accion == "volume") {  // 0x40b19d: shows the volume window
        a.ventana_volumen = true;
        return true;
    }
    if (!hecho) return false;
    guardar_config(a.cfg);
    abrir_pagina_local(a, a.npc_pagina);  // every toggle clears and reopens its own page (0x43c510 + vtable[0x54])
    return true;
}

void accion_comunidad(App& a, std::string_view accion) {
    if (accion_opciones(a, accion)) return;
    const bool principe = clase_personaje(a) == 0;
    if (accion == "party") enviar(a, cmd::ver_grupo(a.op));
    else if (accion == "outparty") enviar(a, cmd::dejar_grupo(a.op));
    else if (accion == "join") {  // a prince confirms first (string 0x313); the others send it at once (0x40b950)
        if (principe) abrir_prompt(a, cadena(a, 787), "join");
        else enviar(a, cmd::entrar_clan(a.op));
    } else if (accion == "leave") abrir_prompt(a, cadena(a, 484), "leave");  // string 0x1e4, then y / n (0x405fb0)
    else if (accion == "pledge") enviar(a, cmd::ver_clan(a.op));
    else if (accion == "buddy" || accion == "refreshbuddy") enviar(a, cmd::amigos(a.op));
    else if (accion == "invite") {  // click-on-target mode: message 0xb3, then the click (elegir_invitado)
        aviso(a, cadena(a, 179));
        a.invitando = true;
    } else if (accion == "banish") abrir_prompt(a, cadena(a, 409), accion);
    else if (accion == "ban") abrir_prompt(a, cadena(a, 107), accion);
    else if (accion == "addbuddy") abrir_prompt(a, cadena(a, 588), accion);
    else if (accion == "delbuddy") abrir_prompt(a, cadena(a, 589), accion);
    else if (accion == "title") abrir_prompt(a, cadena(a, 586), accion);
    else if (accion == "war" || accion == "surrender" || accion == "cease") {  // princes only (0x40bb46); war also needs the clan state
        if (!principe) return;
        abrir_prompt(a, cadena(a, accion == "war" ? 577 : accion == "surrender" ? 578 : 579), accion);
    }
    else if (accion == "create") abrir_prompt(a, cadena(a, 83), accion);  // bpledge2: "What will be the name of your Blood Pledge?"
    else if (accion == "emblem") abrir_prompt(a, cadena(a, 576), accion);    // 0x40d030
    else if (accion == "checkbmp") abrir_prompt(a, cadena(a, 575), accion);  // 0x40cfd0
    else if (accion == "alliancelist") enviar(a, cmd::alianza(a.op, 2));
    else if (accion == "alliancejoin") {  // s3ep1 sends only `cc` 0x4e 3: the server takes the lord you face
        if (a.op.perfil.bytes_numero_pregunta) abrir_prompt(a, cadena(a, 580), accion);
        else enviar(a, cmd::alianza(a.op, 3));
    } else if (accion == "allianceleave") {
        if (a.op.perfil.bytes_numero_pregunta) enviar(a, cmd::alianza(a.op, 4));
        else abrir_prompt(a, cadena(a, 1210), accion);  // asks first, y -> 0x4e 4 (0x401d86)
    }
    // pledgefocus*: not wired (audit U-D7)
}

void responder_pregunta(App& a, bool si) {
    if (a.preguntas.empty()) return;
    const Pregunta& p = a.preguntas.front();
    if (p.tipo == PREGUNTA_NOMBRE_CASA) {  // se responde con el texto (o no se responde: el servidor no lo espera)
        if (si && !p.respuesta.empty()) enviar(a, cmd::responder_texto(a.op, p.numero, p.tipo, p.respuesta));
    } else {
        enviar(a, cmd::responder(a.op, p.numero, p.tipo, si));
    }
    a.preguntas.erase(a.preguntas.begin());
}

namespace {

// Recuadro con título para las ventanas de este módulo; true al cerrar.
bool marco(App& a, Rect r, std::string_view titulo, bool con_cierre = true) {
    Graficos& g = *a.g;
    const u8 op = opacidad_ventanas(a);
    g.gpu().rectangulo(r.x, r.y, r.w, r.h, clave_orden(CAPA_UI, 10), rgba(10, 8, 6, op));
    g.gpu().rectangulo(r.x + 1, r.y + 1, r.w - 2, ui::ALTO_TITULO - 2, clave_orden(CAPA_UI, 11),
                       rgba(40, 32, 22, static_cast<u8>(std::min(255, op + 60))));
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    g.texto(titulo, static_cast<float>(r.x + 8), static_cast<float>(r.y + 6), kt, rgba(214, 190, 140));
    if (!con_cierre) return false;
    g.texto("[x]", static_cast<float>(r.x + r.w - 26), static_cast<float>(r.y + 6), kt, rgba(255, 255, 254));
    return a.entrada.clic && ui::sobre(a.entrada, r.x + r.w - 28, r.y + 2, 26, 20);
}

// Grilla de ítems ofrecidos en el intercambio; devuelve el índice bajo el puntero o -1.
int grilla(App& a, const std::vector<ItemIntercambio>& items, int x0, int y0, bool original = false, int first = 0) {
    Graficos& g = *a.g;
    int bajo = -1;
    const int cols = original ? TRADE_COLS : COLUMNAS, cell = original ? TRADE_CELL : CELDA;
    for (int c = 0; c < cols * (original ? TRADE_ROWS : FILAS); ++c) {
        const int x = x0 + (c % cols) * cell, y = y0 + (c / cols) * cell;
        const bool encima = ui::sobre(a.entrada, x, y, cell - 2, cell - 2);
        if (original) {  // the frame image has the cell lines
            if (encima) g.gpu().rectangulo(x, y, cell - 1, cell - 1, clave_orden(CAPA_UI, 14), rgba(120, 100, 40, 110));
        } else {
            g.gpu().rectangulo(x, y, cell - 2, cell - 2, clave_orden(CAPA_UI, 12), encima ? rgba(60, 50, 35, 230) : rgba(28, 24, 20, 230));
        }
        const int idx = first + c;  // `first` = items scrolled past (whole rows)
        if (idx >= static_cast<int>(items.size())) continue;
        draw_item_icon(g.gpu(), g.icono(items[idx].gfx), x + 1, y + 1, clave_orden(CAPA_UI, 13), items[idx].bendicion);
        if (encima) bajo = idx;
    }
    return bajo;
}

}  // namespace

void eventos_social(App& a, const Evento& e) {
    switch (e.tipo) {
    case Ev::GRUPO:
        if (e.v[GR_TIPO] == GR_VACIAR) {
            a.grupo.clear();
        } else if (e.v[GR_TIPO] == GR_MIEMBRO) {
            auto it = std::find_if(a.grupo.begin(), a.grupo.end(), [&](const MiembroGrupo& m) { return m.id == e.id; });
            if (it == a.grupo.end()) it = a.grupo.insert(a.grupo.end(), MiembroGrupo{e.id, std::string(e.s[0])});
            if (e.v[GR_HP] != 0xFF) it->hp = static_cast<u8>(e.v[GR_HP]);
            if (e.v[GR_LIDER]) it->lider = true;
        } else {
            for (MiembroGrupo& m : a.grupo) m.lider = m.id == e.id;
        }
        break;
    case Ev::HP_OBJETO:  // S_HpMeter: el servidor se la manda a los del grupo, aunque estén lejos
        for (MiembroGrupo& m : a.grupo)
            if (m.id == e.id) m.hp = static_cast<u8>(e.v[0]);
        break;
    case Ev::MENSAJE_SERVIDOR:
        // 418 el grupo se disolvió, 419 lo echaron, 420 "%0 dejó el grupo" (también a quien se va).
        if (e.v[MS_ID] == 418 || e.v[MS_ID] == 419 || (e.v[MS_ID] == 420 && e.s[0] == nombre_personaje(a))) {
            a.grupo.clear();
        } else if (e.v[MS_ID] == 420) {
            std::erase_if(a.grupo, [&](const MiembroGrupo& m) { return m.nombre == e.s[0]; });
        }
        break;
    case Ev::DUELO: {  // S_PacketBox 5: message 645 "%s accepted the duel..." with the opponent's name
        const TablaObjetos& o = a.mundo.objetos;
        const Slot s = o.buscar(e.id);
        if (s == SIN_SLOT) break;
        Evento m{};
        m.tipo = Ev::MENSAJE_SERVIDOR;
        m.v[MS_ID] = 645;
        m.v[MS_NARGS] = 1;
        m.s[0] = texto(a.mundo, o.col<O_NOMBRE>()[s]);
        char buf[256];
        u32 color = 0;
        const size_t n = formatear_mensaje(a.mensajes, a.desc, m, buf, sizeof buf, color);
        a.chat.agregar({buf, n}, color, CHAT_MENSAJE);
        break;
    }
    case Ev::BLOQUEADO: {
        const std::string nombre(e.s[0]);
        std::erase(a.bloqueados, nombre);
        if (e.v[0]) a.bloqueados.push_back(nombre);
        break;
    }
    case Ev::PREGUNTA: {
        Pregunta p;
        p.numero = static_cast<u32>(e.v[PR_NUMERO]);
        p.tipo = static_cast<u16>(e.v[PR_TIPO]);
        // El texto, ya armado: string-e.tbl con los argumentos (como S_ServerMessage).
        Evento m = e;
        m.v[MS_ID] = e.v[PR_TIPO];
        m.v[MS_NARGS] = !e.s[0].empty() + !e.s[1].empty();
        char buf[256];
        u32 color = 0;
        size_t n = formatear_mensaje(a.mensajes, a.desc, m, buf, sizeof buf, color);
        p.texto.assign(buf, n);
        if (p.tipo == 951 && (a.cfg.opciones_orig >> OPC_PARTYREJECT & 1)) {  // 0x4887e6: refuse invitations by itself
            std::string t = cadena(a, 1005);
            if (const size_t k = t.find("%0"); k != std::string::npos) t.replace(k, 2, e.s[0]);
            aviso(a, t);
            enviar(a, cmd::responder(a.op, p.numero, p.tipo, false));
            break;
        }
        if (p.tipo == 252 && !(a.cfg.opciones_orig >> OPC_ACCEPTTRADE & 1)) {  // 0x48876d: refuses trade requests by itself
            enviar(a, cmd::responder(a.op, p.numero, p.tipo, false));
            break;
        }
        a.preguntas.push_back(std::move(p));
        break;
    }
    case Ev::INTERCAMBIO_ABRIR:
        a.intercambio.abierto = true;
        a.intercambio.con = e.s[0];
        a.intercambio.propio.clear();
        a.intercambio.otro.clear();
        a.intercambio.ok = false;
        a.ventana_inventario = true;  // de ahí se arrastran los ítems
        break;
    case Ev::INTERCAMBIO_ITEM:
        (e.v[IN_LADO] == 0 ? a.intercambio.propio : a.intercambio.otro)
            .push_back({static_cast<u16>(e.v[IN_GFX]), static_cast<u8>(e.v[IN_BENDICION]), std::string(e.s[0])});
        break;
    case Ev::EMBLEMA: a.g->poner_emblema(e.id, {reinterpret_cast<const u8*>(e.s[0].data()), e.s[0].size()}, static_cast<u32>(a.entrada.ahora_ms)); break;
    case Ev::INTERCAMBIO_FIN:
        aviso(a, e.v[IN_CANCELADO] ? "The trade was cancelled." : "The trade is complete.");
        a.intercambio = {};
        break;
    default: break;
    }
}

void ofrecer_en_intercambio(App& a, ObjId item) {
    if (!a.intercambio.abierto || a.intercambio.ok) return;
    Slot s = a.mundo.inventario.items.buscar(item);
    if (s == SIN_SLOT) return;
    const u32 n = a.mundo.inventario.items.col<I_CANTIDAD>()[s];
    // Original (drag release 0x448230 -> 0x4477b0): a stack of more than one asks how many in the chat line, with the give
    // prompt (string 140); the callback 0x4472b0 takes empty = all, 0 or more than the stack = nothing.
    if (!a.cfg.ui_simple && n > 1) {
        if (a.chat_prompt.activo) return;
        abrir_prompt(a, texto_con_maximo(a, 140, n), "ofrecer");
        a.chat_prompt.id = item, a.chat_prompt.maximo = n;
        return;
    }
    enviar(a, cmd::ofrecer_item(a.op, item, n));
}

bool comando_social(App& a, std::string_view linea) {
    if (linea.empty() || linea[0] != '/') return false;
    std::string_view resto = linea.substr(1);
    std::string_view orden = resto.substr(0, resto.find(' '));
    std::string_view arg = orden.size() < resto.size() ? resto.substr(orden.size() + 1) : std::string_view{};
    while (!arg.empty() && arg.front() == ' ') arg.remove_prefix(1);
    if (comando_marcador(a, orden, arg)) return true;
    if (comando_tienda(a, orden, arg)) return true;
    if (comando_correo(a, orden)) return true;
    if (comando_castillos(a, orden, arg)) return true;
    if (igual_sin_mayusculas(orden, "pk")) {  // modo PK (del cliente): atacar jugadores con un clic
        a.modo_pk = !a.modo_pk;
        aviso(a, a.modo_pk ? "PK mode ON: clicking another player attacks them. /pk to turn it off."
                           : "PK mode OFF: Ctrl+click to attack another player.");
        return true;
    }
    if (igual_sin_mayusculas(orden, "exclude") || igual_sin_mayusculas(orden, "block")) {
        // Bloquear (o desbloquear, si ya estaba) a un personaje; sin nombre, la lista.
        if (!arg.empty()) {
            enviar(a, cmd::bloquear(a.op, arg));
        } else {
            std::string lista = a.bloqueados.empty() ? "nobody" : "";
            for (const std::string& n : a.bloqueados) lista += (lista.empty() ? "" : ", ") + n;
            aviso(a, ("Blocked: " + lista + ". /exclude <name> blocks or unblocks.").c_str());
        }
        return true;
    }
    if (igual_sin_mayusculas(orden, "buddy")) {
        enviar(a, cmd::amigos(a.op));  // la página "buddy" con la lista y quién está conectado
        return true;
    }
    if (igual_sin_mayusculas(orden, "addbuddy") || igual_sin_mayusculas(orden, "delbuddy")) {
        if (arg.empty()) aviso(a, "/addbuddy <name>, /delbuddy <name>; /buddy shows the list.");
        else enviar(a, cmd::amigo(a.op, igual_sin_mayusculas(orden, "addbuddy"), arg));
        return true;
    }
    if (igual_sin_mayusculas(orden, "chatparty")) {
        enviar(a, cmd::grupo_chat(a.op, 2));
        return true;
    }
    if (igual_sin_mayusculas(orden, "chatoutparty")) {
        enviar(a, cmd::grupo_chat(a.op, 1));
        return true;
    }
    if (igual_sin_mayusculas(orden, "chatbanish")) {
        enviar(a, cmd::grupo_chat(a.op, 0, arg));
        return true;
    }
    if (igual_sin_mayusculas(orden, "invite")) {
        ObjId id = buscar_por_nombre(a, arg);
        if (id) enviar(a, cmd::invitar_grupo(a.op, id));
        else aviso(a, "/invite: that character is not in sight.");
    } else if (igual_sin_mayusculas(orden, "party")) {
        enviar(a, cmd::ver_grupo(a.op));
    } else if (igual_sin_mayusculas(orden, "leave") || igual_sin_mayusculas(orden, "outparty")) {
        enviar(a, cmd::dejar_grupo(a.op));
    } else if (igual_sin_mayusculas(orden, "kick") || igual_sin_mayusculas(orden, "banish")) {
        if (arg.empty()) accion_comunidad(a, "banish");  // no name: the original asks for it (string 409)
        else enviar(a, cmd::echar_del_grupo(a.op, arg));
    } else if (igual_sin_mayusculas(orden, "trade")) {
        enviar(a, cmd::pedir_intercambio(a.op));
    } else if (igual_sin_mayusculas(orden, "pledge")) {
        enviar(a, cmd::ver_clan(a.op));
    } else if (igual_sin_mayusculas(orden, "join")) {
        enviar(a, cmd::entrar_clan(a.op));
    } else if (igual_sin_mayusculas(orden, "leaveclan") || igual_sin_mayusculas(orden, "pledgeleave") ||
               igual_sin_mayusculas(orden, "leavepledge")) {
        enviar(a, cmd::dejar_clan(a.op));
    } else if (igual_sin_mayusculas(orden, "clan")) {
        if (arg.empty()) aviso(a, "/clan <name>: create a blood pledge (princes; 30000 adena).");
        else enviar(a, cmd::crear_clan(a.op, arg));
    } else if (igual_sin_mayusculas(orden, "banclan") || igual_sin_mayusculas(orden, "ban")) {
        if (arg.empty()) accion_comunidad(a, "ban");  // no name: the original asks for it (string 107)
        else enviar(a, cmd::echar_del_clan(a.op, arg));
    } else if (igual_sin_mayusculas(orden, "rank")) {  // /rank <rank 1..3> <name> (C_Rank sub 1)
        const size_t sp = arg.find(' ');
        int rango = 0;
        std::string_view quien = sp == std::string_view::npos ? std::string_view{} : arg.substr(sp + 1);
        while (!quien.empty() && quien.front() == ' ') quien.remove_prefix(1);
        const auto r = std::from_chars(arg.data(), arg.data() + std::min(sp, arg.size()), rango);
        if (sp == std::string_view::npos || r.ec != std::errc{} || rango < 1 || rango > 3 || quien.empty())
            aviso(a, "/rank <1-3> <name>: sets a pledge member's rank (princes).");
        else enviar(a, cmd::rango_clan(a.op, static_cast<u8>(rango), quien));
    } else if (igual_sin_mayusculas(orden, "propose")) {
        enviar(a, cmd::proponer(a.op, false));
    } else if (igual_sin_mayusculas(orden, "divorce")) {
        enviar(a, cmd::proponer(a.op, true));
    } else if (igual_sin_mayusculas(orden, "duel")) {
        enviar(a, cmd::duelo(a.op));
    } else if (igual_sin_mayusculas(orden, "checkpk")) {
        enviar(a, cmd::ver_pk(a.op));
    } else if (igual_sin_mayusculas(orden, "alliancelist") || igual_sin_mayusculas(orden, "alliancejoin") ||
               igual_sin_mayusculas(orden, "allianceleave")) {
        accion_comunidad(a, igual_sin_mayusculas(orden, "alliancelist") ? "alliancelist"
                            : igual_sin_mayusculas(orden, "alliancejoin") ? "alliancejoin" : "allianceleave");
    } else if (igual_sin_mayusculas(orden, "chatinvite")) {
        if (arg.empty()) aviso(a, "/chatinvite <name>: invites a character to your chat party.");
        else enviar(a, cmd::invitar_chat(a.op, arg));
    } else if (igual_sin_mayusculas(orden, "loc")) {  // 0x405e90
        char t[48];
        const Slot s = a.mundo.objetos.buscar(a.mundo.jugador.id);
        std::snprintf(t, sizeof t, "location (%d, %d)", s != SIN_SLOT ? static_cast<int>(a.mundo.objetos.col<O_X>()[s]) : 0,
                      s != SIN_SLOT ? static_cast<int>(a.mundo.objetos.col<O_Y>()[s]) : 0);
        aviso(a, t);
    } else if (igual_sin_mayusculas(orden, "telcon")) {  // elves of level 50+ only; otherwise the flag is forced off (0x40a100)
        a.telcon = clase_personaje(a) == 2 && a.mundo.jugador.nivel >= 50 && !a.telcon;
    } else if (igual_sin_mayusculas(orden, "title")) {
        // /title <título> (el propio) o /title <personaje> <título> (príncipe a su clan).
        std::string_view primero = arg.substr(0, arg.find(' '));
        bool otro = primero.size() < arg.size() && buscar_por_nombre(a, primero);
        std::string_view quien = otro ? primero : nombre_personaje(a);
        std::string_view titulo = otro ? arg.substr(primero.size() + 1) : arg;
        enviar(a, cmd::poner_titulo(a.op, quien, titulo));
    } else if (igual_sin_mayusculas(orden, "emblem")) {
        if (arg.empty()) accion_comunidad(a, "emblem");
        else subir_emblema(a, arg);
    } else if (igual_sin_mayusculas(orden, "checkbmp")) {
        if (arg.empty()) accion_comunidad(a, "checkbmp");
        else ver_emblema(a, arg);
    } else if (igual_sin_mayusculas(orden, "who")) {
        enviar(a, cmd::quien(a.op, arg));
    } else {
        return false;  // no es un comando del cliente: se manda como chat
    }
    return true;
}

// Splits `t` into at most two lines that fit `max_w` pixels (word wrap); returns the byte lengths in `n`.
static void partir_pregunta(std::string_view t, int max_w, char (&lineas)[2][128], size_t (&n)[2]) {
    n[0] = n[1] = 0;
    int linea = 0;
    for (size_t i = 0; i < t.size() && linea < 2;) {
        const size_t fin = t.find(' ', i);
        const std::string_view palabra = t.substr(i, (fin == std::string_view::npos ? t.size() : fin) - i);
        if (n[linea] && ancho_texto(lineas[linea], n[linea]) + ancho_texto(palabra.data(), palabra.size()) + 4 > max_w) ++linea;
        if (linea >= 2) break;
        if (n[linea] && n[linea] < 126) lineas[linea][n[linea]++] = ' ';
        const size_t m = std::min(palabra.size(), sizeof lineas[0] - 1 - n[linea]);
        std::memcpy(lineas[linea] + n[linea], palabra.data(), m);
        n[linea] += m;
        i = fin == std::string_view::npos ? t.size() : fin + 1;
    }
}

static void dibujar_pregunta_original(App& a) {
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    constexpr int BAR_X = 137, BAR_Y = 437, YES_DX = 224, NO_DX = 264, TEXT_PAD = 8, TEXT_W = 210;
    const int bx = (a.gpu.ancho() - 640) / 2 + BAR_X, by = a.gpu.alto() - 480 + BAR_Y;
    const u64 kb = clave_orden(CAPA_UI, 20), kt = clave_orden(CAPA_UI_TEXTO, 20);
    g.gpu().dibujar(g.img(701), bx, by, kb);
    char lineas[2][128];
    size_t n[2];
    partir_pregunta(a.preguntas.front().texto, TEXT_W, lineas, n);
    for (int k = 0; k < 2; ++k) g.texto_contorno({lineas[k], n[k]}, static_cast<float>(bx + TEXT_PAD), static_cast<float>(by + 4 + 12 * k), kt, BLANCO);
    bool si = e.enter, no = e.escape;
    const int ty = by + (32 - ALTO_GLIFO) / 2;
    auto boton = [&](std::string_view t, int dx) {
        const int w = ancho_texto(t.data(), t.size());
        const bool encima = ui::sobre(e, bx + dx - 2, ty - 2, w + 4, ALTO_GLIFO + 4);
        g.texto_contorno(t, static_cast<float>(bx + dx), static_cast<float>(ty), kt, encima ? rgba(255, 230, 120) : BLANCO);
        if (encima && e.clic) a.clic_consumido = true;
        return encima && e.clic;
    };
    si = boton("YES", YES_DX) || si;
    no = boton("NO", NO_DX) || no;
    if (si || no) {
        responder_pregunta(a, si && !no);
        e.enter = e.escape = false;  // no van al chat
    }
}

void dibujar_pregunta(App& a) {
    if (a.preguntas.empty()) return;
    Graficos& g = *a.g;
    EntradaFrame& e = a.entrada;
    // Original UI (0x41fe30): a parchment bar (image 701) at x 137 of the 640x480 block with the question on up to two
    // lines (12 px pitch from y + 4) and YES / NO at x + 224 / x + 264, vertically centred. A house-name prompt still
    // needs its text box, so it keeps the dialog below.
    if (!a.cfg.ui_simple && a.preguntas.front().tipo != PREGUNTA_NOMBRE_CASA) return dibujar_pregunta_original(a);
    const Rect r{(a.gpu.ancho() - ANCHO_PREGUNTA) / 2, a.gpu.alto() / 4, ANCHO_PREGUNTA, ALTO_PREGUNTA};
    marco(a, r, "Question", false);
    // El mensaje en hasta 2 líneas.
    const std::string& t = a.preguntas.front().texto;
    char lineas[2][128];
    size_t n[2] = {0, 0};
    int linea = 0;
    for (size_t i = 0; i < t.size() && linea < 2;) {
        size_t fin = t.find(' ', i);
        std::string_view palabra(t.data() + i, (fin == std::string::npos ? t.size() : fin) - i);
        if (n[linea] && ancho_texto(lineas[linea], n[linea]) + ancho_texto(palabra.data(), palabra.size()) + 4 > r.w - 20) ++linea;
        if (linea >= 2) break;
        if (n[linea] && n[linea] < 126) lineas[linea][n[linea]++] = ' ';
        size_t m = std::min(palabra.size(), sizeof lineas[0] - 1 - n[linea]);
        std::memcpy(lineas[linea] + n[linea], palabra.data(), m);
        n[linea] += m;
        i = fin == std::string::npos ? t.size() : fin + 1;
    }
    Pregunta& p = a.preguntas.front();
    const bool con_texto = p.tipo == PREGUNTA_NOMBRE_CASA;
    for (int k = 0; k < (con_texto ? 1 : 2); ++k)
        g.texto({lineas[k], n[k]}, static_cast<float>(r.x + 10), static_cast<float>(r.y + 28 + k * 15),
                clave_orden(CAPA_UI_TEXTO, 10), rgba(255, 255, 254));
    if (con_texto) {  // el nombre: hasta 16 caracteres (C_Attr 512)
        g.gpu().rectangulo(r.x + 10, r.y + 42, r.w - 20, 18, clave_orden(CAPA_UI, 11), rgba(40, 32, 25, 255));
        ui::caja_texto(g, e, p.respuesta, r.x + 13, r.y + 45, r.w - 26, true, false, 16);
        e.ntexto = 0;
    }
    bool si = ui::boton_texto(g, e, con_texto ? "OK" : "Yes", r.x + r.w / 2 - 90, r.y + r.h - 26, 80, 18) || e.enter;
    bool no = ui::boton_texto(g, e, con_texto ? "Cancel" : "No", r.x + r.w / 2 + 10, r.y + r.h - 26, 80, 18) || e.escape;
    if (si || no) {
        responder_pregunta(a, si);
        e.enter = e.escape = false;  // no van al chat
    }
}

void dibujar_intercambio(App& a) {
    if (!a.intercambio.abierto) return;
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    arrastrar_ventana(a, IdVentana::INTERCAMBIO);
    const Rect r = rect_ventana(a, IdVentana::INTERCAMBIO);
    char titulo[96];
    if (!a.cfg.ui_simple) {
        if (draw_original_frame(a, r.x, r.y, 1199, 7, 10, 1201, 174, 18)) enviar(a, cmd::cancelar_intercambio(a.op));
        draw_original_overlay(a, r.x + 2, r.y + 12, 1200);
        // Scroll bars at x 168 (0x42c327, 0x42c3bd): up at y 46 / 175, track 1212 from y + 8, down at y + 93; the grids
        // show 3 rows of 4 and scroll by whole rows.
        Intercambio& tr = a.intercambio;
        const bool sobre_tr = sobre_ventana(a, IdVentana::INTERCAMBIO);
        const std::vector<ItemIntercambio>* listas[2] = {&tr.propio, &tr.otro};
        const int sy[2] = {TRADE_MINE_Y - 1, TRADE_OTHER_Y};
        for (int k = 0; k < 2; ++k)
            original_scrollbar(a, tr.barra[k], r.x + TRADE_SCROLL_X, r.y + sy[k], TRADE_SCROLL_H,
                               static_cast<u32>((listas[k]->size() + TRADE_COLS - 1) / TRADE_COLS), TRADE_ROWS, tr.desplaza[k],
                               sobre_tr && ui::sobre(a.entrada, r.x + TRADE_X, r.y + sy[k], TRADE_SCROLL_X + 12 - TRADE_X, TRADE_SCROLL_H),
                               1212, -2);
        const int bajo_propio = grilla(a, tr.propio, r.x + TRADE_X + 1, r.y + TRADE_MINE_Y + 1, true, static_cast<int>(tr.desplaza[0]) * TRADE_COLS);
        const int bajo_otro = grilla(a, tr.otro, r.x + TRADE_X + 1, r.y + TRADE_OTHER_Y + 1, true, static_cast<int>(tr.desplaza[1]) * TRADE_COLS);
        const ItemIntercambio* it = bajo_propio >= 0 ? &a.intercambio.propio[bajo_propio]
                                    : bajo_otro >= 0 ? &a.intercambio.otro[bajo_otro] : nullptr;
        if (it) {
            char buf[256];
            const size_t m = expandir_desc(a.desc, it->nombre, buf, sizeof buf);
            const int w = ancho_texto(buf, m);
            g.gpu().rectangulo(e.mx + 10, e.my + 14, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
            g.texto({buf, m}, static_cast<float>(e.mx + 14), static_cast<float>(e.my + 17), clave_orden(CAPA_UI_TEXTO, 30));
        }
        // Name plates printed on the frame: own name at (43,31), the partner's at (43,159), white (0x42d4e4).
        g.texto_sombra(nombre_personaje(a), static_cast<float>(r.x + 43), static_cast<float>(r.y + 31), clave_orden(CAPA_UI_TEXTO, 14),
                       rgba(255, 255, 254));
        g.texto_sombra(tr.con, static_cast<float>(r.x + 43), static_cast<float>(r.y + 159), clave_orden(CAPA_UI_TEXTO, 14),
                       rgba(255, 255, 254));
        if (a.intercambio.ok) {
            g.texto_sombra("Waiting...", static_cast<float>(r.x + 53), static_cast<float>(r.y + 284), clave_orden(CAPA_UI_TEXTO, 14),
                           rgba(170, 230, 255));
        } else if (draw_original_button(a, r.x + 53, r.y + 282, 1202, 1203)) {
            enviar(a, cmd::aceptar_intercambio(a.op));
            a.intercambio.ok = true;
        }
        if (draw_original_button(a, r.x + 100, r.y + 282, 1204, 1205)) enviar(a, cmd::cancelar_intercambio(a.op));
        return;
    }
    std::snprintf(titulo, sizeof titulo, "Trade with %s", a.intercambio.con.c_str());
    if (marco(a, r, titulo)) enviar(a, cmd::cancelar_intercambio(a.op));
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 12);
    const int x0 = r.x + (r.w - COLUMNAS * CELDA) / 2;
    g.texto("You offer (drag items here)", static_cast<float>(x0), static_cast<float>(r.y + 28), kt, rgba(214, 190, 140));
    int bajo_propio = grilla(a, a.intercambio.propio, x0, r.y + 42);
    std::snprintf(titulo, sizeof titulo, "%s offers", a.intercambio.con.c_str());
    g.texto(titulo, static_cast<float>(x0), static_cast<float>(r.y + 42 + FILAS * CELDA + 6), kt, rgba(214, 190, 140));
    int bajo_otro = grilla(a, a.intercambio.otro, x0, r.y + 56 + FILAS * CELDA + 6);
    // Nombre del ítem bajo el puntero.
    const ItemIntercambio* it = bajo_propio >= 0 ? &a.intercambio.propio[bajo_propio]
                                : bajo_otro >= 0 ? &a.intercambio.otro[bajo_otro] : nullptr;
    if (it) {
        char buf[256];
        size_t m = expandir_desc(a.desc, it->nombre, buf, sizeof buf);
        int w = ancho_texto(buf, m);
        g.gpu().rectangulo(e.mx + 10, e.my + 14, w + 8, ALTO_GLIFO + 6, clave_orden(CAPA_UI, 30), rgba(0, 0, 0, 220));
        g.texto({buf, m}, static_cast<float>(e.mx + 14), static_cast<float>(e.my + 17), clave_orden(CAPA_UI_TEXTO, 30));
    }
    const int yb = r.y + r.h - 26;
    if (a.intercambio.ok) {
        g.texto("Waiting for the other side...", static_cast<float>(r.x + 12), static_cast<float>(yb + 3), kt, rgba(170, 230, 255));
    } else if (ui::boton_texto(g, e, "OK", r.x + 12, yb, 90, 18)) {
        enviar(a, cmd::aceptar_intercambio(a.op));
        a.intercambio.ok = true;
    }
    if (ui::boton_texto(g, e, "Cancel", r.x + r.w - 102, yb, 90, 18)) enviar(a, cmd::cancelar_intercambio(a.op));
}

// Grupo: la lista de miembros (sin el propio personaje) a la izquierda, a media altura, con su vida;
// el líder con una corona ("*").
void dibujar_grupo(App& a) {
    const std::string_view yo = nombre_personaje(a);
    // Al líder solo le llegan los que se unen (105); a los demás, la lista con él (104).
    if (std::none_of(a.grupo.begin(), a.grupo.end(), [&](const MiembroGrupo& m) { return m.nombre != yo; })) return;
    Graficos& g = *a.g;
    const u64 kf = clave_orden(CAPA_UI, 4), kt = clave_orden(CAPA_UI_TEXTO, 4);
    constexpr int ANCHO = 122, FILA = ALTO_GLIFO + 8;
    int y = a.gpu.alto() / 3;
    g.texto_sombra("Party", 6.0f, static_cast<float>(y - ALTO_GLIFO - 4), kt, ui::COLOR_TITULO);
    for (const MiembroGrupo& m : a.grupo) {
        if (m.nombre == yo) continue;
        g.gpu().rectangulo(4, y, ANCHO, FILA - 2, kf, rgba(20, 18, 15, 190));
        if (m.hp <= 100) g.gpu().rectangulo(4, y + FILA - 4, ANCHO * m.hp / 100, 2, kf + (1ull << 24), rgba(220, 50, 40));
        char t[64];
        std::snprintf(t, sizeof t, "%s%s", m.lider ? "* " : "", m.nombre.c_str());
        g.texto(t, 9.0f, static_cast<float>(y + 3), kt, m.lider ? rgba(255, 220, 120) : ui::COLOR_TEXTO);
        y += FILA;
    }
}

void dibujar_social(App& a) {
    if (!a.ventana_social) return;
    Graficos& g = *a.g;
    const EntradaFrame& e = a.entrada;
    arrastrar_ventana(a, IdVentana::SOCIAL);
    const Rect r = rect_ventana(a, IdVentana::SOCIAL);
    if (marco(a, r, "Social") || e.escape) {
        a.ventana_social = false;
        return;
    }
    struct Boton {
        std::string_view texto;
        std::string_view comando;
    } botones[] = {
        {"Party members", "/party"},       {"Leave party", "/leave"},
        {"Trade (face to face)", "/trade"}, {"Clan members", "/pledge"},
        {"Join clan (face prince)", "/join"}, {"Leave clan", "/leaveclan"},
    };
    int y = r.y + ui::ALTO_TITULO + 6;
    for (const Boton& b : botones) {
        if (ui::boton_texto(g, e, b.texto, r.x + 10, y, r.w - 20, 18)) comando_social(a, b.comando);
        y += 22;
    }
    const u64 kt = clave_orden(CAPA_UI_TEXTO, 10);
    const char* ayuda[] = {"/invite <name>  /kick <name>", "/clan <name>  /title <text>", "/who <name>"};
    for (const char* t : ayuda) {
        g.texto(t, static_cast<float>(r.x + 10), static_cast<float>(y + 4), kt, rgba(170, 160, 140));
        y += 15;
    }
}

Rect rect_social(const App& a) { return {(a.gpu.ancho() - ANCHO_SOCIAL) / 2 - 140, ui::VENTANA_Y + 60, ANCHO_SOCIAL, ALTO_SOCIAL}; }
Rect rect_intercambio(const App& a) {
    if (!a.cfg.ui_simple) return {(a.gpu.ancho() - TRADE_W) / 2, ui::VENTANA_Y + 20, TRADE_W, TRADE_H};
    return {(a.gpu.ancho() - ANCHO_INTERCAMBIO) / 2, ui::VENTANA_Y + 20, ANCHO_INTERCAMBIO, ALTO_INTERCAMBIO};
}

// Emblem cache file (the original's Emblem.dat / Emblem<server>.dat) in the settings folder, one per server port.
void emblemas_abrir(App& a) {
    namespace fs = std::filesystem;
    const u16 puerto = servidor_elegido(a.cfg).puerto;
    const std::string nombre = puerto == 2000 ? "Emblem.dat" : "Emblem" + std::to_string(puerto) + ".dat";
    const std::string ruta = (fs::path(ruta_configuracion()).parent_path() / nombre).string();
    if (ruta == a.emblemas_ruta) return;
    emblemas_guardar(a, a.entrada.ahora_ms, true);
    a.emblemas_ruta = ruta;
    a.g->emblemas_cargar(ruta);
}

void emblemas_guardar(App& a, u64 ahora, bool ya) {
    if (!a.g || a.emblemas_ruta.empty() || !a.g->emblemas_sucios()) return;
    if (!a.t_emblema_sucio) a.t_emblema_sucio = ahora;
    if (!ya && ahora < a.t_emblema_sucio + 3000) return;  // batches of arrivals are written once
    a.t_emblema_sucio = 0;
    a.g->emblemas_guardar(a.emblemas_ruta);
}

}  // namespace lin


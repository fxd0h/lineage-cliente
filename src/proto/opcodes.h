// Opcodes: el código trabaja con ids internos; el número de cable depende de la versión de
// protocolo y se traduce con una tabla (PLAN_CLIENTE.md §4.2).
#pragma once
#include <string_view>
#include "core/tipos.h"

namespace lin {

// Servidor -> cliente (nombres de L1J).
enum class Srv : u8 {
    DESCONOCIDO = 0,
    INITPACKET, SERVERVERSION, LOGINRESULT, COMMONNEWS, CHARAMOUNT, CHARLIST, NEWCHARPACK,
    NEWCHARWRONG, DETELECHAROK, LOGINTOGAME, DISCONNECT, OWNCHARSTATUS, MAPID, CHARPACK,
    MOVEOBJECT, REMOVE_OBJECT, CHANGEHEADING, LIGHT, GAMETIME, WEATHER, SERVERMSG, HPUPDATE,
    MPUPDATE, NORMALCHAT, GLOBALCHAT, CHARVISUALUPDATE, TELEPORT, DOACTIONGFX, SPMR,
    OWNCHARATTRDEF, CHARTITLE, EXP, LAWFUL, WHISPERCHAT, NPCSHOUT, OWNCHARSTATUS2, PACKETBOX,
    BLUEMESSAGE, ATTACKPACKET, HPMETER, INVLIST, ADDITEM, DELETEINVENTORYITEM, ITEMNAME, ITEMAMOUNT,
    ITEMCOLOR, ADDSKILL, DELSKILL, SKILLSOUNDGFX, EFFECTLOCATION, SKILLHASTE, SKILLBRAVE, SKILLICONSHIELD,
    RANGESKILLS, POLY, SHOWHTML, SHOWSHOPBUYLIST, SHOWSHOPSELLLIST, SHOWRETRIEVELIST,
    YES_NO, TRADE, TRADEADDITEM, TRADESTATUS, EMBLEM, SKILLBUY,
    DOOR, PARALYSIS, POISON, CURSEBLIND, INPUTAMOUNT, PETCTRL, SELECTTARGET, TRUETARGET,
    BOOKMARKS, SELECTLIST, IDENTIFYDESC, INVIS, RESURRECTION, STRUP, DEXUP, PINKNAME, LIQUOR, BLESSOFEVA, ABILITY, SOUND, PRIVATESHOPLIST, BOARD, BOARDREAD, MAIL,
    HOUSELIST, HOUSEMAP, WAR, CASTLEMASTER, TAXRATE, DEPOSIT, DRAWAL, USEMAP,
    REDMESSAGE, CLANNAME, CLANATTENTION, CHANGENAME,
    WARTIME,
    CANTIDAD
};

// Cliente -> servidor.
enum class Cli : u8 {
    CLIENTVERSION, LOGINPACKET, COMMONCLICK, NEWCHAR, DELETECHAR, LOGINTOSERVER, MOVECHAR,
    CHANGEHEADING, KEEPALIVE, QUITGAME, CHAT, CHATWHISPER, ATTACK, RESTART, USEITEM, PICKUPITEM,
    DROPITEM, USESKILL, NPCACTION, NPCTALK, RESULT, TELEPORT, ATTR,
    CREATEPARTY, PARTYLIST, LEAVEPARTY, BANPARTY, TRADE, TRADEADDITEM, TRADEADDOK, TRADEADDCANCEL,
    CREATECLAN, JOINCLAN, LEAVECLAN, BANCLAN, TITLE, PLEDGE, WHO, CLAN, EMBLEM, SKILLBUY, SKILLBUYOK,
    DOOR, GIVEITEM, AMOUNT, PETMENU, USEPETITEM, SELECTTARGET,
    CHANGECHAR, FIXWEAPONLIST, SELECTLIST, BOOKMARK, BOOKMARKDELETE, DELETEINVENTORYITEM,
    EXITGHOST, RETURNTOLOGIN, ENTERPORTAL, CHARRESET, EXCLUDE, BUDDYLIST, ADDBUDDY, DELETEBUDDY, CHATPARTY, SHOP, SHOPLIST,
    BOARD, BOARDREAD, BOARDNEXT, BOARDWRITE, BOARDDELETE, MAIL,
    WAR, TAXRATE, DEPOSIT, DRAWAL, CHANGEWARTIME,
    LOGINTOSERVEROK,  // C_LoginToServerOK (opciones de chat): c tipo, c flag
    SHIP,             // C_Ship (s3ep1 33): h mapa de destino, h x, h y, lo manda el cliente al terminar un viaje en barco
    PROPOSE,          // C_Propose: c 0 propose, c 1 divorce
    FIGHT,            // C_Fight (/duel): no body
    CHECKPK,          // C_CheckPK (/checkpk): sin cuerpo
    EXTCOMMAND,       // C_ExtraCommand (s3ep1 38): c acción, un gesto que el servidor reenvía como S_DoActionGFX
    RANK,             // C_Rank: c sub (1 rango, 2 lista de alianza, 3 unirse a la alianza, 4 salir de la alianza), c rango, s nombre
    FISHCLICK,        // C_FishClick: c flag de picada (el servidor lo ignora y juzga el momento él mismo)
    FARATTACK,        // C_FarAttack (tw380 123): same body as ATTACK, sent for bows; where undefined the bow uses ATTACK
    CANTIDAD
};

// Parámetros del formato de los cuerpos que varían entre versiones (los números de opcode van en la
// tabla). Toda diferencia de versión se expresa como un dato de acá, no como un `if` en los
// decodificadores (ver docs/analisis/PLAN_MULTIVERSION.md). Cada campo arranca con el valor del
// 3.63c, que es el caso base.
struct PerfilProtocolo {
    u8 bytes_vida = 2;  // ancho de vida y maná (S_OwnCharStatus, S_HPUpdate, S_MPUpdate); 3.80C stock: 2, login38: 4
    // Cuerpo de ítem (S_AddItem, S_InvList): el 3.63/3.80 ponen antes del tipo un campo h de
    // catalizador mágico (2 bytes) y cierran con una cola fija (c 10, d 0, d 0, h 0 = 11 bytes); el
    // US 2009 no tiene ni uno ni otra.
    u8 bytes_catalizador_item = 2;
    u8 bytes_cola_item = 11;
    // Pregunta sí/no (S_MessageYN) y su respuesta (C_Attr). El 3.63/3.80 anteponen un h 0 y un d con
    // el número de pregunta, y responden con número+tipo+respuesta en 2 bytes; el US 2009 manda solo
    // el tipo y responde con la respuesta en 1 byte (el tipo alcanza para elegir la pregunta). Ver
    // `pregunta` y `responder`: con número = forma larga; sin número (0) = forma corta.
    u8 bytes_pre_pregunta = 2;
    u8 bytes_numero_pregunta = 4;
    // Ancho del campo de golpe/acierto en los paquetes de ataque (S_AttackPacket y variantes): el
    // 3.0c/US 2009 lo manda en 1 byte (`writeC`), el 3.3c en adelante en 2 (`writeH`; ver el comentario
    // de S_AttackPacket.java del jp363, que conserva ambas formas). Leer el ancho equivocado desalinea
    // la dirección y confunde un fallo con un acierto.
    u8 bytes_golpe_ataque = 2;
    // Codificación del texto del servidor: Big5 (Taiwán 3.80C) se convierte a UTF-8 al decodificar; false = CP949 tal como llega.
    bool texto_big5 = false;
    u8 rango_principe = 4;        // nibble de rango de clan propio del príncipe (10 en 3.80C); 0xb = sin clan
    u8 marcadores_ranuras = 104;  // bytes de casillas de S_BookmarkLoad (127 en 3.80C)
    // L1JGO-Whale (tw380) lee la respuesta de C_Attr como [h tipo][c respuesta] aunque S_MessageYN trae la forma
    // larga: con esto activo, `responder` manda la respuesta corta (la larga hacía que toda respuesta se leyera como "no").
    bool respuesta_corta = false;
    // Filas de la lista de S_Mail: l1j-en classic (US 2009) manda `d id, c leída, c aa, c mm, c dd, s remitente, asunto` (toda fila es una
    // carta recibida); L1J-JP / Whale mandan `d id, c leída, d hora unix, c copia propia, s otro, asunto`.
    bool correo_ymd = false;
    // Field widths that dialects override (see Dialecto in opcodes.cpp). Defaults are the stock 3.63 layout.
    u8 bytes_vida_lista = 2;      // HP/MP in the character list (S_CharPacks, S_NewCharPacket); login38: 4
    u8 bytes_ca = 1;              // AC in S_OwnCharStatus; login38: 4
    u8 bytes_ca_reinicio1 = 1;    // AC in S_CharReset sub 1 (reset init); login38: 4
    u8 bytes_ca_reinicio2 = 2;    // AC in S_CharReset sub 2 (level up); login38: 4
    u8 bytes_resistencia = 1;     // elemental resistances in S_OwnCharAttrDef; tw380: 2
    u8 bytes_mr = 1;              // MR in S_SPMR; login38: 4
    u8 bytes_marca_rango = 1;     // per-target hit mark in S_RangeSkill; login38: 2
    u8 bytes_dano_rango = 0;      // per-target damage in S_RangeSkill; login38: 4
    bool sonido_objeto = false;   // S_Sound starts with d object (login38) instead of c type
};

struct TablaOpcodes {
    Srv srv_de_cable[256];                          // número de cable -> interno
    u8 cable_de_cli[static_cast<int>(Cli::CANTIDAD)];  // interno -> número de cable
    // El comando tiene número en esta versión. No alcanza con cable != 0: el 0 es válido (BOOKMARK de s3ep1).
    bool cli_definido[static_cast<int>(Cli::CANTIDAD)];
    u8 cable_init;                                  // opcode del paquete inicial (sin cifrar)
    PerfilProtocolo perfil;                         // formato de los cuerpos para esta versión

    Srv srv(u8 cable) const { return srv_de_cable[cable]; }
    u8 cli(Cli c) const { return cable_de_cli[static_cast<int>(c)]; }
    bool tiene(Cli c) const { return cli_definido[static_cast<int>(c)]; }
};

// Versiones disponibles: "jp363" (L1J-JP 3.63c), "tw380" (3.80C de Taiwán, L1JGO-Whale) y "s3ep1"
// (3.0 English / US 2009, l1j-en/classic).
// PENDIENTE: cargar las demás desde los opcodes.txt de docs/analisis/opcodes (s3ep1, kr380...).
// `dialecto`: a small set of overrides (field widths, opcode remaps) on top of the profile; "original" = none.
// Returns false for an unknown profile or a dialect the profile does not have (tw380 has "login38").
bool tabla_opcodes(std::string_view version, TablaOpcodes& salida, std::string_view dialecto = "original");

const char* nombre_srv(Srv s);

}  // namespace lin

// Big5 (CP950) -> UTF-8. El servidor de Taiwán 3.80C manda su texto en Big5.
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/tipos.h"

namespace lin {

// Convierte texto Big5 a UTF-8. Los bytes sin correspondencia pasan a '?'. El ASCII pasa sin cambios.
void big5_a_utf8(std::string_view entrada, std::string& salida);
// UTF-8 -> Big5, agregado a `salida` (lo que el cliente le manda al servidor de Taiwán). Un carácter sin código Big5
// pasa a '?'. shortcut: un nombre que el servidor mandó con un código Big5 que falta en nuestra tabla ya llegó como '?', así que
// no se puede devolver exacto; guardar los bytes crudos por nombre lo arreglaría si alguna vez importa.
void utf8_a_big5(std::string_view entrada, std::vector<u8>& salida);

}  // namespace lin

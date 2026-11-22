// Big5 (CP950) -> UTF-8. The Taiwan 3.80C server sends its text in Big5.
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "core/tipos.h"

namespace lin {

// Converts Big5 text to UTF-8. Bytes with no mapping become '?'. ASCII passes through unchanged.
void big5_a_utf8(std::string_view entrada, std::string& salida);
// UTF-8 -> Big5, appended to `salida` (what the client sends to the Taiwan server). A character with no Big5 code
// becomes '?'. shortcut: a name the server sent with a Big5 code missing from our table already arrived as '?', so
// it can't be sent back exactly; keeping the raw bytes per name would fix it if it ever matters.
void utf8_a_big5(std::string_view entrada, std::vector<u8>& salida);

}  // namespace lin

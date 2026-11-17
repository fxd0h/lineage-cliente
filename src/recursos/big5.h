// Big5 (CP950) -> UTF-8. The Taiwan 3.80C server sends its text in Big5.
#pragma once
#include <string>
#include <string_view>

namespace lin {

// Converts Big5 text to UTF-8. Bytes with no mapping become '?'. ASCII passes through unchanged.
void big5_a_utf8(std::string_view entrada, std::string& salida);

}  // namespace lin

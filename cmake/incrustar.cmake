# Uso: cmake -DSALIDA=<shaders.h> "-DBINARIOS=<a.spv;b.spv...>" -P incrustar.cmake
# Escribe cada binario como un arreglo de bytes con el nombre del archivo en mayúsculas
# (vs_sprite.spv -> SHADER_VS_SPRITE).
set(texto "// Generado por cmake/incrustar.cmake desde src/render/shaders/*.hlsl. No editar.\n#pragma once\n\n")
foreach(b IN LISTS BINARIOS)
    get_filename_component(nombre ${b} NAME_WE)
    string(TOUPPER ${nombre} nombre)
    file(READ ${b} hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(APPEND texto "inline constexpr unsigned char SHADER_${nombre}[] = {${bytes}};\n")
endforeach()
file(WRITE ${SALIDA} "${texto}")

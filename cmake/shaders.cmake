# Shaders: una sola fuente en HLSL (src/render/shaders/*.hlsl), compilada en cada plataforma al
# formato de su backend de SDL_GPU e incrustada en ${CMAKE_BINARY_DIR}/generado/shaders.h.
# - Windows (D3D12): DXIL con dxc.exe del SDK de Windows (viene con Visual Studio; firma el DXIL).
# - Linux (Vulkan): SPIR-V con glslangValidator (paquete glslang-tools) o, si está, dxc del Vulkan SDK.
# - macOS (Metal): MSL con spirv-cross desde el SPIR-V de glslang (brew install glslang spirv-cross).
#   --msl-decoration-binding usa el binding de SPIR-V como índice de [[buffer]]/[[texture]]/[[sampler]];
#   con -DLIN_MSL, sprite.hlsl corre el storage buffer a buffer(1), detrás de los uniformes, como pide SDL.

set(LIN_SHADERS_FUENTE ${CMAKE_SOURCE_DIR}/src/render/shaders)
# archivo:entrada:etapa
set(LIN_SHADERS
    sprite:vs_sprite:vert sprite:fs_sprite:frag
    brillo:vs_brillo:vert brillo:fs_brillo:frag
    mundo:vs_mundo:vert mundo:fs_mundo:frag)

set(LIN_SHADERS_DIR ${CMAKE_BINARY_DIR}/generado)
set(LIN_SHADERS_H ${LIN_SHADERS_DIR}/shaders.h)
file(MAKE_DIRECTORY ${LIN_SHADERS_DIR})

if(WIN32)
    # Con la consola de desarrollador, WindowsSdkVerBinPath apunta a ...\Windows Kits\10\bin\<versión>\.
    find_program(LIN_DXC dxc
        HINTS "$ENV{WindowsSdkVerBinPath}/x64" "$ENV{WindowsSdkVerBinPath}/arm64"
        DOC "dxc.exe del SDK de Windows (DXIL)")
    if(NOT LIN_DXC)
        message(FATAL_ERROR "No se encontró dxc.exe (SDK de Windows). Compilar desde la consola de desarrollador "
                            "de Visual Studio, o pasar -DLIN_DXC=<ruta a dxc.exe>.")
    endif()
    set(LIN_SHADERS_EXT dxil)
else()
    find_program(LIN_GLSLANG NAMES glslangValidator glslang DOC "glslang (HLSL a SPIR-V)")
    if(NOT LIN_GLSLANG)
        message(FATAL_ERROR "No se encontró glslangValidator: instalar glslang-tools (apt) o glslang (brew).")
    endif()
    set(LIN_SHADERS_EXT spv)
    if(APPLE)
        find_program(LIN_SPIRV_CROSS spirv-cross DOC "spirv-cross (SPIR-V a MSL)")
        if(NOT LIN_SPIRV_CROSS)
            message(FATAL_ERROR "No se encontró spirv-cross: brew install spirv-cross.")
        endif()
        set(LIN_SHADERS_EXT metal)
    endif()
endif()

set(LIN_SHADERS_BINARIOS "")
foreach(s IN LISTS LIN_SHADERS)
    string(REPLACE ":" ";" partes ${s})
    list(GET partes 0 archivo)
    list(GET partes 1 entrada)
    list(GET partes 2 etapa)
    set(salida ${LIN_SHADERS_DIR}/${entrada}.${LIN_SHADERS_EXT})
    if(WIN32)
        if(etapa STREQUAL vert)
            set(perfil vs_6_0)
        else()
            set(perfil ps_6_0)
        endif()
        set(comando ${LIN_DXC} -nologo -T ${perfil} -E ${entrada} -O3 -Fo ${salida} ${LIN_SHADERS_FUENTE}/${archivo}.hlsl)
    elseif(APPLE)
        set(spv ${LIN_SHADERS_DIR}/${entrada}.spv)
        set(comando ${LIN_GLSLANG} -D -V --quiet --auto-sampled-textures -DLIN_MSL -S ${etapa} -e ${entrada}
                -o ${spv} ${LIN_SHADERS_FUENTE}/${archivo}.hlsl
            COMMAND ${LIN_SPIRV_CROSS} --msl --msl-version 20100 --msl-decoration-binding ${spv} --output ${salida})
    else()
        # --auto-sampled-textures: textura y sampler en el mismo binding pasan a ser un sampler
        # combinado, que es lo que espera el backend Vulkan de SDL_GPU.
        set(comando ${LIN_GLSLANG} -D -V --quiet --auto-sampled-textures -S ${etapa} -e ${entrada}
            -o ${salida} ${LIN_SHADERS_FUENTE}/${archivo}.hlsl)
    endif()
    add_custom_command(OUTPUT ${salida} COMMAND ${comando}
        DEPENDS ${LIN_SHADERS_FUENTE}/${archivo}.hlsl
        COMMENT "Shader ${entrada} (${LIN_SHADERS_EXT})" VERBATIM)
    list(APPEND LIN_SHADERS_BINARIOS ${salida})
endforeach()

add_custom_command(OUTPUT ${LIN_SHADERS_H}
    COMMAND ${CMAKE_COMMAND} -DSALIDA=${LIN_SHADERS_H} "-DBINARIOS=${LIN_SHADERS_BINARIOS}"
            -P ${CMAKE_SOURCE_DIR}/cmake/incrustar.cmake
    DEPENDS ${LIN_SHADERS_BINARIOS} ${CMAKE_SOURCE_DIR}/cmake/incrustar.cmake
    COMMENT "Incrustando los shaders en shaders.h" VERBATIM)

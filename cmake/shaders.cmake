# Windows only (included from CMakeLists.txt under if(WIN32)): the HLSL sources in src/render/shaders/*.hlsl are
# compiled to DXIL with dxc.exe (Windows SDK, ships with Visual Studio; it signs the DXIL) and embedded in
# ${CMAKE_BINARY_DIR}/generado/shaders.h as SHADER_<ENTRY> byte arrays, which Gpu::crear_pipeline picks when
# SDL_GetGPUShaderFormats() reports SDL_GPU_SHADERFORMAT_DXIL.
# Linux/Vulkan uses the GLSL twins (*.vert/*.frag -> SPIR-V via herramientas/compilar_shaders.sh) and macOS/Metal the
# hand-written MSL in gpu.cpp and shaders_msl_anime4k.h. A shader change must be made in all three; see docs/fork/README.md.
# One entry point per output; files whose entries bind different textures at the same register are split
# (texto / texto_comp, anime4k_c1..c4 / d2s / resample).

set(LIN_SHADERS_FUENTE ${CMAKE_SOURCE_DIR}/src/render/shaders)
# archivo:entrada:etapa
set(LIN_SHADERS
    sprite:vs_sprite:vert sprite:fs_sprite:frag
    texto:fs_texto:frag texto_comp:fs_texto_comp:frag
    brillo:vs_brillo:vert brillo:fs_brillo:frag
    mundo:vs_mundo:vert mundo:fs_mundo:frag
    escalado:fs_escalado_xbr:frag escalado:fs_escalado_fsr:frag escalado:fs_escalado_crt:frag
    anime4k_c1:fs_anime4k_c1:frag anime4k_c2:fs_anime4k_c2:frag anime4k_c3:fs_anime4k_c3:frag
    anime4k_c4:fs_anime4k_c4:frag anime4k_d2s:fs_anime4k_d2s:frag anime4k_resample:fs_anime4k_resample:frag)

set(LIN_SHADERS_DIR ${CMAKE_BINARY_DIR}/generado)
set(LIN_SHADERS_H ${LIN_SHADERS_DIR}/shaders.h)
file(MAKE_DIRECTORY ${LIN_SHADERS_DIR})

# With the developer console, WindowsSdkVerBinPath points to ...\Windows Kits\10\bin\<version>\.
find_program(LIN_DXC dxc
    HINTS "$ENV{WindowsSdkVerBinPath}/x64" "$ENV{WindowsSdkVerBinPath}/arm64"
    DOC "dxc.exe from the Windows SDK (DXIL)")
if(NOT LIN_DXC)
    message(FATAL_ERROR "dxc.exe (Windows SDK) not found. Build from the Visual Studio developer console, "
                        "or pass -DLIN_DXC=<path to dxc.exe>.")
endif()
set(LIN_SHADERS_EXT dxil)

set(LIN_SHADERS_BINARIOS "")
foreach(s IN LISTS LIN_SHADERS)
    string(REPLACE ":" ";" partes ${s})
    list(GET partes 0 archivo)
    list(GET partes 1 entrada)
    list(GET partes 2 etapa)
    set(salida ${LIN_SHADERS_DIR}/${entrada}.${LIN_SHADERS_EXT})
    if(etapa STREQUAL vert)
        set(perfil vs_6_0)
    else()
        set(perfil ps_6_0)
    endif()
    set(comando ${LIN_DXC} -nologo -T ${perfil} -E ${entrada} -O3 -Fo ${salida} ${LIN_SHADERS_FUENTE}/${archivo}.hlsl)
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

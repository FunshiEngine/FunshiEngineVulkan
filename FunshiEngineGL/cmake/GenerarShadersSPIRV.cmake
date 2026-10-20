# Genera un .cpp con el SPIR-V de los shaders embebido.
#
# Se invoca desde el CMakeLists con:
#   cmake -DENTRADA=lista.txt -DSALIDA="destino.cpp" -P GenerarShadersSPIRV.cmake
#
# ENTRADA es un archivo con un path de SPIR-V por linea. La lista no puede ir
# en el -D: separada por ";" se parte en varias al pasar por el shell (en sh el
# ";" es separador de comandos) y el script deja de recibir los shaders.
#
# Los archivos de entrada son binarios SPIR-V (salida de glslc o
# glslangValidator). El resultado es un .cpp con un array de uint32_t por
# shader, con el nombre del simbolo derivado del nombre del archivo: "Malla.vert"
# se convierte en "kMallaVert".

if(NOT DEFINED ENTRADA OR NOT DEFINED SALIDA)
    message(FATAL_ERROR "GenerarShadersSPIRV: faltan ENTRADA o SALIDA")
endif()

if(NOT EXISTS "${ENTRADA}")
    message(FATAL_ERROR "GenerarShadersSPIRV: no existe la lista ${ENTRADA}")
endif()

file(STRINGS "${ENTRADA}" ARCHIVOS_SPIRV)

# Cada palabra del binario es un uint32 en orden little-endian: los bytes se
# invierten al pasarlos a hexadecimal.
function(palabra_a_literal palabra RESULTADO)
    string(SUBSTRING "${palabra}" 0 2 b0)
    string(SUBSTRING "${palabra}" 2 2 b1)
    string(SUBSTRING "${palabra}" 4 2 b2)
    string(SUBSTRING "${palabra}" 6 2 b3)
    set(${RESULTADO} "0x${b3}${b2}${b1}${b0}u" PARENT_SCOPE)
endfunction()

set(CONTENIDO "/*\n")
string(APPEND CONTENIDO "    FunshiEngineGL - Motor de juegos 3D con API grafica intercambiable e ImGui\n")
string(APPEND CONTENIDO "    Copyright 2026 Gianfranco Ivan Enrique\n\n")
string(APPEND CONTENIDO "    Licensed under the Apache License, Version 2.0 (the \"License\");\n")
string(APPEND CONTENIDO "    you may not use this file except in compliance with the License.\n")
string(APPEND CONTENIDO "    You may obtain a copy of the License at\n\n")
string(APPEND CONTENIDO "        http://www.apache.org/licenses/LICENSE-2.0\n\n")
string(APPEND CONTENIDO "    Unless required by applicable law or agreed to in writing, software\n")
string(APPEND CONTENIDO "    distributed under the License is distributed on an \"AS IS\" BASIS,\n")
string(APPEND CONTENIDO "    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n")
string(APPEND CONTENIDO "    See the License for the specific language governing permissions and\n")
string(APPEND CONTENIDO "    limitations under the License.\n\n")
string(APPEND CONTENIDO "    SPDX-License-Identifier: Apache-2.0\n")
string(APPEND CONTENIDO "*/\n")
string(APPEND CONTENIDO "// Archivo generado: no editar a mano. El origen son los shaders de\n")
string(APPEND CONTENIDO "// src/Rendering/BackendVulkan/Shaders, compilados a SPIR-V en tiempo de build.\n")
string(APPEND CONTENIDO "#include \"Rendering/BackendVulkan/ShadersSPIRV.h\"\n\n")
string(APPEND CONTENIDO "namespace Rendering {\nnamespace Backend {\nnamespace Vulkan {\nnamespace Shaders {\n\n")

foreach(archivo ${ARCHIVOS_SPIRV})
    # "Malla.vert.spv" -> tipo "Malla", etapa "vert", simbolo "kMallaVert".
    # El nombre se parte a mano porque el nombre del archivo tiene dos puntos y
    # get_filename_component no los separa como hace falta.
    get_filename_component(nombre ${archivo} NAME)
    string(REGEX REPLACE "\\.spv$" "" base "${nombre}")
    string(REGEX REPLACE "^([^.]*)\\..*$" "\\1" tipo "${base}")
    string(REGEX REPLACE "^[^.]*\\." "" paso "${base}")

    # La etapa se escribe en camelCase: primera letra en mayuscula y el resto
    # en minuscula ("vert" -> "Vert").
    string(SUBSTRING "${paso}" 0 1 inicial)
    string(TOUPPER "${inicial}" inicial)
    string(SUBSTRING "${paso}" 1 -1 resto)
    string(TOLOWER "${resto}" resto)
    set(simbolo "k${tipo}${inicial}${resto}")

    file(READ "${archivo}" hexadecimal HEX)
    string(REGEX MATCHALL
        "[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]"
        palabras "${hexadecimal}")

    set(elementos "")
    foreach(palabra ${palabras})
        palabra_a_literal("${palabra}" literal)
        string(APPEND elementos "    ${literal},\n")
    endforeach()

    file(SIZE "${archivo}" tamano)

    string(APPEND CONTENIDO "const std::uint32_t ${simbolo}[] = {\n")
    string(APPEND CONTENIDO "${elementos}")
    string(APPEND CONTENIDO "};\n")
    string(APPEND CONTENIDO "const std::size_t ${simbolo}Bytes = ${tamano};\n\n")
endforeach()

string(APPEND CONTENIDO "} // namespace Shaders\n")
string(APPEND CONTENIDO "} // namespace Vulkan\n")
string(APPEND CONTENIDO "} // namespace Backend\n")
string(APPEND CONTENIDO "} // namespace Rendering\n")

file(WRITE "${SALIDA}" "${CONTENIDO}")

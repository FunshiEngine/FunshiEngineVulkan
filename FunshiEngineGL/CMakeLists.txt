cmake_minimum_required(VERSION 3.15)

# Versión del producto, fuente ÚNICA: alimenta la de project() y el recurso
# VERSIONINFO del .exe de Windows (packaging/icon/app.rc.in). Es una variable de
# caché para que se pueda sobreescribir al configurar (-DFUNSHI_VERSION=1.2.3),
# que es lo que hace el workflow al publicar: así el número del .exe y el del
# instalador salen del mismo tag, en vez de dos números escritos a mano que
# nadie sincroniza.
set(FUNSHI_VERSION "0.5.4" CACHE STRING "Versión del producto (X.Y.Z)")
# C y CXX: el proyecto es C++, pero las distribuciones de CEF (cef_variables.cmake)
# corren CHECK_C_COMPILER_FLAG en configure y CMake exige el lenguaje habilitado.
project(FunshiEngineGL VERSION ${FUNSHI_VERSION} LANGUAGES C CXX)
message(STATUS "FunshiEngineGL ${PROJECT_VERSION} (FUNSHI_VERSION=${FUNSHI_VERSION})")
# Sustituciones de configure_file para el .rc: el icono se referencia relativo
# al .rc (se copia al build), la version se lleva a comas (1,2,3,0).
set(FUNSHI_APP_ICON "app.ico")
string(REPLACE "." "," FUNSHI_VERSION_COMMA "${FUNSHI_VERSION}")
set(FUNSHI_VERSION_COMMA "${FUNSHI_VERSION_COMMA},0")

# Configuración básica
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Runtime de C++ de MSVC (CMP0091): el engine y las .dll de script que compila
# en runtime tienen que usar el MISMO CRT. Los SerializeField cruzan la frontera
# de la .dll con std::string, std::vector y std::function: la memoria se aloca
# en un modulo y se libera en el otro, y cada CRT estatico (/MT) trae su propio
# heap (liberar memoria del otro corrompe el heap y el motor muere). Se declara
# explicito para que el contrato sea visible en el build y no un default
# implicito de CMake. En Debug el runtime es /MDd, y FUNSHI_CXX_RUNTIME_FLAG
# hornea ese mismo valor para la linea de compilacion del script.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(FUNSHI_CXX_RUNTIME_FLAG "$<IF:$<CONFIG:Debug>,/MDd,/MD>")

# Configuración de rutas. CMAKE_CURRENT_SOURCE_DIR, no CMAKE_SOURCE_DIR: la
# raiz del engine se consumio tambien via add_subdirectory (el exportador de
# juegos), y ahi CMAKE_SOURCE_DIR es el proyecto temporal, no este directorio.
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Búsqueda de archivos fuente
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp ${SRC_DIR}/*.c)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS ${SRC_DIR}/*.h ${SRC_DIR}/*.hpp)

# Las herramientas de proyecto y diagnóstico se comparten entre el editor y
# Funshi CLI. Estas bibliotecas no dependen de la GUI ni de la pila gráfica.
list(REMOVE_ITEM SOURCES
    ${SRC_DIR}/Configuracion/ProjectManager.cpp
    ${SRC_DIR}/Configuracion/ProjectPaths.cpp
    ${SRC_DIR}/Diagnosticos/RegistroDiagnosticos.cpp
    ${SRC_DIR}/ExcepcionesCPP/RuntimeException.cpp)
# El backend CEF exige C++20 (headers de CEF) y el motor compila en C++17, asi
# que vive en su propia unidad/target (funshi_webui_cef) y no entra en la GLOB.
list(REMOVE_ITEM SOURCES ${SRC_DIR}/WebUI/BackendCef.cpp)
add_library(funshi_project_core STATIC
    ${SRC_DIR}/Configuracion/ProjectManager.cpp
    ${SRC_DIR}/Configuracion/ProjectPaths.cpp)
target_include_directories(funshi_project_core PUBLIC ${SRC_DIR})

add_library(funshi_diagnostics_core STATIC
    ${SRC_DIR}/Diagnosticos/RegistroDiagnosticos.cpp
    ${SRC_DIR}/ExcepcionesCPP/RuntimeException.cpp)
target_include_directories(funshi_diagnostics_core
    PUBLIC ${SRC_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/External)
target_link_libraries(funshi_diagnostics_core PUBLIC funshi_project_core)
if(WIN32)
    target_link_libraries(funshi_diagnostics_core PUBLIC dbghelp)
else()
    target_link_libraries(funshi_diagnostics_core PUBLIC ${CMAKE_DL_LIBS})
endif()



# El motor se compila como libreria estatica (funshi_engine) y el ejecutable
# solo aporta el punto de entrada (main.cpp). Asi el engine es reutilizable
# (tests, herramientas, otros front-ends) sin recompilar todo. CI puede compilar
# SOLO las pruebas headless con BUILD_ENGINE=OFF.
option(BUILD_ENGINE "Build engine library and executable" ON)
# Soporte de scripts Java via JNI: requiere JDK en tiempo de build para jni.h.
# El JVM en runtime se carga dinamicamente solo si hay una escena con .java.
# Por defecto ON: si el build encuentra el JDK se habilita solo, si no, el
# backend Java se omite sin romper la compilacion de nada.
option(FUNSHI_JAVA "Soporte de scripts Java via JNI (requiere JDK en build)" ON)
# Soporte de interfaces de juego web (HUD en HTML/CSS/JS). El contrato
# (IWebUIBackend) y el backend nulo se compilan siempre; esta opcion habilita los
# backends web reales (p. ej. CEF) cuando el motor se enlaza con sus
# dependencias. Sin ningun backend real el runtime cae al nulo y no dibuja nada.
option(FUNSHI_WEBUI "Soporte de interfaces web (backend CEF)" ON)
# Soporte de backend CEF concreto (consume distribucion CEF descargada en
# configure). Requiere FUNSHI_WEBUI=ON.
option(FUNSHI_CEF "Compilar backend CEF real" OFF)
# Build runtime-only: solo la libreria del motor, sin ejecutable del editor ni
# objetivos de prueba. Lo usa el exportador de juegos para compilar el binario
# standalone (add_subdirectory de la raiz del engine con esta opcion FORZADA).
option(BUILD_RUNTIME "Construir solo la libreria del motor (sin editor ni pruebas)" OFF)

if(BUILD_ENGINE)
    # main.cpp y main_juego.cpp son puntos de entrada, no parte de la libreria.
    list(REMOVE_ITEM SOURCES ${SRC_DIR}/main.cpp)
    list(REMOVE_ITEM SOURCES ${SRC_DIR}/Runtime/main_juego.cpp)

    # El backend Java se compila cuando FUNSHI_JAVA=ON y hay JDK disponible.
    # Debe ir en la libreria principal (funshi_engine) para que los juegos
    # exportados tengan soporte de scripts Java. El target scripts-java-tests
    # lo compila aparte solo para los tests de CI.
    if(FUNSHI_JAVA)
        if(NOT Java_JAVAC_EXECUTABLE)
            # Sin JDK: quitamos BackendJava.cpp para que no falle la
            # compilacion (no hay jni.h).
            list(REMOVE_ITEM SOURCES ${SRC_DIR}/Behaviour/Backends/BackendJava.cpp)
        endif()
    else()
        # FUNSHI_JAVA=OFF: BackendJava.cpp no es necesario en la libreria
        # del motor.
        list(REMOVE_ITEM SOURCES ${SRC_DIR}/Behaviour/Backends/BackendJava.cpp)
    endif()

    add_library(funshi_engine STATIC ${SOURCES} ${HEADERS})
    target_link_libraries(funshi_engine PUBLIC
        funshi_project_core funshi_diagnostics_core)

    if(NOT BUILD_RUNTIME)
    # Ejecutable principal (solo el entry point).
    add_executable(${PROJECT_NAME} ${SRC_DIR}/main.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE funshi_engine)
    # En Windows el binario es GUI (sin consola): WinMain arma argc/argv desde
    # GetCommandLineW y delega en el arranque comun, y la salida va al log, asi
    # no se abre ninguna terminal. En Linux no tiene efecto.
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE shell32)
        # Recurso Windows del ejecutable: icono (app.ico) + bloque VERSIONINFO
        # con CompanyName/ProductVersion. El .rc se genera por configure_file
        # al directorio de build (junto a una copia del .ico) y el compilador de
        # recursos de MSVC/MinGW lo enlaza al .exe.
        configure_file(
            ${CMAKE_SOURCE_DIR}/packaging/icon/app.rc.in
            ${CMAKE_BINARY_DIR}/FunshiEngineGL.rc @ONLY)
        file(COPY ${CMAKE_SOURCE_DIR}/packaging/icon/app.ico
             DESTINATION ${CMAKE_BINARY_DIR})
        target_sources(${PROJECT_NAME} PRIVATE
            ${CMAKE_BINARY_DIR}/FunshiEngineGL.rc)
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    endif() # NOT BUILD_RUNTIME (sin ejecutable del editor)
    target_include_directories(funshi_engine PUBLIC ${SRC_DIR})
    target_compile_definitions(funshi_engine PUBLIC GLM_ENABLE_EXPERIMENTAL)

    if(FUNSHI_WEBUI)
        target_compile_definitions(funshi_engine PUBLIC FUNSHI_WEBUI)
    endif()

    # --- Backend CEF ----------------------------------------------------------
    # Solo con FUNSHI_CEF=ON se descarga la distribución (~657 MB) y se
    # compila funshi_webui_cef (C++20). Con FUNSHI_WEBUI=ON pero FUNSHI_CEF=OFF
    # el motor compila sin CEF y el runtime cae al backend nulo.
    if(FUNSHI_CEF AND FUNSHI_WEBUI)
        if(WIN32)
            set(CEF_PLATFORM "windows64")
        else()
            set(CEF_PLATFORM "linux64")
        endif()
        set(CEF_VERSION "154.0.34+g14c5a08+chromium-154.0.8037.98")
        set(CEF_PKG "cef_binary_${CEF_VERSION}_${CEF_PLATFORM}.tar.bz2")
        # SHA1 pineado de la distribución Linux (sonda CEF OSR). En Windows
        # todavía no hay pin verificado: se descarga sin EXPECTED_HASH y se
        # avisa en el log de configure hasta pinarlo.
        set(CEF_SHA1 "d0a822afcf769371e961344b64f5a461066b6a0f")
        # La distribución vive bajo el build dir, no en el source: no contamina
        # el árbol ni el estado de git (ver .gitignore: .cef/, .cef_cache/).
        set(CEF_CACHE_DIR "${CMAKE_BINARY_DIR}/cef_cache")
        set(CEF_SRC_DIR "${CMAKE_BINARY_DIR}/cef")
        if(NOT EXISTS "${CEF_CACHE_DIR}")
            file(MAKE_DIRECTORY "${CEF_CACHE_DIR}")
        endif()
        if(NOT EXISTS "${CEF_SRC_DIR}")
            file(MAKE_DIRECTORY "${CEF_SRC_DIR}")
        endif()
        set(CEF_TAR "${CEF_CACHE_DIR}/${CEF_PKG}")
        if(NOT EXISTS "${CEF_TAR}")
            message(STATUS "Descargando CEF ${CEF_VERSION}...")
            if(WIN32)
                message(WARNING "CEF Windows sin SHA1 pineado: descarga sin "
                    "verificacion hasta pinarlo.")
                file(DOWNLOAD
                    "https://cef-builds.spotifycdn.com/${CEF_PKG}"
                    "${CEF_TAR}"
                    SHOW_PROGRESS)
            else()
                file(DOWNLOAD
                    "https://cef-builds.spotifycdn.com/${CEF_PKG}"
                    "${CEF_TAR}"
                    EXPECTED_HASH SHA1=${CEF_SHA1}
                    SHOW_PROGRESS)
            endif()
        endif()
        set(CEF_ROOT "${CEF_SRC_DIR}/cef_binary_${CEF_VERSION}_${CEF_PLATFORM}")
        if(NOT EXISTS "${CEF_ROOT}/include/cef_app.h")
            message(STATUS "Descomprimiendo CEF en ${CEF_SRC_DIR}...")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xjf "${CEF_TAR}"
                WORKING_DIRECTORY "${CEF_SRC_DIR}"
                RESULT_VARIABLE TAR_RES
            )
            if(NOT TAR_RES EQUAL 0)
                message(FATAL_ERROR "No se pudo descomprimir ${CEF_TAR}")
            endif()
        endif()
        if(EXISTS "${CEF_ROOT}/cmake")
            set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CEF_ROOT}/cmake")
            find_package(CEF REQUIRED)
            # Solo el wrapper (cef_variables fija CEF_LIBCEF_DLL_WRAPPER_PATH a
            # <distro>/libcef_dll). NO se agrega el CMakeLists raiz de la
            # distribucion: ese raiz tambien agrega cefsimple/cefclient/ceftests,
            # que son muestras de CEF compiladas con -Werror y no compilan con el
            # GCC actual (-Werror=self-move); el motor solo necesita el wrapper.
            add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper)
            if(OS_LINUX)
                ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
            endif()
            add_library(funshi_webui_cef STATIC ${SRC_DIR}/WebUI/BackendCef.cpp)
            set_target_properties(funshi_webui_cef PROPERTIES
                CXX_STANDARD 20
                CXX_STANDARD_REQUIRED ON
                CXX_EXTENSIONS OFF)
            target_include_directories(funshi_webui_cef PRIVATE
                ${SRC_DIR}
                ${CEF_ROOT}
                ${CMAKE_CURRENT_SOURCE_DIR}/External)
            add_dependencies(funshi_webui_cef libcef_dll_wrapper)
            # Orden de enlace estatico: primero el consumidor (wrapper), despues
            # el proveedor (libcef.so). Al reves, GNU ld no rescane libcef.so y
            # el wrapper queda con cef_string_* sin resolver.
            target_link_libraries(funshi_webui_cef PRIVATE
                libcef_dll_wrapper
                libcef_lib
                ${CEF_STANDARD_LIBS})
            if(WIN32)
                target_compile_definitions(funshi_webui_cef PRIVATE _HAS_STD_BYTE=0)
            endif()
            target_compile_definitions(funshi_engine PUBLIC FUNSHI_CEF)
            target_link_libraries(funshi_engine PRIVATE funshi_webui_cef)
            if(NOT BUILD_RUNTIME AND TARGET ${PROJECT_NAME})
                target_link_libraries(${PROJECT_NAME} PRIVATE funshi_webui_cef)
            endif()
            message(STATUS "CEF ${CEF_VERSION} habilitado (funshi_webui_cef C++20).")
            # El runtime de CEF (libcef + recursos) debe estar junto al
            # ejecutable: CefInitialize los resuelve contra el dir del binario.
            # Sin esta copia, inicializar() falla fuera del árbol de la sonda.
            if(NOT BUILD_RUNTIME AND TARGET ${PROJECT_NAME})
                if(UNIX)
                    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Release/libcef.so"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libcef.so"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Resources/chrome_100_percent.pak"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/chrome_100_percent.pak"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Resources/chrome_200_percent.pak"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/chrome_200_percent.pak"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Resources/resources.pak"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources.pak"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Resources/icudtl.dat"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/icudtl.dat"
                        COMMAND ${CMAKE_COMMAND} -E copy_directory
                            "${CEF_ROOT}/Resources/locales"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/locales"
                        COMMENT "Copiando runtime CEF junto al editor")
                else()
                    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Release/libcef.dll"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/libcef.dll"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Resources/chrome_100_percent.pak"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/chrome_100_percent.pak"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Resources/chrome_200_percent.pak"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/chrome_200_percent.pak"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Resources/resources.pak"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources.pak"
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${CEF_ROOT}/Resources/icudtl.dat"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/icudtl.dat"
                        COMMAND ${CMAKE_COMMAND} -E copy_directory
                            "${CEF_ROOT}/Resources/locales"
                            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/locales"
                        COMMENT "Copiando runtime CEF junto al editor")
                endif()
            endif()
        else()
            message(WARNING "No se encontro CEF en ${CEF_ROOT}; backend CEF no se habilitara.")
        endif()
    endif()

    # Soporte opcional de scripts Java (JNI). Se detecta el JDK con FindJNI y
    # se embeben las rutas de javac/libjvm para el runtime (se pueden
    # sobreescribir con las variables de entorno JAVA_HOME/JAVAC/FUNSHI_LIBJVM).
    # Sin JDK en el sistema el backend simplemente no se compila, y la GUI lo
    # reporta como "sin soporte".
    find_package(JNI QUIET)
    if(FUNSHI_JAVA AND JNI_FOUND)
        find_package(Java QUIET COMPONENTS Development)
        target_sources(funshi_engine PRIVATE
            ${SRC_DIR}/Behaviour/Backends/BackendJava.cpp)
        target_include_directories(funshi_engine PRIVATE ${JNI_INCLUDE_DIRS})
        target_compile_definitions(funshi_engine PUBLIC FUNSHI_JAVA)
        if(Java_JAVAC_EXECUTABLE)
            target_compile_definitions(funshi_engine
                PRIVATE FUNSHI_JAVAC_DEFAULT="${Java_JAVAC_EXECUTABLE}")
        endif()
        if(JAVA_JVM_LIBRARY)
            target_compile_definitions(funshi_engine
                PRIVATE FUNSHI_LIBJVM_DEFAULT="${JAVA_JVM_LIBRARY}")
        endif()
        if(UNIX)
            target_link_libraries(funshi_engine PRIVATE ${CMAKE_DL_LIBS})
        endif()
        message(STATUS
            "Scripts Java HABILITADOS (javac: ${Java_JAVAC_EXECUTABLE}).")
    elseif(FUNSHI_JAVA)
        message(STATUS
            "FUNSHI_JAVA=ON pero no se encontro el JDK (jni.h): el backend "
            "Java queda desactivado.")
    endif()

    # Cabeceras que un script C++ necesita al compilarse en caliente. El valor
    # horneado es un NOMBRE relativo, no la ruta del workspace de build: el
    # BackendCpp lo resuelve contra la carpeta del ejecutable, asi el mismo
    # binario funciona en el arbol de desarrollo (build/include) y en la
    # instalacion (<app>/include). La ruta absoluta al checkout solo sirve en la
    # maquina que construyo el motor y no existe para el usuario.
    set(FUNSHI_SRC_DIR_RELATIVO "include")
    add_custom_target(funshi_cabeceras_script ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_BINARY_DIR}/include/Behaviour/Reflection"
                "${CMAKE_BINARY_DIR}/include/Matematicas"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SRC_DIR}/Behaviour/IScriptBehaviour.h"
                "${SRC_DIR}/Behaviour/ScriptGameObject.h"
                "${CMAKE_BINARY_DIR}/include/Behaviour"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SRC_DIR}/Behaviour/Reflection/BehaviourReflection.h"
                "${CMAKE_BINARY_DIR}/include/Behaviour/Reflection"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SRC_DIR}/Matematicas/StructVec3.h"
                "${CMAKE_BINARY_DIR}/include/Matematicas"
        COMMENT "Copiando las cabeceras que necesitan los scripts C++")
    add_dependencies(funshi_engine funshi_cabeceras_script)

    # Compilador C++ a invocar por el BackendCpp. El runtime de C++ del build lo
    # usa la linea de compilacion de los scripts (ComandoCompilacionCpp.h) para
    # no desajustar el CRT.
    target_compile_definitions(funshi_engine
        PUBLIC FUNSHI_SRC_DIR="${FUNSHI_SRC_DIR_RELATIVO}"
               FUNSHI_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
               FUNSHI_CXX_RUNTIME_FLAG="${FUNSHI_CXX_RUNTIME_FLAG}")
    # Raiz de los fuentes del engine, horneada: el exportador de juegos la usa
    # para el add_subdirectory del proyecto de exportacion sin deducirla de
    # rutas relativas al proyecto (que apuntan a MotorGrafico/, no al engine).
    target_compile_definitions(funshi_engine
        PUBLIC FUNSHI_ENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# 1. Configuración de GLFW
#
# GLFW es la capa de ventana e input: no es una API gráfica. El motor crea la
# ventana sin contexto gráfico (GLFW_NO_API) y deja que el backend activo se
# haga cargo de la superficie, así que GLFW es la única dependencia de bajo
# nivel que el core necesita.
find_package(glfw3 REQUIRED)
target_link_libraries(funshi_engine PRIVATE glfw)

# --- Backend de Vulkan --------------------------------------------------------
#
# Este repositorio implementa el backend de Vulkan del contrato de render, así
# que la API gráfica sí está presente: la ventana se crea sin contexto
# (GLFW_NO_API) y el backend crea la superficie.
#
# La librería de Vulkan se ENLAZA (no se carga en tiempo de ejecución): el
# motor igual arranca en una máquina sin Vulkan, porque el backend consulta la
# disponibilidad antes de inicializar y, si no hay, sigue con el backend nulo.
option(FUNSHI_VULKAN "Compilar el backend de Vulkan" ON)

if(FUNSHI_VULKAN)
    find_package(Vulkan REQUIRED)
    target_link_libraries(funshi_engine PRIVATE Vulkan::Vulkan)
    # La fachada de la interfaz compila el backend de render segun la API
    # grafica que hay en este repositorio.
    target_compile_definitions(funshi_engine PUBLIC FUNSHI_VULKAN=1)

    # --- Shaders ---------------------------------------------------------------
    #
    # El GLSL de los shaders vive en src/Rendering/BackendVulkan/Shaders y se
    # compila a SPIR-V en tiempo de build (con glslc de shaderc o con
    # glslangValidator). El binario se embebe en el ejecutable: en runtime no
    # hace falta un compilador de shaders ni leer archivos.
    set(FUNSHI_SHADERS_DIR ${SRC_DIR}/Rendering/BackendVulkan/Shaders)

    find_program(FUNSHI_GLSLC NAMES glslc
        HINTS ENV VULKAN_SDK
        PATH_SUFFIXES bin)
    find_program(FUNSHI_GLSLANG NAMES glslangValidator
        HINTS ENV VULKAN_SDK
        PATH_SUFFIXES bin)

    if(FUNSHI_GLSLC)
        set(FUNSHI_COMPILADOR_SHADERS glslc)
    elseif(FUNSHI_GLSLANG)
        set(FUNSHI_COMPILADOR_SHADERS glslangValidator)
    else()
        message(FATAL_ERROR
            "FUNSHI_VULKAN=ON necesita un compilador de shaders a SPIR-V: "
            "instala glslc (shaderc) o glslangValidator, o configura con "
            "-DFUNSHI_VULKAN=OFF.")
    endif()

    file(GLOB FUENTES_SHADERS CONFIGURE_DEPENDS
        "${FUNSHI_SHADERS_DIR}/*.vert"
        "${FUNSHI_SHADERS_DIR}/*.frag")

    set(ARCHIVOS_SPIRV)
    foreach(fuente ${FUENTES_SHADERS})
        get_filename_component(nombre ${fuente} NAME)
        get_filename_component(etapa ${nombre} EXT)
        string(REPLACE "." "" etapa ${etapa})
        set(spv ${CMAKE_CURRENT_BINARY_DIR}/shaders/${nombre}.spv)
        if(FUNSHI_COMPILADOR_SHADERS STREQUAL "glslc")
            add_custom_command(
                OUTPUT ${spv}
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        ${CMAKE_CURRENT_BINARY_DIR}/shaders
                COMMAND ${FUNSHI_GLSLC} -fshader-stage=${etapa}
                        --target-env=vulkan1.1 -o ${spv} ${fuente}
                DEPENDS ${fuente}
                COMMENT "Compilando ${nombre} a SPIR-V")
        else()
            add_custom_command(
                OUTPUT ${spv}
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        ${CMAKE_CURRENT_BINARY_DIR}/shaders
                COMMAND ${FUNSHI_GLSLANG} -S ${etapa} --target-env vulkan1.1
                        -o ${spv} ${fuente}
                DEPENDS ${fuente}
                COMMENT "Compilando ${nombre} a SPIR-V")
        endif()
        list(APPEND ARCHIVOS_SPIRV ${spv})
    endforeach()

    set(ARCHIVO_SPIRV_GENERADO
        ${CMAKE_CURRENT_BINARY_DIR}/ShadersSPIRV.generado.cpp)
    # La lista de archivos se pasa por archivo y no en el -D: una lista de CMake
    # separada por ";" se parte en varias al pasar por el shell (en sh el ";"
    # es separador de comandos) y el script deja de recibir los shaders.
    set(ARCHIVO_LISTA_SPIRV ${CMAKE_CURRENT_BINARY_DIR}/shaders.lista.txt)
    string(REPLACE ";" "\n" LISTA_SPIRV "${ARCHIVOS_SPIRV}")
    file(WRITE ${ARCHIVO_LISTA_SPIRV} "${LISTA_SPIRV}\n")
    add_custom_command(
        OUTPUT ${ARCHIVO_SPIRV_GENERADO}
        COMMAND ${CMAKE_COMMAND}
                "-DENTRADA=${ARCHIVO_LISTA_SPIRV}"
                "-DSALIDA=${ARCHIVO_SPIRV_GENERADO}"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerarShadersSPIRV.cmake
        DEPENDS ${ARCHIVOS_SPIRV}
                ${ARCHIVO_LISTA_SPIRV}
                ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerarShadersSPIRV.cmake
        COMMENT "Generando el SPIR-V embebido de los shaders")
    target_sources(funshi_engine PRIVATE ${ARCHIVO_SPIRV_GENERADO})

    # --- Pruebas headless del backend de Vulkan ------------------------------
    #
    # El backend se comprueba sin ventana: se dibuja sobre un render target
    # fuera de pantalla y se leen los pixeles de la imagen. Si la maquina no
    # tiene Vulkan el test sale con 77 (skipped) y la suite sigue verde.
    add_executable(vulkan-backend-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/VulkanBackendTests.cpp
    )
    target_include_directories(vulkan-backend-tests PRIVATE
        ${SRC_DIR}
        ${CMAKE_SOURCE_DIR}/External
    )
    target_link_libraries(vulkan-backend-tests PRIVATE funshi_engine Vulkan::Vulkan)
    set_target_properties(vulkan-backend-tests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    add_test(NAME vulkan-backend-tests COMMAND vulkan-backend-tests)
else()
    target_compile_definitions(funshi_engine PUBLIC FUNSHI_VULKAN=0)
    # Sin el backend de Vulkan no se compilan ni el backend ni el de ImGui:
    # hacen falta los headers de la libreria de Vulkan.
    list(REMOVE_ITEM SOURCES
        ${SRC_DIR}/Rendering/BackendVulkan/BackendVulkan.cpp
        ${SRC_DIR}/Rendering/BackendVulkan/BackendVulkanDibujo.cpp
        ${SRC_DIR}/Rendering/BackendVulkan/BackendVulkanRecursos.cpp
        ${SRC_DIR}/Rendering/BackendVulkan/BackendVulkanShaders.cpp
        ${SRC_DIR}/Rendering/BackendVulkan/LayoutUniformes.cpp
        ${SRC_DIR}/ImGui/imgui_impl_vulkan.cpp)
endif()

# Bullet Physics
find_package(Bullet REQUIRED)

# Si Bullet define targets modernos
if(TARGET Bullet::BulletDynamics)
    target_link_libraries(funshi_engine PRIVATE
        Bullet::BulletDynamics
        Bullet::BulletCollision
        Bullet::LinearMath
    )
else()
    # Método clásico: uso de variables
    target_include_directories(funshi_engine PUBLIC ${BULLET_INCLUDE_DIRS})
    target_link_libraries(funshi_engine PRIVATE
        ${BULLET_LIBRARIES}
    )
endif()

# --- Sanitizers --------------------------------------------------------------
option(ENABLE_ASAN "Enable AddressSanitizer for Debug builds" ON)

# Asegurate de generar en Debug para mejores trazas:
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# Los flags viven en un target INTERFACE para no duplicarlos: se aplican a la
# libreria y, por propagacion de usage requirements, tambien al ejecutable que
# la enlaza (los link options deben llegar al binario final, no al .a).
add_library(funshi_sanitizers INTERFACE)
if(ENABLE_ASAN AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
  message(STATUS "Building with AddressSanitizer (ASan)")
  # Compilación: ASan + mejores stack traces + símbolos + optimización ligera
  target_compile_options(funshi_sanitizers INTERFACE
    -fsanitize=address
    -fno-omit-frame-pointer
    -O1
    -g
  )
  # Enlace: ASan (importante agregar también al linkeo)
  target_link_options(funshi_sanitizers INTERFACE
    -fsanitize=address
  )

  # Opcional pero útil: UndefinedBehaviorSanitizer (UBSan)
  # (si molesta en 3rd-party libs, desactivalo)
  target_compile_options(funshi_sanitizers INTERFACE -fsanitize=undefined)
  target_link_options(funshi_sanitizers INTERFACE -fsanitize=undefined)
endif()
target_link_libraries(funshi_engine PUBLIC funshi_sanitizers)
# ---------------------------------------------------------------------------


# Configuración para Windows
if(WIN32)
    # dbghelp: CapturaBackTrace/SymFromAddr/SymInitialize (RuntimeException).
    # En MinGW los #pragma comment(lib) de MSVC se ignoran, hay que enlazarla.
    target_link_libraries(funshi_engine PRIVATE dbghelp)
    # Evitar conflicto std::byte (C++17) vs typedef byte en headers Windows (rpcndr.h, etc.)
    target_compile_definitions(funshi_engine PRIVATE _HAS_STD_BYTE=0)
endif()

# Dependencias específicas de Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(funshi_engine PRIVATE
        dl
        X11
        pthread
        util
        Xrandr
        Xi
    )

    # Ncurses
    find_package(Curses REQUIRED)
    target_include_directories(funshi_engine PUBLIC ${CURSES_INCLUDE_DIRS})
    target_link_libraries(funshi_engine PRIVATE ${CURSES_LIBRARIES})
endif()

# ImGui
file(GLOB IMGUI_SRC ${SRC_DIR}/ImGui/*.cpp ${SRC_DIR}/ImGui/backends/*.cpp)
target_sources(funshi_engine PRIVATE ${IMGUI_SRC})
target_include_directories(funshi_engine PUBLIC ${SRC_DIR}/ImGui ${SRC_DIR}/ImGui/backends)

# ImGuizmo (fuera de src)
file(GLOB IMGUZIMO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/ImGuizmo/*.cpp)
target_sources(funshi_engine PRIVATE ${IMGUZIMO_SRC})
target_include_directories(funshi_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ImGuizmo)

# nlohmann/json (vendoriado en External/nlohmann) para la configuracion del
# editor (EditorConfig). Mismamente el nombre .hpp cuelga de External/.
target_include_directories(funshi_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/External)

# Assimp
option(USE_ASSIMP "Enable Assimp model loading" ON)
if(USE_ASSIMP)
    find_package(assimp REQUIRED)
    target_include_directories(funshi_engine PUBLIC ${ASSIMP_INCLUDE_DIRS})
    target_link_libraries(funshi_engine PRIVATE assimp::assimp)
endif()

# GLM (solo headers): se usa el directorio embebido si existe en el repo; si no
# (p. ej. en CI sin External/), se busca la copia del sistema (libglm-dev en
# Ubuntu, vcpkg en Windows). Se expone como target INTERFACE para que lo usen
# tanto el motor como las pruebas que compilan sueltas (que no enlazan
# funshi_engine): en Windows el include de vcpkg no esta en el path por
# defecto y sin esto esas pruebas no compilan.
set(GLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/External/glm")
add_library(funshi_glm INTERFACE)
if(EXISTS "${GLM_INCLUDE_DIR}/glm/glm.hpp")
    target_include_directories(funshi_glm INTERFACE ${GLM_INCLUDE_DIR})
else()
    find_package(glm REQUIRED)
    target_link_libraries(funshi_glm INTERFACE glm::glm)
endif()
target_link_libraries(funshi_engine PUBLIC funshi_glm)

if(NOT BUILD_RUNTIME)
# Comandos (sistema undo/redo): se enlaza con funshi_engine que provee todas las
# dependencias del motor (ImGui, componentes, EditorController, SceneRegistry).
# Solo se compila el archivo de test; las fuentes del motor vienen del engine.
add_executable(comandos-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/ComandosTests.cpp
)
target_include_directories(comandos-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
    ${CMAKE_SOURCE_DIR}/External/imgui
)
target_link_libraries(comandos-tests PRIVATE funshi_engine)
set_target_properties(comandos-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME comandos-tests COMMAND comandos-tests)

if(WIN32)
    target_link_libraries(comandos-tests PRIVATE dbghelp)
    target_compile_definitions(comandos-tests PRIVATE _HAS_STD_BYTE=0)
endif()

# --- Pruebas headless de serializacion de escena (round-trip y nombres, H-6) --
# Cubre lo que model-serialization-tests no alcanza: guardar -> recargar ->
# conservar nombre, id y jerarquia, y que los objetos nuevos nazcan con nombre.
# Necesita funshi_engine (SceneRegistry/SceneSerializer/EditorController), asi
# que al igual que comandos-tests solo se compila con BUILD_ENGINE=ON.
add_executable(escena-serializacion-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/SceneSerializationTests.cpp
)
target_include_directories(escena-serializacion-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
    ${CMAKE_SOURCE_DIR}/External/imgui
)
target_link_libraries(escena-serializacion-tests PRIVATE funshi_engine)
set_target_properties(escena-serializacion-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME escena-serializacion-tests COMMAND escena-serializacion-tests)
# Cubre tambien el recorrido de paneles del inspector en un frame headless: si
# ese recorrido no avanza (cuelgue al seleccionar un objeto), la prueba no
# vuelve y CTest la da por fallida por timeout en lugar de quedarse esperando.
set_tests_properties(escena-serializacion-tests PROPERTIES TIMEOUT 60)

# --- Pruebas headless del catalogo de componentes ---------------------------
# La tabla del catalogo es la fuente unica de que componentes existen, en que
# dimension aplican y que requisitos piden; la consumen ComponentFactory y el
# menu del inspector. Necesita funshi_engine (GameObject), como comandos-tests.
add_executable(catalogo-componentes-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/CatalogoComponentesTests.cpp
)
target_include_directories(catalogo-componentes-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
    ${CMAKE_SOURCE_DIR}/External/imgui
)
target_link_libraries(catalogo-componentes-tests PRIVATE funshi_engine)
set_target_properties(catalogo-componentes-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME catalogo-componentes-tests COMMAND catalogo-componentes-tests)

if(WIN32)
    target_link_libraries(escena-serializacion-tests PRIVATE dbghelp)
    target_compile_definitions(escena-serializacion-tests PRIVATE _HAS_STD_BYTE=0)
    target_link_libraries(catalogo-componentes-tests PRIVATE dbghelp)
    target_compile_definitions(catalogo-componentes-tests PRIVATE _HAS_STD_BYTE=0)
endif()

# --- Pruebas headless de las interfaces de juego (HUD) ---
# InterfacesJuego::recolectarDeEscena es logica pura sobre la lista de objetos
# de la escena: verifica que se recolecten TODAS las interfaces referenciadas
# (no solo la primera) y en orden. Necesita funshi_engine (GameObject), como
# catalogo-componentes-tests.
add_executable(interfaces-juego-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/InterfacesJuegoTests.cpp
)
target_include_directories(interfaces-juego-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
    ${CMAKE_SOURCE_DIR}/External/imgui
)
target_link_libraries(interfaces-juego-tests PRIVATE funshi_engine)
set_target_properties(interfaces-juego-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME interfaces-juego-tests COMMAND interfaces-juego-tests)

if(WIN32)
    target_link_libraries(interfaces-juego-tests PRIVATE dbghelp)
    target_compile_definitions(interfaces-juego-tests PRIVATE _HAS_STD_BYTE=0)
endif()

# --- Pruebas headless del tileset (JSON estilo Tiled) y del componente Tilemap
# Cubre el parseo del tileset, la geometria de la grilla, el flag de solidez por
# tile, el pintado por capas, el horneado de la malla, el round-trip binario y
# la shape compuesta del TilemapCollider (tramos horizontales de tiles solidos).
add_executable(tilemap-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/TilemapTests.cpp
)
target_include_directories(tilemap-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
)
target_link_libraries(tilemap-tests PRIVATE funshi_engine)
set_target_properties(tilemap-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME tilemap-tests COMMAND tilemap-tests)

if(WIN32)
    target_link_libraries(tilemap-tests PRIVATE dbghelp)
    target_compile_definitions(tilemap-tests PRIVATE _HAS_STD_BYTE=0)
endif()

# --- Contrato de render (headless) -------------------------------------------
# La tabla de uniformes que cada tipo de programa tiene que exponer, el backend
# nulo que cumple el contrato entero sin dibujar, y el mecanismo de instalacion
# del backend. No hay GPU: el backend de mentira solo registra las llamadas, asi
# que el resultado no depende del driver.
add_executable(contrato-render-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/ContratoRenderTests.cpp
)
target_include_directories(contrato-render-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
)
target_link_libraries(contrato-render-tests PRIVATE funshi_engine)
set_target_properties(contrato-render-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME contrato-render-tests COMMAND contrato-render-tests)

if(WIN32)
    target_link_libraries(contrato-render-tests PRIVATE dbghelp)
    target_compile_definitions(contrato-render-tests PRIVATE _HAS_STD_BYTE=0)
endif()

# --- Pruebas headless del sistema de Prefabs (round-trip y validacion) -------
# Cubre: guardar prefab -> instanciar -> conservar nombre, tag, jerarquia,
# componentes, Transform; PrefabLibrary cache y validacion de archivos corruptos.
add_executable(prefab-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/PrefabTests.cpp
)
target_include_directories(prefab-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
)
target_link_libraries(prefab-tests PRIVATE funshi_engine)
set_target_properties(prefab-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME prefab-tests COMMAND prefab-tests)

# Nucleo del manifiesto de assets de la escena (SceneAssets.json): headless,
# sin GameObjects (como MenuModel). Cubre JSON round-trip, tolerancia a
# archivos corruptos/inexistentes, relativizacion/absolutizacion contra la
# raiz de assets y la regla de precedencia del manifiesto sobre el .db.
add_executable(manifiesto-assets-tests
    ${SRC_DIR}/Configuracion/ProjectPaths.cpp
    ${SRC_DIR}/Configuracion/ProjectManager.cpp
    ${SRC_DIR}/Configuracion/ConfigPersistence.cpp
    ${SRC_DIR}/Scenes/ManifiestoAssetsCore.cpp
    ${SRC_DIR}/Configuracion/EditorConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/ManifiestoAssetsTests.cpp
)
target_link_libraries(manifiesto-assets-tests PRIVATE funshi_diagnostics_core)
target_include_directories(manifiesto-assets-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
)
set_target_properties(manifiesto-assets-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME manifiesto-assets-tests COMMAND manifiesto-assets-tests)

# Orquestador de estados de GUI (la "funcion de marco" de F5/F6/F7 y las teclas)
# tambien es pura logica de estados y se prueba headless: cubre las reglas por
# estado de Play/Pausa/Stop, Escape -> menu e "Iniciar Estudio" -> editor.
add_executable(orquestador-estado-tests
    ${SRC_DIR}/States/ApplicationStateMachine.cpp
    ${SRC_DIR}/States/OrquestadorEstadoGUI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/OrquestadorEstadoTests.cpp
)
set_target_properties(orquestador-estado-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME orquestador-estado-tests COMMAND orquestador-estado-tests)

# TemaEditor (aplicacion del perfil de apariencia al estilo de ImGui) se prueba
# headless como los anteriores: el estilo solo necesita el contexto de ImGui,
# sin backends ni pila grafica. Cubre la regresion "el color de acento no llega
# a toda la interfaz": verifica que con un acento no azul NINGUN rol de la
# paleta conserve el azul de fabrica de Dear ImGui (FrameBg, Tab, Border,
# TableHeaderBg, TextLink, DragDropTarget...), que con el acento por defecto no
# cambie el aspecto historico, y que el modo B/N siga dejando todo monocromo.
add_executable(tema-tests
    ${SRC_DIR}/ImGui/imgui.cpp
    ${SRC_DIR}/ImGui/imgui_draw.cpp
    ${SRC_DIR}/ImGui/imgui_tables.cpp
    ${SRC_DIR}/ImGui/imgui_widgets.cpp
    ${SRC_DIR}/GUI/Tema/TemaEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/TemaEditorTests.cpp
)
target_include_directories(tema-tests PRIVATE ${SRC_DIR} ${SRC_DIR}/ImGui)
set_target_properties(tema-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME tema-tests COMMAND tema-tests)

# --- Pruebas del InterfaceComponent (serializacion binaria) ------------------
# El componente guarda la ruta del .html de la interfaz mas su colocacion; el
# formato nuevo lleva una marca para distinguirse del payload viejo (solo el
# nombre). La prueba cubre el round-trip y que el lector del formato viejo no
# consuma bytes de mas (lo que desalinearia los componentes siguientes).
# Compila InterfaceComponent.cpp suelto (sin funshi_engine), pero el componente
# usa glm (headers de matematicas), que no esta disponible en un build sin
# dependencias externas: por eso vive en el bloque del motor y no en el de las
# pruebas headless.
add_executable(interface-component-tests
    ${SRC_DIR}/Objetos/Componentes/InterfaceComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/InterfaceComponentTests.cpp
)
target_link_libraries(interface-component-tests PRIVATE
    funshi_diagnostics_core funshi_glm)
target_include_directories(interface-component-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
)
set_target_properties(interface-component-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME interface-component-tests COMMAND interface-component-tests)

if(WIN32)
    target_link_libraries(manifiesto-assets-tests PRIVATE dbghelp)
    target_link_libraries(orquestador-estado-tests PRIVATE dbghelp)
    target_link_libraries(tema-tests PRIVATE dbghelp)
    target_compile_definitions(manifiesto-assets-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(orquestador-estado-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(tema-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(interface-component-tests PRIVATE _HAS_STD_BYTE=0)
endif()

endif() # NOT BUILD_RUNTIME (sin pruebas del bloque editor)

endif()



if(NOT BUILD_RUNTIME)
# --- Pruebas headless del subsistema FileManager (sin pila gráfica) ---------
# Compilan en cualquier plataforma (solo std C++17 + los headers de ImGui para
# TreeGUI.h) y ejercitan GestorDeArchivos/FileManager/FileSystemWatcher contra
# un proyecto temporal, sin ventanas. Registradas en ctest.
enable_testing()
add_executable(diagnostic-registry-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/DiagnosticRegistryTests.cpp
)
target_link_libraries(diagnostic-registry-tests PRIVATE funshi_diagnostics_core)
target_include_directories(diagnostic-registry-tests PRIVATE ${SRC_DIR})
set_target_properties(diagnostic-registry-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME diagnostic-registry-tests COMMAND diagnostic-registry-tests)

# --- Pruebas headless del formato de la barra de progreso en texto ---
# BarraProgresoTexto es header-only y puro (solo std): acotado de `hecha` a
# `[0, total]` y caso `total == 0`, sin enlazar funshi_engine.
add_executable(barra-progreso-tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/BarraProgresoTextoTests.cpp
)
target_include_directories(barra-progreso-tests PRIVATE ${SRC_DIR})
set_target_properties(barra-progreso-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME barra-progreso-tests COMMAND barra-progreso-tests)

if(WIN32)
    target_compile_definitions(barra-progreso-tests PRIVATE _HAS_STD_BYTE=0)
endif()

add_executable(terminal-emulator-tests
    ${SRC_DIR}/Terminal/TerminalEmulator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/TerminalEmulatorTests.cpp
)
target_include_directories(terminal-emulator-tests PRIVATE ${SRC_DIR})
set_target_properties(terminal-emulator-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME terminal-emulator-tests COMMAND terminal-emulator-tests)

add_executable(filemanager-tests
    ${SRC_DIR}/GestorDeArchivos/GestorDeArchivos.cpp
    ${SRC_DIR}/GestorDeArchivos/File.cpp
    ${SRC_DIR}/GestorDeArchivos/Carpeta.cpp
    ${SRC_DIR}/FileManager/FileManager.cpp
    ${SRC_DIR}/FileManager/FileSystemWatcher.cpp
    ${SRC_DIR}/Events/EditorEventBus.cpp
    ${SRC_DIR}/ExcepcionesCPP/RuntimeException.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/FileManagerTests.cpp
)
target_include_directories(filemanager-tests PRIVATE ${SRC_DIR}/ImGui)
set_target_properties(filemanager-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME filemanager-tests COMMAND filemanager-tests)

# Runner de procesos sin shell (Proceso, H-3 nivel 2): round-trip de argv con
# argumentos hostiles (espacios, &, comilla, barra final, vacio) relanzando
# este mismo binario copiado a un directorio con espacios, exit codes,
# truncado del log, cwd y entorno extra. En Windows ademas la receta cruda de
# cmd.exe que usa el harvest de vcvars. Se compila Proceso.cpp directamente
# (no hace falta funshi_engine: el modulo es std + API de SO).
add_executable(proceso-tests
    ${SRC_DIR}/FileManager/Proceso.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/ProcesoTests.cpp
)
target_include_directories(proceso-tests PRIVATE ${SRC_DIR})
set_target_properties(proceso-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME proceso-tests COMMAND proceso-tests)

# EditorConfig (JSON tolerante) tambien se prueba headless y en las tres
# plataformas: verifica round-trip y la carga ante ausencia/corrupcion.
add_executable(configuracion-tests
    ${SRC_DIR}/Configuracion/ProjectPaths.cpp
    ${SRC_DIR}/Configuracion/ProjectManager.cpp
    ${SRC_DIR}/Configuracion/ConfigPersistence.cpp
    ${SRC_DIR}/Configuracion/EditorConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/EditorConfigTests.cpp
)
target_link_libraries(configuracion-tests PRIVATE funshi_diagnostics_core)
target_include_directories(configuracion-tests PRIVATE ${CMAKE_SOURCE_DIR}/External ${SRC_DIR})
set_target_properties(configuracion-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME configuracion-tests COMMAND configuracion-tests)

# EditorEventBus (canal tipado de GUI interna, Fase 2) se prueba headless como
# los anteriores: subscribe/publish/unsubscribe con payload declarativo, sin
# pila grafica.
add_executable(eventbus-tests
    ${SRC_DIR}/Events/EditorEventBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/EditorEventBusTests.cpp
)
target_include_directories(eventbus-tests PRIVATE ${SRC_DIR})
set_target_properties(eventbus-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME eventbus-tests COMMAND eventbus-tests)

# Model (componente de malla) serializa el path con prefijo de longitud. Se
# prueba headless el round-trip y, sobre todo, que un path mas largo que el
# buffer de lectura NO desalinee el stream: regresion del core al cargar
# escenas con paths de mas de 99 bytes (std::length_error -> abort).
add_executable(model-serialization-tests
    ${SRC_DIR}/Configuracion/ProjectPaths.cpp
    ${SRC_DIR}/Configuracion/ProjectManager.cpp
    ${SRC_DIR}/Configuracion/ConfigPersistence.cpp
    ${SRC_DIR}/Objetos/Componentes/Model.cpp
    ${SRC_DIR}/Configuracion/EditorConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/ModelSerializationTests.cpp
)
target_link_libraries(model-serialization-tests PRIVATE funshi_diagnostics_core)
target_include_directories(model-serialization-tests PRIVATE
    ${SRC_DIR}
    ${CMAKE_SOURCE_DIR}/External
)
set_target_properties(model-serialization-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME model-serialization-tests COMMAND model-serialization-tests)

# MenuModel (logica pura del menu: traduccion, observer de cambios y reset) se
# prueba headless como los demas: no depende de ImGui/GLFW.
add_executable(menu-tests
    ${SRC_DIR}/GUI/MenusGUI/MenuModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/MenuModelTests.cpp
)
target_include_directories(menu-tests PRIVATE ${SRC_DIR})
set_target_properties(menu-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME menu-tests COMMAND menu-tests)

# AssetManager (cache Flyweight de meshes CPU) tambien headless y portatil:
# rutas (AssetPath), geometria (Mesh) y el registro compartido con un loader
# artificial, sin Assimp ni pila grafica.
add_executable(assetmanager-tests
    ${SRC_DIR}/Assets/AssetManager.cpp
    ${SRC_DIR}/Assets/Mesh.cpp
    ${SRC_DIR}/Matematicas/StructVec3.cpp
    ${SRC_DIR}/ExcepcionesCPP/RuntimeException.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/AssetManagerTests.cpp
)
target_include_directories(assetmanager-tests PRIVATE ${SRC_DIR})
set_target_properties(assetmanager-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME assetmanager-tests COMMAND assetmanager-tests)

# TextureManager (cache Flyweight de imagenes CPU) headless y portatil al
# igual que assetmanager-tests: registro compartido + loader artificial, sin
# stb_image (StbImageLoader es del engine, aqui no entra la pila grafica).
add_executable(texturemanager-tests
    ${SRC_DIR}/Assets/TextureManager.cpp
    ${SRC_DIR}/ExcepcionesCPP/RuntimeException.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/TextureManagerTests.cpp
)
target_include_directories(texturemanager-tests PRIVATE ${SRC_DIR})
set_target_properties(texturemanager-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME texturemanager-tests COMMAND texturemanager-tests)

# Estructuras de datos (headless y portatil): las estructuras son cabeceras
# puras, solo se necesita RuntimeException.cpp por la jerarquia de excepciones.
# Cubre las clases verificadas/corregidas (ListaDE, ArbolEnlazado) y las
# implementadas para el motor (Heaps, ListMergeSort, ArbolBinarioEnlazado).
add_executable(estructuras-tests
    ${SRC_DIR}/ExcepcionesCPP/RuntimeException.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/EstructurasTests.cpp
)
set_target_properties(estructuras-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME estructuras-tests COMMAND estructuras-tests)

# Geometria y logica de color del pipeline moderno (headless y portatil): solo la
# parte CPU (LineBuilder, la expansion de cada segmento al quad que dibuja el
# shader de lineas; el calculo de la recta de la guia de eje; el difuminado del
# piso; los colores efectivos del cielo degradado y la clave de identidad del
# cubemap del Skybox). No entra la pila grafica ni glm a proposito, asi el layout del VAO,
# la cuenta de vertices y los extremos de la guia se pueden verificar sin pila
# grafica.
add_executable(rendering-tests
    ${SRC_DIR}/Rendering/LineBuilder.cpp
    ${SRC_DIR}/Rendering/GuiaEje.cpp
    ${SRC_DIR}/Rendering/Difuminado.cpp
    ${SRC_DIR}/Matematicas/StructVec3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/RenderingTests.cpp
)
set_target_properties(rendering-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME rendering-tests COMMAND rendering-tests)

# Reflexion de comportamiento (motor de SerializeField): headless y portatil.
# Cubre las macros REFLECT_*, la lectura/escritura de campos (incl. grupos
# anidados y vectores de grupos) y el round-trip binario del arbol de valores.
add_executable(scripts-tests
    ${SRC_DIR}/Behaviour/Reflection/BehaviourReflection.cpp
    ${SRC_DIR}/FileManager/Proceso.cpp
    ${SRC_DIR}/Input/InputScripts.cpp
    ${SRC_DIR}/Matematicas/StructVec3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/ScriptsTests.cpp
)
# El contrato de flags con el que se compilan los scripts C++ se verifica en
# esta suite (headless y en las tres plataformas): el test necesita el mismo
# compilador y runtime horneados que el engine, por eso recibe las definiciones.
target_compile_definitions(scripts-tests PRIVATE
    FUNSHI_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
    FUNSHI_CXX_RUNTIME_FLAG="${FUNSHI_CXX_RUNTIME_FLAG}")
set_target_properties(scripts-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
add_test(NAME scripts-tests COMMAND scripts-tests)

# Runtime de scripts: valida de punta a punta el BackendCpp (compila el fuente
# de un script a .so, lo carga con dlopen y ejecuta el ciclo). Requiere un
# compilador C++ en PATH; si no hay, el test sale con SKIP (77).
# El skip es por FAMILIA de toolchain, no por SO: con MSVC hace falta el
# entorno de Visual Studio (vcvars), pero con GCC/MinGW el test corre en
# Windows igual que en Linux (H-14).
add_executable(scripts-runtime-tests
    ${SRC_DIR}/Behaviour/Reflection/BehaviourReflection.cpp
    ${SRC_DIR}/Behaviour/Backends/BackendCpp.cpp
    ${SRC_DIR}/Behaviour/ScriptRuntime.cpp
    ${SRC_DIR}/Configuracion/ProjectPaths.cpp
    ${SRC_DIR}/FileManager/Proceso.cpp
    ${SRC_DIR}/Matematicas/StructVec3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/ScriptsRuntimeTests.cpp
)
set_target_properties(scripts-runtime-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
# The SRC_DIR/CXX_COMPILER macros normally come from the engine target; expose
# them here too para que el BackendCpp pueda compilar el script de prueba.
target_include_directories(scripts-runtime-tests PRIVATE ${SRC_DIR})
target_link_libraries(scripts-runtime-tests PRIVATE funshi_diagnostics_core)
target_compile_definitions(scripts-runtime-tests
    PRIVATE FUNSHI_SRC_DIR="${SRC_DIR}"
            FUNSHI_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
            FUNSHI_CXX_RUNTIME_FLAG="${FUNSHI_CXX_RUNTIME_FLAG}")
# dlopen/dlsym on Linux/macOS requiere -ldl (en glibc moderno esta en libc,
# pero -ldl no molesta); sin backend C fallaria con dlopen.
if(UNIX)
    target_link_libraries(scripts-runtime-tests PRIVATE ${CMAKE_DL_LIBS})
endif()
find_package(Threads QUIET)
if(Threads_FOUND)
    target_link_libraries(scripts-runtime-tests PRIVATE Threads::Threads)
endif()
add_test(NAME scripts-runtime-tests COMMAND scripts-runtime-tests)
# El test devuelve 77 cuando no hay compilador C++ (p. ej. Windows sin cl.exe
# en PATH): ctest lo reporta como SKIP en lugar de fallo.
set_tests_properties(scripts-runtime-tests PROPERTIES SKIP_RETURN_CODE 77)

# AudioEngine (facade de audio, hilo de trabajo) headless y portatil: se monta
# con NullAudioBackend (sin dispositivo) y se prueba el contrato de la cola de
# comandos: registro de clips, handles, encolado no bloqueante, detencion,
# volumen y limpieza. Necesita hilos (El worker thread del motor).
add_executable(audio-tests
    ${SRC_DIR}/Audio/AudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/AudioEngineTests.cpp
)
target_include_directories(audio-tests PRIVATE ${SRC_DIR})
set_target_properties(audio-tests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
find_package(Threads QUIET)
if(Threads_FOUND)
    target_link_libraries(audio-tests PRIVATE Threads::Threads)
endif()
add_test(NAME audio-tests COMMAND audio-tests)

# Prueba end-to-end del backend Java (se compila solo con FUNSHI_JAVA=ON).
if(FUNSHI_JAVA)
    find_package(JNI REQUIRED)
    find_package(Java QUIET COMPONENTS Development)
    add_executable(scripts-java-tests
        ${SRC_DIR}/Behaviour/Reflection/BehaviourReflection.cpp
        ${SRC_DIR}/Behaviour/Backends/BackendCpp.cpp
        ${SRC_DIR}/Behaviour/Backends/BackendJava.cpp
        ${SRC_DIR}/Behaviour/ScriptRuntime.cpp
        ${SRC_DIR}/Configuracion/ProjectPaths.cpp
        ${SRC_DIR}/FileManager/Proceso.cpp
        ${SRC_DIR}/Matematicas/StructVec3.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests/ScriptsJavaTests.cpp
    )
    set_target_properties(scripts-java-tests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    target_include_directories(scripts-java-tests
        PRIVATE ${SRC_DIR} ${JNI_INCLUDE_DIRS})
    target_link_libraries(scripts-java-tests PRIVATE funshi_diagnostics_core)
    target_compile_definitions(scripts-java-tests PRIVATE FUNSHI_JAVA)
    if(Java_JAVAC_EXECUTABLE)
        target_compile_definitions(scripts-java-tests
            PRIVATE FUNSHI_JAVAC_DEFAULT="${Java_JAVAC_EXECUTABLE}")
    endif()
    if(JAVA_JVM_LIBRARY)
        target_compile_definitions(scripts-java-tests
            PRIVATE FUNSHI_LIBJVM_DEFAULT="${JAVA_JVM_LIBRARY}")
    endif()
    if(UNIX)
        target_link_libraries(scripts-java-tests PRIVATE ${CMAKE_DL_LIBS})
    endif()
    find_package(Threads QUIET)
    if(Threads_FOUND)
        target_link_libraries(scripts-java-tests PRIVATE Threads::Threads)
    endif()
    add_test(NAME scripts-java-tests COMMAND scripts-java-tests)
    set_tests_properties(scripts-java-tests PROPERTIES SKIP_RETURN_CODE 77)
endif()

if(WIN32)
    # dbghelp: CapturaBackTrace/SymFromAddr/SymInitialize (RuntimeException).
    # En MinGW los #pragma comment(lib) de MSVC se ignoran, hay que enlazarla.
    target_compile_definitions(diagnostic-registry-tests PRIVATE _HAS_STD_BYTE=0)
    target_link_libraries(filemanager-tests PRIVATE dbghelp)
    target_link_libraries(assetmanager-tests PRIVATE dbghelp)
    target_link_libraries(texturemanager-tests PRIVATE dbghelp)
    target_link_libraries(estructuras-tests PRIVATE dbghelp)
    target_link_libraries(configuracion-tests PRIVATE dbghelp)
    target_link_libraries(eventbus-tests PRIVATE dbghelp)
    target_link_libraries(model-serialization-tests PRIVATE dbghelp)
    target_link_libraries(menu-tests PRIVATE dbghelp)
    target_link_libraries(scripts-tests PRIVATE dbghelp)
    target_link_libraries(scripts-runtime-tests PRIVATE dbghelp)
    target_link_libraries(audio-tests PRIVATE dbghelp)
    # Evitar conflicto std::byte (C++17) vs typedef byte en headers Windows (rpcndr.h, etc.)
    target_compile_definitions(filemanager-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(assetmanager-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(texturemanager-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(estructuras-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(configuracion-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(eventbus-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(model-serialization-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(menu-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(scripts-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(scripts-runtime-tests PRIVATE _HAS_STD_BYTE=0)
    target_compile_definitions(audio-tests PRIVATE _HAS_STD_BYTE=0)
endif()

endif() # NOT BUILD_RUNTIME (sin pruebas headless en el build runtime)

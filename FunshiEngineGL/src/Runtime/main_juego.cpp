/*
    FunshiEngineGL - Motor de juegos 3D con API grafica intercambiable e ImGui
    Copyright 2026 Gianfranco Ivan Enrique

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

    SPDX-License-Identifier: Apache-2.0
*/

// Punto de entrada DEDICADO para el JUEGO EXPORTADO.
// NO incluye codigo del editor (GUIManager, MenuGUI, paneles, atajos de editor).
// Solo lo que corre en modo "Play": escena, fisica, scripts, render, input de juego.

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define _HAS_STD_BYTE 0
#endif

#include "../Scenes/GameScene.h"
#include "../Configuracion/EditorConfig.h"
#include "../Configuracion/ProjectPaths.h"
#include "../Configuracion/RutasLog.h"
#include "../Exportador/ConfigJuegoExportado.h"
#include "../EngineTime.h"
#include "../Ventana.h"
#include "../Input/EditorInput.h"
#include "../Rendering/Backend/IRenderBackend.h"
#include "../Behaviour/ScriptRuntime.h"
#include "../States/ApplicationStateMachine.h"
#include "../States/OrquestadorEstadoGUI.h"
#ifdef FUNSHI_CEF
#include "../WebUI/WebUIRuntime.h"
#endif

#include <imgui.h>
#include "ImGui/BackendImGui.h"


#include <iostream>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static FILE* g_logSalida = nullptr;
static int ventanaHeightEjeY, ventanaWidthEjeX;

// Copiado de main.cpp: redirige stdout/stderr a log con timestamp UTC
static std::string redirigirSalidaALog(char* argv0) {
    const fs::path ejecutable =
        argv0 && argv0[0] != '\0' ? fs::path(argv0).parent_path() : fs::path(".");
    std::error_code ec;
    const fs::path temporal = fs::temp_directory_path(ec);

    const std::time_t ahora = std::time(nullptr);
    const std::tm tmUtc = *std::gmtime(&ahora);
    char sufijo[32];
    std::strftime(sufijo, sizeof(sufijo), "%Y%m%d_%H%M%S", &tmUtc);
    const std::string nombre = "JuegoExportado_" + std::string(sufijo) + ".log";

    for (const std::string& carpeta :
         RutasLog::candidatas(ProjectPaths::directorioBase(),
                              ejecutable.string(), temporal.string())) {
        try {
            fs::create_directories(carpeta);
            const std::string ruta = (fs::path(carpeta) / nombre).string();
            g_logSalida = std::freopen(ruta.c_str(), "a", stdout);
            if (!g_logSalida) continue;
            std::freopen(ruta.c_str(), "a", stderr);
            std::cout << "\n========== Arranque JuegoExportado " << sufijo
                      << " ==========\n";
            std::cout << "Log en: " << ruta << "\n";
            return ruta;
        } catch (...) {
            g_logSalida = nullptr;
        }
    }
    return std::string();
}

// Windows GUI entry point (sin consola)
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
static int EjecutarJuego(int argc, char* argv[]);

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<char*> argvAnsi;
    if (wargv) {
        for (int i = 0; i < argc; ++i) {
            int largo = WideCharToMultiByte(CP_ACP, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
            if (largo > 0) {
                std::string s(largo, '\0');
                WideCharToMultiByte(CP_ACP, 0, wargv[i], -1, &s[0], largo, nullptr, nullptr);
                s.pop_back();
                argvAnsi.push_back(strdup(s.c_str()));
            }
        }
        LocalFree(wargv);
    }
    int ret = EjecutarJuego(static_cast<int>(argvAnsi.size()), argvAnsi.data());
    for (auto p : argvAnsi) free(p);
    return ret;
}
#endif

// Entry point principal (Linux y fallback Windows)
int EjecutarJuego(int argc, char* argv[]) {
#ifdef FUNSHI_CEF
    // Igual que en el editor: si CEF relanzo este binario como subproceso, se
    // ejecuta ese rol y no se arranca el juego.
    {
        const int codigoSubproceso = WebUIRuntime::ejecutarSubproceso(argc, argv);
        if (codigoSubproceso >= 0) return codigoSubproceso;
    }
#else
    (void)argc; (void)argv; // En exportacion no se usan argumentos CLI
#endif

    // 1. Redirigir salida al log
    std::string rutaLog = redirigirSalidaALog(argc > 0 ? argv[0] : nullptr);
    (void)rutaLog;

    // 2. Registrar el callback de errores de GLFW antes de crear la ventana.
    // Ventana::initVentana llama a glfwInit(); no se duplica aqui.
    glfwSetErrorCallback([](int error, const char* desc) {
        std::cerr << "[GLFW] error " << error << ": " << desc << '\n';
    });

    // 3. Crear ventana (Ventana llama a glfwInit internamente y crea la
    // ventana y sus eventos; la superficie grafica la crea el backend).
    Ventana* auxVentana = new Ventana();
    auxVentana->initVentana();
    GLFWwindow* window = auxVentana->getWindow();

    // 4. ImGui init (contexto básico, SIN GUIManager del editor)
    // En juego exportado NO se crean GUIManager, MenuGUI, TreeFilesInterface, etc.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr; // No guardar imgui.ini en juego exportado

    // La interfaz se prepara por la fachada: el motor no llama a un backend de
    // ImGui concreto porque cada API grafica tiene el suyo.
    MotorImGui::init(window, true);

    // 5. GameScene (nucleo: render, fisica, scripts, input)
    // El constructor que NO requiere GUIManager - solo para runtime
    GameScene* scene = new GameScene(nullptr);

    // 6. Leer manifiesto JuegoExportado.json junto al binario para obtener metadatos
    // El juego exportado NO replica la estructura MotorGrafico/Proyects: las escenas
    // y assets están directamente en scenes/ y assets/ junto al ejecutable.
    std::string dirEjecutable = ProjectPaths::directorioEjecutable();
    std::string nombreProyecto;
    bool modoJuego = true;
    if (!ConfigJuegoExportado::leer(ConfigJuegoExportado::rutaPorDefecto(dirEjecutable), nombreProyecto, modoJuego)) {
        std::cerr << "Advertencia: no se encontro JuegoExportado.json, usando configuracion por defecto\n";
        nombreProyecto = "JuegoExportado";
    }

    // 7. Crear directorios de trabajo si no existen (scripts y cache)
    const std::string dirEjecutableConBarra = dirEjecutable.back() == '/' || dirEjecutable.back() == '\\' 
        ? dirEjecutable : dirEjecutable + "/";
    const std::string scriptsDir = dirEjecutableConBarra + "scripts";
    const std::string cacheDir = dirEjecutableConBarra + "cache";
    
    std::error_code ec;
    fs::create_directories(scriptsDir, ec);
    fs::create_directories(cacheDir, ec);

    // 8. Cargar escena desde la estructura simplificada del juego exportado
    // Las escenas están en scenes/ junto al ejecutable, NO en MotorGrafico/Proyects.
    // Las rutas de assets de la escena se guardan RELATIVAS a la raiz del
    // juego (p. ej. "Assets/Modelos/Cube.obj"): se fija esa raiz ANTES de
    // cargar, para que se resuelvan contra el directorio del juego y no
    // contra el cwd desde donde se lanzo (que dejaria las mallas sin
    // encontrar y la pantalla en negro).
    EditorConfig::fijarRaizAssets(dirEjecutable);
    const std::string sceneBBDD = dirEjecutableConBarra + "scenes/Scene.db";
    const std::string sceneDir = dirEjecutableConBarra + "scenes/SceneDir";
    
    if (fs::exists(sceneBBDD)) {
        scene->loadScene(sceneBBDD, sceneDir);
        std::cout << "[juego] Escena cargada desde: " << sceneBBDD << '\n';
    } else {
        std::cerr << "[juego] ADVERTENCIA: No se encontro Scene.db en: " << sceneBBDD << '\n';
        std::cerr << "[juego] Iniciando con escena vacia (el juego seguira ejecutandose)\n";
        // No cargar escena - GameScene arranca con una escena vacía por defecto
    }

    // configurarProyecto() inicializa el catalogo de clips de audio
    // (Sonidos/) y las interfaces de la escena (Memory/Interfaces).
    // Debe llamarse DESPUES de loadScene para que la escena ya exista.
    scene->configurarProyecto(nombreProyecto);

    // 9. Maquina de estados y orquestador
    ApplicationStateMachine appStateMachine;
    OrquestadorEstadoGUI orquestadorDeGUI(&appStateMachine);

    // 10. Entrar DIRECTAMENTE en modo Playing (Juego)
    appStateMachine.transitionTo(ApplicationState::Playing);
    scene->setModoJuego(true);

    // 11. Input para juego (EditorInput maneja input de juego SIN GUI de editor)
    EditorInput* input = new EditorInput(scene, &appStateMachine, &orquestadorDeGUI);

    // 12. Registrar callbacks de input (antes del loop)
    input->registrarCallbacks(window);
    input->aplicarModoCursor(window);

    // 13. Bucle principal del juego
    Time::start();

#ifdef FUNSHI_CEF
    // Motor web: mismo ciclo que el editor (init una vez, pump por frame,
    // apagado al salir). Sin runtime CEF cae al nulo y el juego sigue.
    if (!WebUIRuntime::inicializar(argc, argv)) {
        std::cout << "[webui] Sin backend web real en el juego exportado."
                  << std::endl;
    }
#endif

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Apertura del frame: el backend prepara lo que necesita para dibujar
        // en el (en una API con swapchain, la imagen siguiente).
        Rendering::Backend::activeBackend().beginFrame();
#ifdef FUNSHI_CEF
        WebUIRuntime::bombear();
#endif
        Time::update();

        MotorImGui::newFrame();
        ImGui::NewFrame();

        // El juego exportado siempre corre en modo Playing: setStart(true)
        // habilita el avance de la fisica y los scripts cada frame. En
        // main.cpp este valor lo refleja el orquestador de estados; aqui se
        // fija directamente porque el estado nunca cambia a Editing.
        scene->setStart(true);
        scene->setModoJuego(true);

        // update() avanza la fisica, los scripts y las animaciones.
        // gameScene() dibuja el frame con el estado ya actualizado.
        scene->update(Time::getDeltaTime());
        scene->gameScene();

        ImGui::Render();
        MotorImGui::render(ImGui::GetDrawData());

        // Cierre del frame: el backend envia lo dibujado y muestra el
        // resultado. El intercambio de buffers es un detalle de la API
        // grafica, asi que lo hace el backend y no el motor.
        Rendering::Backend::activeBackend().endFrame();
    }

    // 14. Cleanup ordenado (igual que main.cpp, sin GUIManager)
    scene->cerrarSimulacionAntesDeGuardar();
    scene->descargarScripts();
    ScriptRuntime::apagarScripts();

#ifdef FUNSHI_CEF
    WebUIRuntime::apagar();
#endif

    MotorImGui::shutdown();
    ImGui::DestroyContext();

    delete input;
    delete scene;
    delete auxVentana;
    glfwTerminate();
    return 0;
}

#ifndef _WIN32
int main(int argc, char* argv[]) { return EjecutarJuego(argc, argv); }
#endif

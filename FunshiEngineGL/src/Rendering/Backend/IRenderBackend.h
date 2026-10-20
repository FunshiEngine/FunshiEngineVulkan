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
#ifndef IRENDERBACKEND_H
#define IRENDERBACKEND_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

#include "../ContratoRender.h"

namespace Rendering {
namespace Backend {

// Handle opaco a un recurso GPU (0 = vacio/null). El engine trabaja a traves
// de la interfaz; el backend concreto traduce cada handle a su objeto nativo
// (un id de buffer, una imagen, ...). La interfaz no filtra detalles de la API.
using Handle = std::uint64_t;
constexpr Handle kInvalidHandle = 0;

// Geometria CPU a subir a GPU (misma informacion que los parametros posicionales
// de MeshGPU::upload, pero agrupada para la interfaz).
struct MeshData {
    const float* vertices = nullptr;
    std::size_t vertexCount = 0;
    const float* normals = nullptr;
    std::size_t normalCount = 0;
    const float* uvs = nullptr;
    std::size_t uvCount = 0;
    const float* tangents = nullptr;
    std::size_t tangentCount = 0;
    const float* bitangents = nullptr;
    std::size_t bitangentCount = 0;
    const unsigned int* indices = nullptr;
    std::size_t indexCount = 0;
};

// Pixels RGBA8 sin firmar de una imagen CPU (para subir texturas 2D).
struct Image2D {
    int width = 0;
    int height = 0;
    const unsigned char* pixels = nullptr;
    // Genera la cadena de mipmaps (muestreo trilinear al minificar). Si el
    // backend no puede generarlos cae a lineal (solo el nivel base).
    bool generateMipmaps = false;
};

// Funcion de comparacion de profundidad, en los terminos de la interfaz (no los
// de GL). Solo se exponen las dos que el engine usa: la base del pipeline
// (GL_LESS, la que deja applyBaseState) y la que acepta un fragmento en el
// plano lejano (GL_LEQUAL), que es como se dibuja el fondo del Skybox.
enum class DepthFunc { Less, LessEqual };
constexpr DepthFunc kDepthFuncLess = DepthFunc::Less;
constexpr DepthFunc kDepthFuncLessEqual = DepthFunc::LessEqual;

// Contrato de backend grafico. Los recursos se crean/destruyen por handle y
// las operaciones de dibujado son el pipeline moderno completo: no queda
// superficie fixed-function (ni glBegin/glEnd, ni stack de matrices, ni
// luces de estado fijo). Los consumidores de la capa de entidades nunca ven la
// API grafica: todo pasa por aca (o por sus wrappers RAII).
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    // Prepara el backend para dibujar.
    //
    // ventanaNativa es el puntero opaco de la ventana del sistema (el
    // GLFWwindow del motor). El backend lo necesita porque es quien crea la
    // superficie sobre la que dibuja: con la ventana sin contexto grafico, la
    // superficie la crea el backend y no hay otro modo de llegar a ella.
    //
    // Devuelve false si el backend no esta disponible (no hay API grafica en
    // la maquina, no se pudo crear el dispositivo, ...). Devuelve true si el
    // motor puede seguir: un backend que dibuja nada tambien puede arrancar.
    virtual bool init(void* ventanaNativa) = 0;
    virtual bool available() const = 0;

    // Limite de frame.
    //
    // beginFrame abre el frame: el backend prepara lo que necesita para
    // dibujar en el (en una API con swapchain, la imagen siguiente). endFrame
    // lo cierra: el backend envia lo dibujado y muestra el resultado.
    //
    // El motor no sabe nada de como se hace: el intercambio de buffers es un
    // detalle de la API, no del motor. Por eso el limite de frame es parte
    // del contrato y no un detalle del bucle principal.
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;

    // --- Malla GPU (VAO/VBO/EBO o su analogo nativo) -------------------------
    virtual Handle createMesh(const MeshData& data) = 0;
    virtual void destroyMesh(Handle mesh) = 0;
    virtual void drawMesh(Handle mesh, unsigned int indexCount) = 0;

    // --- Batch de lineas (buffer de la geometria expandida por LineBuilder) ---
    // Los vertices vienen en el layout de LineBuilder (12 floats: inicio, fin,
    // lado, avance, rgba) y se dibujan como triangulos (la expansion a quads
    // es lo que reemplaza al dibujado de lineas de ancho fijo). update vuelve
    // a subir el buffer del mismo recurso, que es el caso por frame de la
    // grilla y los marcadores.
    virtual Handle createLineBatch(const float* vertices,
                                   std::size_t vertexCount) = 0;
    virtual void destroyLineBatch(Handle batch) = 0;
    virtual void updateLineBatch(Handle batch, const float* vertices,
                                 std::size_t vertexCount) = 0;
    virtual void drawLineBatch(Handle batch, unsigned int vertexCount) = 0;

    // --- Textura 2D ----------------------------------------------------------
    virtual Handle createTexture2D(const Image2D& image) = 0;
    virtual void destroyTexture2D(Handle texture) = 0;
    // Bindeo del sampleo en la unidad indicada (la unidad 0 es la primera).
    virtual void bindTexture2D(Handle texture, int unit) = 0;
    // Reescribe el contenido de una textura 2D ya creada. Si las dimensiones de
    // la imagen coinciden con las de la textura, sube solo la region
    // modificada sin reasignar el almacenamiento; si cambian, el backend
    // reasigna. Pensado para fuentes que entregan un bitmap nuevo por frame
    // (p. ej. la vista web offscreen), evitando crear/destruir la textura cada
    // frame.
    virtual void updateTexture2D(Handle texture, const Image2D& image) = 0;

    // --- Textura Cubemap -----------------------------------------------------
    // 6 imagenes, una por cara (+X, -X, +Y, -Y, +Z, -Z). Todas deben tener
    // las mismas dimensiones y formato RGBA8.
    struct ImageCube {
        int width = 0;
        int height = 0;
        // En orden: +X, -X, +Y, -Y, +Z, -Z
        const unsigned char* faces[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
        bool generateMipmaps = false;
    };
    virtual Handle createTextureCube(const ImageCube& image) = 0;
    virtual void destroyTextureCube(Handle texture) = 0;
    virtual void bindTextureCube(Handle texture, int unit) = 0;

    // --- Programa de shaders (pipeline moderno) ------------------------------
    // El motor pide un programa por tipo (Contrato::ShaderKind) y el backend lo
    // construye con su propio dialecto: el fuente GLSL (o SPIR-V, o el que
    // corresponda) vive en el backend, no en el motor, porque cada API tiene
    // reglas distintas de version, de empaquetado de uniformes y de samplers.
    // createProgram puede lanzar (falla de compilacion/limite): la excepcion
    // es especifica del backend (ShaderCompile/Link/Unavailable) pero el
    // contrato no impone un tipo.
    virtual Handle createProgram(Contrato::ShaderKind kind) = 0;
    virtual void destroyProgram(Handle program) = 0;
    virtual void useProgram(Handle program) = 0;
    // Localizador de un uniforme por nombre. El valor es un token opaco del
    // backend (en una API con uniforms por bloque puede ser un offset, no un
    // indice de location): el motor solo lo reenvia a los setUniform*. -1
    // significa que el programa no expone ese uniforme.
    virtual int uniformLocation(Handle program, const char* name) = 0;
    virtual void setUniformMat4(int location, const glm::mat4& value) = 0;
    virtual void setUniformMat3(int location, const glm::mat3& value) = 0;
    virtual void setUniformVec3(int location, const glm::vec3& value) = 0;
    virtual void setUniformVec4(int location, const float* value) = 0;
    virtual void setUniformFloat(int location, float value) = 0;
    virtual void setUniformInt(int location, int value) = 0;

    // --- Render target (textura con destino de render para viewports) --------
    virtual Handle createRenderTarget(int width, int height) = 0;
    virtual void destroyRenderTarget(Handle target) = 0;
    virtual void bindRenderTarget(Handle target) = 0;
    virtual void bindDefaultFramebuffer() = 0;
    // Handle de la textura de color del target (para mostrarla como ImTextureID).
    virtual Handle renderTargetColorTexture(Handle target) const = 0;
    // Handle como descriptor opaco para la GUI (ImGui::Image): el backend
    // devuelve el identificador que su API necesita para que la GUI pueda
    // dibujar esa textura. La GUI nunca ve el valor concreto, solo lo reenvia
    // como ImTextureID.
    virtual void* imguiTextureId(Handle texture) const = 0;

    // --- Estado de la pasada y del framebuffer -------------------------------
    // Viewport del framebuffer actual: lo necesitan la pasada principal y las
    // vistas previas (cada una con su propio encuadre).
    virtual void setViewport(int x, int y, int width, int height) = 0;
    // Limpia el framebuffer actual con el fondo dado (color + profundidad).
    virtual void clearScreen(const float color[3]) = 0;
    // Estado base de un contexto recien creado: test de profundidad y
    // multisample. Se llama una vez al arrancar, despues de crear el contexto y
    // antes del bucle.
    virtual void applyBaseState() = 0;
    // Color de limpieza del framebuffer. clearScreen(null) limpia con este
    // color sin tocarlo.
    virtual void setClearColor(const float color[3]) = 0;
    // Info del GPU/contexto (API, version, driver) como texto para los logs de
    // arranque de la ventana.
    virtual const char* diagnosticoGPU() const = 0;
    // Blend del framebuffer. Lo necesitan las lineas con alpha por vertice del
    // pipeline moderno (el difuminado de la grilla se funde con el fondo). No
    // se confunde con el suavizado de lineas: el ancho de linea no se toma de
    // un parametro de estado, lo calcula el shader.
    virtual void setBlendEnabled(bool enabled) = 0;

    // --- Estado de profundidad (para el cielo: depth test on + depth mask off)
    virtual void setDepthTestEnabled(bool enabled) = 0;
    virtual void setDepthMask(bool enabled) = 0;
    // Funcion de comparacion de profundidad. Vive en el backend porque es
    // estado puro de la API. La necesita la pasada del cubemap del Skybox, que
    // dibuja en el plano lejano (NDC z = 1.0, el mismo valor con el que se
    // limpia el z-buffer): con la comparacion por defecto (Less) ese empate se
    // descarta y el fondo no se dibuja. Se usan las constantes de abajo para no
    // filtrar valores de la API a la capa de escena.
    virtual void setDepthFunc(DepthFunc func) = 0;

    // --- Pasada a pantalla completa (sin atributos) ---------------------------
    // Dibuja un triangulo que cubre el viewport con el programa YA en uso. El
    // vertex shader lo arma con el indice de vertice y no lee atributos, asi
    // que no lleva geometria ni buffer. Va por el backend porque cada API
    // exige su propio estado ligado para poder dibujar.
    virtual void drawFullscreenTriangle() = 0;
};

// Backend activo del engine.
//
// Por defecto es el backend nulo: el motor define el contrato de render pero
// no implementa ninguna API grafica, que es justamente la parte que este
// repositorio deja por implementar. Un backend de verdad se instala con
// instalarBackend() antes de arrancar el bucle.
IRenderBackend& activeBackend();

// Reemplaza el backend activo. La propiedad pasa al motor, que lo destruye al
// ser reemplazado o al salir. nullptr restaura el backend nulo.
//
// Permite que otro repositorio (o una prueba headless con un backend de
// mentira) inyecte su implementacion del contrato.
void instalarBackend(std::unique_ptr<IRenderBackend> backend);

} // namespace Backend
} // namespace Rendering

#endif

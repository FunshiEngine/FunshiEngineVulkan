# FunshiEngineGL

Motor y editor de juegos en tiempo real escrito en C++17, con interfaz Dear ImGui y arquitectura modular (backends intercambiables).

Este repositorio implementa el **backend de Vulkan** del contrato de render: el motor (que viene del core `FunshiEngineCore`) define qué recursos, estados y uniformes necesita, y acá está la implementación que los resuelve sobre Vulkan. El motor no habla el dialecto de ninguna API: pide un programa por tipo y el backend lo construye con el suyo.

**Licencia:** [Apache License 2.0](LICENSE).

## Compilación

```bash
cmake -S FunshiEngineGL -B FunshiEngineGL/build \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=OFF -DFUNSHI_JAVA=ON
cmake --build FunshiEngineGL/build -j$(nproc)
```

### Requisitos

- **La librería de Vulkan** (el *loader*): en Windows viene con el driver de
  la GPU; en Linux, con `libvulkan1` (y `libvulkan-dev` para compilar).
- **Un compilador de shaders a SPIR-V**: `glslc` (del Vulkan SDK o de
  `shaderc`) o `glslangValidator` (`glslang-tools`). Los shaders del motor se
  compilan a SPIR-V **en tiempo de build** y quedan embebidos en el
  ejecutable: en runtime no hace falta el compilador ni leer archivos.

El backend se puede dejar de compilar con `-DFUNSHI_VULKAN=OFF` (el motor
arranca entonces sin dibujar).

### Opciones de CMake

| Opción | Por defecto | Qué hace |
|---|---|---|
| `FUNSHI_VULKAN` | `ON` | Compila el backend de Vulkan (y el de ImGui sobre Vulkan). |
| `FUNSHI_JAVA` | `ON` | Compila el bindings de scripts en Java. |
| `ENABLE_ASAN` | `ON` | Activa AddressSanitizer y UndefinedBehaviorSanitizer (solo con compiladores GNU/Clang; en Debug, que es el tipo de build por defecto, no linkea con el GCC de MSYS2: compila en `Release` con `-DENABLE_ASAN=OFF`). |

## Ejecución

```bash
cd FunshiEngineGL && ./build/FunshiEngineGL
```

### Variable de entorno

`FUNSHI_VULKAN_GPU=<índice>` fuerza una GPU concreta de la lista que reporta
Vulkan (0 es la primera). Es una ayuda para las máquinas con varias GPUs o
con un driver que no funciona: sin ella se usa la primera que tenga cola
gráfica y pueda presentar en la pantalla.

## Limitaciones conocidas

- **La librería de Vulkan se enlaza**, no se carga en tiempo de ejecución: el
  proceso necesita la librería instalada para arrancar.
- **La interfaz no tiene suavizado**: la escena se dibuja con multisample de
  4 muestras y se resuelve contra la imagen del swapchain, pero la interfaz
  se dibuja después, sobre la imagen ya resuelta, en una pasada de una sola
  muestra (su pipeline no lleva estado de profundidad y no puede usarse en la
  pasada de la escena).
- El backend arranca el motor sin contexto gráfico (`GLFW_NO_API`): la
  superficie y el swapchain los crea el backend.

## Documentación

- [MANUAL_DE_USO.md](MANUAL_DE_USO.md) – Guía de uso del editor y scripting
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) – Arquitectura y estructura del proyecto
- [FLUJO_DE_RAMAS.md](FLUJO_DE_RAMAS.md) – Convención de ramas
- [DocuTecnicoBugs.md](DocuTecnicoBugs.md) – Registro técnico de bugs
- [AGENTS.md](AGENTS.md) – Convenciones para agentes de código

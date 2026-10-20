# Estructura y relaciones del proyecto

## 1. Descripción general

`FunshiEngineGL` es un editor/motor gráfico 3D en C++17. El ejecutable combina:

- Ventana y eventos mediante GLFW, sin contexto gráfico: la superficie la crea
  el backend de render que se instale.
- Renderizado con un único pipeline moderno, resuelto por un backend
  intercambiable: los modelos usan `MeshRenderer` (malla en GPU + shaders) y
  las líneas un batch en GPU con shader de ancho en píxeles. No queda modo
  inmediato (`glBegin/glEnd`), ni matrices de compatibilidad, ni materiales ni
  luces de estado fijo. La grilla es un componente (`Grid`) en una pasada
  independiente que ya no usa display lists; los marcadores de luz/cámara y los
  gizmos de los colliders se dibujan con el mismo pipeline de líneas. Los tres
  ejes (X rojo, Y verde, Z azul) tienen un solo color de base en toda la
  escena —el de la guía de eje, que usa el gizmo como referencia— y la grilla y
  la guía lo ajustan por contraste contra el color que tienen debajo.
- Interfaz de editor con Dear ImGui y gizmos con ImGuizmo.
- Jerarquía de entidades basada en árboles enlazados propios.
- Simulación física mediante Bullet Physics detrás de una fachada desacoplada.
- Carga de modelos 3D mediante Assimp.
- Serialización binaria de escenas en preorden (jerarquía completa).
- EventBus para notificaciones desacopladas entre subsistemas.
- Máquina de estados explícita (`ApplicationStateMachine`).
- Explorador de archivos del proyecto con fachada propia y vigilancia de cambios.
- Configuración del editor persistida en JSON (`EditorConfig`).
- Caché compartida de assets (meshes CPU e imágenes) mediante Flyweight
  (`AssetManager` / `TextureManager`).
- Scripts dinámicos (`.so`/`.dll`) con reflexión tipo `SerializeField` y hot
  reload (`ScriptRuntime` + backends C++/Java) sobre `IScriptBehaviour`.
- Estructuras de datos genéricas propias y jerarquía de excepciones.

El punto de entrada del **editor** es `FunshiEngineGL/src/main.cpp`. El punto de
entrada de los **juegos exportados** es `FunshiEngineGL/src/Runtime/main_juego.cpp`
(sin código del editor: GUIManager, MenuGUI, paneles). La configuración de compilación
está en `FunshiEngineGL/CMakeLists.txt`, que es el único build soportado: CMake
genera la solución de Visual Studio dentro del directorio de build que elijas
(por ejemplo `FunshiEngineGL/build-win/FunshiEngineGL.sln`), así que no hay
proyectos de Visual Studio mantenidos a mano en el repositorio. Las pruebas
headless viven en `tests/` y el CI en
`.github/workflows/ci.yml`.

`funshi_diagnostics_core` es una biblioteca compartida dentro del motor; no
depende de ImGui y puede consumirla cualquier frente que hable su formato.

`RuntimeException` conserva la traza de pila y el tipo de excepción. Los límites
que deciden que un fallo es reportable lo envían a `RegistroDiagnosticos`; el
constructor de la excepción no registra automáticamente porque algunas
excepciones se usan como control interno. El registro mantiene una ventana
acotada en memoria para Estado y persiste eventos en JSON Lines para que una
invocación externa al editor pueda consultar el mismo historial. La GUI no
es propietaria del registro y el módulo de diagnósticos no depende de ImGui.
Los publicadores aceptan una instancia explícita del registro para pruebas
aisladas; las llamadas de producción usan la instancia global compartida.
`SceneSerializer::load` devuelve `false` si la reconstrucción queda parcial o
falla. Una escena nueva sin índice se crea vacía sin registrar un error; los
índices corruptos producen advertencias y los objetos que no se pueden leer,
errores con su identificador y ruta.
`Modelos3D` publica los errores tipados de `AssetManager` en el mismo registro;
la clave de ruta evita repetir el evento cuando el cache vuelve a entregar el
fallo.
`Script::cargarSiNecesario` publica cada fallo de compilación/carga como
`SCRIPT_LOAD_FAILED` y conserva el mensaje en el componente para su vista
contextual. Repetir la consulta de un intento fallido no genera otro evento;
un nuevo intento sí se registra de nuevo. El registro puede inyectarse en el
componente para pruebas aisladas.
Los backends C++ y Java contienen las excepciones de callback dentro de su
límite de ejecución, conservan el primer fallo en `ComportamientoCargado` y lo
publican como `SCRIPT_CALLBACK_FAILED`; el comportamiento queda deshabilitado
hasta la recarga.

La terminal del editor se compone de `GUI/Terminal/TerminalInterface` y
`Terminal/TerminalSession`. La primera es una ventana persistente registrada en
`GUIManager`, el menú Ventanas y el dock; la sesión lanza una shell dentro del
directorio fijo del proyecto y entrega su salida mediante un hilo lector que
drena hasta el cierre del pseudo-terminal (incluida la salida emitida al
terminar la shell, que `TerminalInterface` procesa siempre, no solo mientras la
sesión está activa). El backend Windows usa ConPTY y el POSIX usa `forkpty` con
el maestro en modo no bloqueante; la sesión no reutiliza `Proceso`, que es
síncrono y no expone una terminal interactiva.
`TerminalEmulator` mantiene la cuadrícula VT, cursor, atributos ANSI comunes,
pantalla alternativa y respuestas a consultas de cursor; `TerminalInterface`
traduce celdas y teclas a ImGui. La entrada UTF-8 se envía al pseudo-terminal.
La emulación cubre el conjunto usual de secuencias de control, sin extensiones
como color true-color/256, ratón o portapapeles.

---

## 2. Árbol de archivos relevante

Se omiten los directorios generados por el build (`build/`, `build_temp/`), los
archivos temporales de editor (`*.swp`, `*.swo`) y el contenido interno de las
dependencias de terceros.

```text
FunshiEngineGL/                          ← raíz del repo
├── README.md                            ← visión general, build, controles y pendientes
├── PROJECT_STRUCTURE.md                 ← este documento
├── MANUAL_DE_USO.md                     ← manual de usuario (editor + scripting C++/Java)
├── CAMARAS_VISTAS_PREVIAS.md            ← Fase 2: cámaras componente + vistas previas
├── ARQUITECTURA_ESTADOS_GUI.md          ← estados/menú/GUI internas (diseño + Fases 1-3)
├── MANUAL_DE_USO.md                     ← manual de usuario (editor + scripting C++/Java)
├── FLUJO_DE_RAMAS.md                    ← convención de ramas (develop/test/staging/release)
├── .github/workflows/ci.yml             ← CI: engine en Ubuntu + pruebas en Linux/Win/macOS
├── .github/workflows/release.yml        ← instaladores Qt IFW (.run) e Inno (.exe) por tag
├── .github/workflows/windows-release.yml← build+release específico de Windows
├── .gitignore / .gitattributes
├── tests/
│   ├── FileManagerTests.cpp             ← headless del explorador de archivos
│   ├── EditorConfigTests.cpp            ← headless de la configuración JSON
│   ├── EditorEventBusTests.cpp          ← headless del canal tipado de GUI (Fase 2)
│   ├── MenuModelTests.cpp               ← headless del modelo del menú (Fase 3)
│   ├── AssetManagerTests.cpp            ← caché Flyweight de meshes (AssetPath/Mesh)
│   ├── TextureManagerTests.cpp          ← caché Flyweight de imágenes (TextureManager)
│   ├── EstructurasTests.cpp             ← listas, árboles, heaps y ordenamiento propios
│   ├── ScriptsTests.cpp                 ← reflexión SerializeField + round-trip binario
│   ├── ScriptsRuntimeTests.cpp          ← BackendCpp end-to-end (compila y dlopen un .so)
│   ├── ScriptsJavaTests.cpp             ← BackendJava end-to-end (solo con FUNSHI_JAVA)
│   ├── AudioEngineTests.cpp             ← AudioEngine/AudioClipsManager con NullAudioBackend
│   ├── SceneSerializationTests.cpp      ← round-trip de escena + nombres por defecto
│   └── UserInterfaceTests.cpp           ← modelo del Creador de interfaces (round-trip JSON)
└── FunshiEngineGL/                      ← proyecto CMake principal
    ├── CMakeLists.txt                   ← GLOB de fuentes, dependencias, sanitizers,
    │                                      pruebas (CTest), opción BUILD_ENGINE y la
    │                                      versión del producto (FUNSHI_VERSION, variable
    │                                      de caché: la de project() y la del
    │                                      VERSIONINFO del .exe salen de ahí; el
    │                                      workflow la fija con -DFUNSHI_VERSION al
    │                                      publicar por tag). Backend gráfico (IRenderBackend),
    │                                      backend audio (IAudioBackend), backend físico
    │                                      (IPhysicsBackend) y backends de scripts (C++/Java)
    │                                      siguen patrón Strategy.
    ├── Imagenes/                        ← íconos del explorador (cpp, cubo, file, folder, hpp)
    ├── ImGuizmo/                        ← dependencia integrada (ImGuizmo.cpp/.h, etc.)
    ├── External/nlohmann/json.hpp       ← nlohmann/json vendoriado (EditorConfig)

    └── src/
        ├── main.cpp                     ← composition root: ventanas, callbacks, bucle, config
        ├── EngineTime.h / EngineTime.cpp← delta time y limitador de FPS
        ├── Ventana.h / Ventana.cpp      ← inicialización GLFW (ventana y eventos, sin contexto)
        ├── Assets/                      ← caché Flyweight compartida (meshes e imágenes)
        │   ├── AssetManager.h/.cpp      ← registro de AssetPath→Mesh (loader inyectable)
        │   ├── TextureManager.h/.cpp    ← registro de AssetPath→Image (loader inyectable)
        │   ├── Tileset.h/.cpp           ← parseo de tilesets estilo Tiled (image, grilla,
        │   │                                margin/spacing, flag solid) y UV por tile
        │   ├── AssetPath.h / Image.h    ← rutas normalizadas y metadatos de imagen
        │   ├── AssetException.h / TextureException.h ← errores de carga con mensaje y ruta
        │   ├── Mesh.h/.cpp              ← geometría CPU (vértices, normales, índices)
        │   ├── AssimpMeshLoader.*       ← loader Assimp→Mesh
        │   └── StbImageLoader.*         ← loader stb_image→Image (solo engine) +
        │                                   `dimensiones()`: lee la cabecera del
        │                                   archivo sin decodificar
        ├── Audio/                       ← audio del motor (backend inyectable)
        │   ├── AudioEngine.h/.cpp       ← fachada thread-safe (cola de comandos + hilo)
        │   ├── AudioClipsManager.h/.cpp ← descubre clips en Sonidos/ y registra por nombre
        │   ├── IAudioBackend.h          ← contrato Strategy del backend de audio
        │   ├── MiniAudioBackend.*       ← backend concreto (miniaudio vendoriado)
        │   ├── NullAudioBackend.h       ← backend nulo para pruebas headless
        │   └── AudioClip.h              ← handle/metadata del clip
        ├── Behaviour/
        │   ├── IScriptBehaviour.h       ← interfaz de scripts (onStart/onUpdate/onStop, campos)
        │   ├── ScriptGameObject.*       ← tablas versionadas de GameObject y servicios
        │   │                                (audio, teclado, mouse y movimiento físico)
        │   ├── ScriptRuntime.*          ← registro de backends, compilación y hot reload
        │   ├── ComportamientoCargado.h  ← instancia compilada (módulo + campos)
        │   ├── Backends/
        │   │   ├── BackendScript.h      ← interfaz de backend (C++/Java)
        │   │   ├── BackendCpp.*         ← compila .cpp→.so/.dll y lo carga (dlopen)
        │   │   ├── RutaCabecerasScript.h ← resuelve include/ junto al ejecutable o la ruta del checkout
        │   │   ├── ResolucionJdk.h    ← empareja libjvm y javac de una misma raíz de JDK
        │   │   └── BackendJava.*        ← Java vía JNI/JVM dinámico (solo con FUNSHI_JAVA)
        │   └── Reflection/
        │       └── BehaviourReflection.* ← reflexión, macros SerializeField y serialización
        │                                    de valores de script
        ├── Configuracion/
        │   ├── Apariencia.h             ← perfil de apariencia (tema/acento/fondo/B-N) + utilidades
        │   ├── EditorConfig.h/.cpp      ← fachada de la configuración (datos() + cargar*/guardar*, guardado
        │   │                              diferido) y punto de entrada estable del CRUD de proyectos, que
        │   │                              DELEGA en ProjectManager (una sola implementación)
        │   ├── ConfigPersistence.h/.cpp ← JSON puro de la config: general + proyecto (escritura atómica)
        │   ├── ProjectPaths.h/.cpp      ← rutas canónicas del motor (una sola fuente de verdad)
        │   └── ProjectManager.h/.cpp    ← ciclo de vida de proyectos; creación estricta para CLI separada
        │                                  del aseguramiento/migración usado por el editor
        ├── Diagnosticos/
        │   └── RegistroDiagnosticos.h/.cpp ← historial acotado en memoria y persistencia JSON Lines
        ├── Terminal/
        │   ├── TerminalSession.h/.cpp      ← shell interactiva mediante ConPTY o pseudo-terminal POSIX
        │   └── TerminalEmulator.h/.cpp     ← cuadrícula VT y parser de secuencias ANSI
        ├── Entity/
        │   ├── Entity.h                 ← base: lista de componentes, Transform, serialización
        │   └── Entity.cpp
        ├── Estructuras/                 ← contenedores genéricos, header-only
        │   ├── Comparable.h / Ordenadora.h
        │   ├── ListasEnlazadas/
        │   │   ├── ListasDoblementeEnlazada/ListaDE.h
        │   │   ├── ListasConPrioridad/PriorityListaDE.h
        │   │   └── PositionList.h
        │   ├── MetodosDeOrdenamiento/ListMergeSort.h
        │   ├── Nodos/DNodo.h + TNodo.h + BNodo.h  ← nodos de listas, árbol n-ario y binario
        │   ├── Position/Position.h
        │   └── Trees/
        │       ├── Tree.h
        │       ├── Heap/Heap.h + MinHeap.h + MaxHeap.h  ← colas de prioridad (vector)
        │       └── ArbolesEnlazados/
        │           ├── ArbolEnlazado.h  ← árbol n-ario: jerarquía de GameObjects
        │           └── ArbolBinarioEnlazado.h ← binario con addLeft/addRight y preorden
        ├── Events/
        │   ├── EventBus.h/.cpp          ← pub/sub tipado de escena con token de suscripción
        │   └── EditorEventBus.h/.cpp    ← canal tipado de GUI interna (ventanas; Fase 2,
        │                                  dueño: GUIManager)
        ├── ExcepcionesCPP/              ← Throwable, RuntimeException, excepciones de
        │                                  contenedores (ExcepcionesEstructuras/)
        ├── Exportador/
        │   ├── GameExporter.h/.cpp      ← exporta el juego standalone: proyecto CMake temporal,
        │   │                              compila el runtime (sin editor), empaqueta ejecutable +
        │   │                              scenes/scripts/assets en estructura simplificada
        │   ├── ConfigJuegoExportado.h   ← manifiesto JuegoExportado.json (header-only)
        │   ├── RaizEngine.h             ← búsqueda de la raíz del engine para add_subdirectory
        │   └── RutasExportacion.h       ← helpers de rutas del proyecto (header-only)
        ├── Fisicas/
        │   ├── IPhysicsBackend.h        ← contrato Strategy del backend de física
        │   ├── ContactoFisico.h           ← transiciones de contacto entre colliders
        │   ├── PhysicsEngine.h/.cpp        ← fachada PIMPL; el header no expone Bullet
        │   └── BulletPhysicsAdapter.h/.cpp ← adaptador concreto de Bullet (RAII)
        ├── FileManager/
        │   ├── FileManager.h/.cpp       ← fachada del explorador (alto nivel): orquesta modelo,
        │   │                              operaciones de dominio y toda la E/S nativa (diálogos,
        │   │                              abrir con la app del sistema, listado de directorio,
        │   │                              plantillas de scripts). Usa GestorDeArchivos internamente.
        │   ├── FileSelection.h          ← estado de navegación compartido entre vistas
        │   ├── FileSystemWatcher.h/.cpp ← vigilancia de cambios externos (inotify
        │   │                              en Linux; polling con mtime en el resto)
        │   └── Proceso.h/.cpp           ← runner de procesos SIN shell (argv propio:
        │                                  CreateProcessW / fork+execvp, log por handles/fd,
        │                                  cwd, entorno extra; única vía a `cmd.exe` es la
        │                                  receta cruda del harvest de vcvars)
        ├── GestorDeArchivos/            ← modelo/infra de bajo nivel: Binario (streams binarios),
        │                                  File, Carpeta, GestorDeArchivos (exploración del filesystem).
        │                                  FileManager lo consume como modelo del árbol.
        ├── GUI/
        │   ├── GeneralUserInterface.h/.cpp ← interfaz base de paneles ImGui
        │   ├── WindowNames.h
        │   ├── DockSpaceGUI/               ← dock principal del editor
        │   ├── Estado/
        │   │   └── StatusBarInterface.h/.cpp ← ventana "Estado": toolchain (Compilador/javac/
        │   │                                  libjvm) y estado de scripts (compilando/cargado/error)
        │   ├── Terminal/
        │   │   └── TerminalInterface.h/.cpp ← panel de shell integrado con cwd del proyecto
        │   ├── Tema/
        │   │   └── TemaEditor.h/.cpp      ← aplica el perfil Apariencia al estilo ImGui en vivo
        │   │                                  (tema claro/oscuro, acento RGB en TODOS los roles de ImGui
        │   │                                  y grises azulados de fábrica a gris neutro; ver tests/TemaEditorTests.cpp)
        │   ├── FileManagerGUI/             ← TreeFilesInterface + ContentFolderInterface
        │   │                                  (vistas del explorador; conversan con FileManager)
        │   │   ├── SoltarEnCarpeta.h       ← helper header-only del arrastre: mueve y publica
        │   │   │                              ArchivosReubicados; lo usan las dos vistas (en el
        │   │   │                              grid Ctrl copia; soltar en el árbol siempre mueve)
        │   │   └── RenombrarElemento.h     ← helper header-only del renombre por click derecho:
        │   │                                  modal (campo enfocado al abrir, Enter confirma) y
        │   │                                  camino único de disco + ArchivosReubicados, también
        │   │                                  compartido por el grid y el árbol
        │   ├── MenusGUI/                   ← paquete del menú de inicio (MVP); ver su README.md
        │   │   ├── MenuModel.h/.cpp        ← lógica pura sin ImGui/GLFW
        │   │   ├── MenuView.h/.cpp         ← dibujo ImGui
        │   │   ├── StartMenuPresenter.h/.cpp ← puente motor ↔ modelo
        │   │   └── MenuGUI.h/.cpp          ← fachada pública del paquete
        │   ├── ObjetosGUI/
        │   │   ├── SettingsObjectInterface.h/.cpp ← inspector: crea Settings* por componente
        │   │   ├── SettingsComponent.h
        │   │   ├── Transform/SettingsTransform.h/.cpp ← transform + checkbox "Gizmo activo"
        │   │   ├── Color/  Model/  Script/
        │   │   ├── Material/SettingsMaterial.*  Light/SettingsLight.*
        │   │   ├── Camera/SettingsCamera.*       ← FOV, planos, velocidad, vista previa
        │   │   ├── Grid/SettingsGrid.*           ← visible y color de la grilla (infinita,
        │   │   │                                 densidad fija: no hay tamaño/separación)
        │   │   ├── AudioSource/SettingsAudioSource.* ← dropdown de clip (src<proyecto>/Sonidos/),
        │   │   │                                 volumen, loop
        │   │   ├── Interface/SettingsInterface.* ← ruta del .html de la interfaz
        │   │   │                                 (drag & drop) + activa, ancla, tamaño y z
        │   │   ├── Skybox/SettingsSkybox.*       ← visible + las seis caras del cubemap
        │   │   │   └── SelectorArchivoCubemap.h ← modal de elección de cara (header-only):
        │   │   │                                la lógica que decide (extensiones válidas,
        │   │   │                                caras faltantes, resolución dispares) son
        │   │   │                                `inline` puras fuera del dibujo, para que
        │   │   │                                las ejercite un target headless sin ImGui
        │   │   ├── RigidBody/SettingsRigidBody.*
        │   │   └── Colliders/ (Esfera, Cubo, Malla, Tilemap) ← sync transform/shape con física
        │   ├── Export/
        │   │   └── ExportDialog.h/.cpp           ← diálogo de exportación del juego standalone
        │   │                                      (arma `GameExporter::Config` y lo lanza)
        │   └── SceneGUI/
        │       ├── SceneSelectedInterface.h/.cpp  ← jerarquía y selección; usa EditorController
        │       ├── SceneObjectTree.h/.cpp         ← árbol de objetos (con drag & drop)
        │       └── SceneMenuBarInterface.h/.cpp   ← controles Depuración/Juego/Pausa/Reset/Terminar
        ├── GUIManager/
        │   └── GUIManager.h/.cpp         ← fábrica y registro de ventanas; posee FileManager
        ├── Herramientas/
        │   ├── TypeUtils.h/.cpp          ← nombres legibles de tipos
        │   ├── PathUtils.h               ← PATH_SEP + regla de cotejo de rutas (rutaBajo)
        │   ├── MaterialPresets.h         ← presets de Material como datos (reemplaza a materiales.h)
        │   ├── TreeGUI/TreeGUI.h         ← widget de árbol genérico para ImGui
        │   └── IconosGUI/                ← carga de íconos con stb_image
        ├── Iluminacion/
        │   └── LightSystem.h/.cpp        ← datos de las luces por frame (los sube el renderer)
        ├── Input/
        │   └── EditorInput.h/.cpp       ← callbacks GLFW de teclado/mouse + máquina de
        │                                  movimiento de cámara (extraídas de main.cpp)
        ├── ImGui/                        ← Dear ImGui v1.x integrado (backend glfw + fachada
        │                                  de render propia, sin backend gráfico atado)
        ├── Matematicas/
        │   └── StructVec3.h/.cpp         ← vec3 propio
        ├── Rendering/
        │   ├── SceneRenderer.h/.cpp       ← renderizador de la escena: skybox, modelos,
        │   │                                  iluminación, horizonte común de grilla/render,
        │   │                                  ejes y gizmos (pipeline de líneas)
        │   ├── MeshRenderer.h/.cpp         ← dibujo de meshes con difuminado radial al horizonte
        │   ├── TilemapRenderer.h/.cpp      ← sube la malla horneada del Tilemap a GPU (caché
        │   │                                  por revisión) y la dibuja con el shader de sprites
        │   ├── LineRenderer.h/.cpp         ← líneas, wireframes y gizmos recortados al mismo horizonte
        │   ├── MeshGPU.h/.cpp              ← wrapper RAII de la malla en GPU
        │   ├── DibujoModelo.h/.cpp         ← helpers de render por modelo
        │   ├── LineBatch.h/.cpp            ← batch GPU de líneas expandidas
        │   ├── LineBuilder.h/.cpp          ← expansión de segmentos a quads
        │   ├── LineRenderer.h/.cpp         ← API de dibujo de líneas (grilla, ejes, gizmos)
        │   ├── GrillaRenderer.h/.cpp       ← geometría/lógica de grilla infinita
        │   ├── GuiaEje.h/.cpp              ← guía de eje con contraste
        │   ├── TexturaGPU.h/.cpp           ← wrapper de textura 2D + cubemap
        │   ├── RenderTarget.h/.cpp         ← render target (viewport/vista previa)
        │   ├── Cielo.h/.cpp                ← cielo degradado
        │   ├── Difuminado.h/.cpp           ← cálculo de difuminado radial
        │   ├── CacheCubemap.h             ← caché de caras del cubemap
        │   ├── ContratoRender.h            ← contrato de render: tipos de programa, nombres
        │   │                                  de uniform, layout de vértices, uniforms por tipo
        │   ├── Backend/                    ← abstracción de backend gráfico (Strategy)
        │   │   ├── IRenderBackend.h        ← contrato (handles opacos, MeshData, Image2D,
        │   │   │                              DepthFunc, recursos GPU, estados, render target)
        │   │   ├── BackendNulo.h/.cpp      ← implementación en blanco (cumple el contrato, no dibuja)
        │   │   └── BackendActivo.cpp       ← backend activo e instalacion (instalarBackend)
        │   └── Shaders/                    ← ShaderProgram (el fuente del shader es del backend)
        │       ├── ShaderProgram.h/.cpp
        │       └── ShaderException.h
        ├── Objetos/
        │   ├── GameObject.h/.cpp         ← id, nombre, tag, estado, update y serialización binaria
        │   ├── TagRegistry.h             ← registro dinámico de tags para el Inspector
        │   ├── GameObjectFactory.h/.cpp
        │   ├── Prefab.h/.cpp            ← formato .prefab (magic PREB, arbol recursivo con componentes/hijos)
        │   ├── PrefabLibrary.h/.cpp     ← cache + escaneo Assets/Prefabs/, crear/obtener/listar/borrar/renombrar
        │   ├── ClonadorObjetos.h/.cpp   ← copia profunda por roundtrip de serializacion (scripts + Ctrl+C/V)
        │   ├── SimpleObject.h            ← GameObject sin geometría (Transform/Grid/Light...)
        │   ├── Modelos3D.h/.cpp          ← carga Assimp y datos de la malla (el dibujado
        │   │                                es de SceneRenderer, vía MeshRenderer)
        │   └── Componentes/
        │       ├── Component.h           ← interfaz base polimórfica (serialize/deserialize)
        │       ├── ComponentFactory.h/.cpp ← creación por nombre (GUI y deserialización)
        │       ├── Transform.h/.cpp      ← local/global con GLM; anti-NaN; flag gizmoHabilitado
        │       ├── CameraComponent.h/.cpp ← cámara componente: vista, FPS, flag de vista previa
        │       ├── Material.h/.cpp       ← AMBIENT/DIFFUSE/SPECULAR/EMISSION/SHININESS
        │       ├── Light.h/.cpp          ← luz puntual serializable
        │       ├── Grid.h/.cpp           ← grilla del suelo: pasada independiente, visible/color
        │       │                            (los campos tam/separacion quedan solo para que las
        │       │                            escenas viejas se lean igual; no se dibujan con ellos)
        │       ├── Skybox.h/.cpp         ← cubemap de 6 caras que reemplaza al cielo degradado
        │       ├── SpriteRenderer.h/.cpp ← quad texturizado con UV recortable (planilla/frames)
        │       ├── Animador2D.h/.cpp     ← anima la region UV del SpriteRenderer sobre una grilla
        │       ├── Tilemap.h/.cpp        ← capas de tiles (visible/colision), pintado, malla
        │       │                            horneada cacheada por revision y serial binaria FTM1
        │       ├── Color.h / Model.h / Script.h ← varios scripts por objeto,
        │       │                                    nombre editable y metadatos reflejados
        │       │                                    serializados para el inspector
        │       ├── RigidBody/RigidBody.h/.cpp ← cuerpo Bullet sincronizado, habilitable, con detección de apoyo (RAII); masa, gravedad por cuerpo (uso+escala), friccion y freeze por ejes via setGravity/setFriction/setLinearFactor/setAngularFactor, serial v2 con lectura v1
        │       └── Colliders/
        │           ├── Collider.h/.cpp   ← base abstracta; radio; visibilidad persistente y gizmo
        │           │                        con wireframe por el batch de líneas
        │           ├── EsfereCollider.*  ← btSphereShape
        │           ├── CubeCollider.*    ← btBoxShape (half extents = radio)
        │           ├── MallaCollider.*   ← btConvexHullShape a partir de la malla
        │           └── TilemapCollider.*  ← btCompoundShape (una caja por tramo de tiles solidos)
        ├── Proyectos/
        │   └── GestorDeProyectos.h/.cpp  ← ciclo de vida del proyecto activo (entrar,
        │                                  guardar estado y escena Ctrl+S, renombrar, eliminar,
        │                                  exportar, imgui.ini); extraído de main.cpp
        ├── Runtime/
        │   └── main_juego.cpp            ← punto de entrada de juegos exportados (sin editor:
        │                                  GUIManager, MenuGUI, paneles). ImGui básico para runtime,
        │                                  carga escenas desde estructura simplificada (scenes/)
        ├── Comandos/
        │   ├── IComando.h              ← interfaz Command: ejecutar(), deshacer(), descripcion()
        │   ├── GestorComandos.h/.cpp   ← pilas undo/redo (máx 50), ejecuta/deshace/rehace
        │   ├── CrearObjetoComando.h/.cpp
        │   ├── BorrarObjetoComando.h/.cpp
        │   ├── DuplicarObjetoComando.h/.cpp ← Ctrl+C/V por la puerta del editor
        │   ├── ReparentarComando.h/.cpp
        │   ├── TransformComando.h/.cpp
        │   ├── AgregarComponenteComando.h/.cpp
        │   ├── QuitarComponenteComando.h/.cpp
        │   └── LimpiarEscenaComando.h/.cpp
        ├── WebUI/
        │   ├── VistaWebUI.h             ← una interfaz web ya montada por un backend
        │   │                               (estado opaco + ruta .html + sello mtime +
        │   │                               textura/handle + área)
        │   ├── IWebUIBackend.h          ← contrato del motor web (Strategy): subproceso,
        │   │                               init/bombeo/apagado globales, cargar,
        │   │                               redimensionar, actualizar, estado/salida,
        │   │                               render, input (ratón/tecla/rueda/foco),
        │   │                               descargar
        │   ├── BackendNull.h            ← backend sin motor (no-op): corre el ciclo
        │   │                               completo headless, sin CEF ni backend grafico (tests/CI)
        │   ├── BackendCef.h             ← backend CEF real (pimpl sin headers CEF,
        │   │                               el motor en C++17 nunca ve a CEF)
        │   ├── BackendCef.cpp           ← única unidad con CEF (C++20, target
        │   │                               funshi_webui_cef): OSR, textura, mapa
        │   │                               GLFW→VK, sentinela de salida JS
        │   ├── WebUIRuntime.h/.cpp      ← registro + selección de backend (por config,
        │   │                               con fallback al nulo) y despacho del ciclo
        │   │                               global (subproceso/init/pump/apagado) y de
        │   │                               cada vista
        │   ├── EstadoInterfaces.h/.cpp  ← estado de runtime por interfaz: dos árboles JSON
        │   │                               (estado motor→JS y salida JS→motor) por rutas JSON
        │   │                               Pointer; logica sin backend grafico (pimpl + nlohmann)
        │   └── VistasWebUI.h/.cpp       ← vistas vivas de una escena, una por interfaz
        │                                   activa: carga, recarga por cambio de .html
        │                                   o por mtime en disco, redimensiona,
        │                                   entrega el estado y recoge la salida,
        │                                   y descarga (logica sin backend grafico)
        └── Scenes/
            ├── GameScene.h/.cpp          ← coordinador del frame: render, GUI, física, gizmo,
            │                                previews y pasada de la grilla
            ├── InterfacesJuego.h/.cpp    ← recolección de las interfaces de juego (HUD) de la
            │                                escena: recorre TODOS los objetos y devuelve las
            │                                InterfaceComponent activas y con ruta, ordenadas por
            │                                z (logica pura, sin GUI ni backend grafico, probada headless)
            ├── GizmoController.h/.cpp   ← gizmo ImGuizmo + picking (desde GameScene): estado
            │                                (operación/global/listo), arrastre con
            │                                TransformComando (historial) y congelación de
            │                                hijos; interacción vía ImGuizmo + raycast AABB
            ├── SceneRegistry.h/.cpp      ← ownership único (unique_ptr) + árbol + vista lineal
            ├── EditorController.h/.cpp   ← mutaciones + GizmoTarget + registro de física
            ├── SceneSerializer.h/.cpp    ← save/load binario preorden con marcadores =>/<=
            ├── RutasReescritura.h/.cpp   ← reescribe referencias (model/textura/script) al
            │                                mover/renombrar assets en el explorador y sanea
            │                                al cargar las que apuntan a un archivo que ya no
            │                                existe (una coincidencia del nombre bajo la raíz
            │                                → repara y guarda; varias/ninguna → avisa)
            ├── ManifiestoAssets.h/.cpp   ← manifiesto SceneAssets.json (add-on del .db):
            │                                guarda/carga rutas de asset por objeto con
            │                                precedencia sobre el binario (Ctrl+S)
            ├── ManifiestoAssetsCore.h/.cpp ← núcleo puro headless (JSON + relativizar/
            │                                absolutizar + precedencia, con las 6 caras del
            │                                Skybox); tests propios
        └── States/
            ├── ApplicationStateMachine.h/.cpp ← MainMenu/Editing/Debugging/Playing/Exiting
            └── OrquestadorEstadoGUI.h/.cpp    ← reglas de transición menú↔editor y de
                                                   Depuración/Juego: pausa, reset, término y
                                                   Escape; headless, con tests propios
                                                   (orquestador-estado-tests)
```

---

## 3. Flujo de ejecución

```text
main.cpp
  ├── Ventana (GLFW init: ventana y eventos, sin contexto gráfico;
  ─   ─   arranca el backend gráfico con la ventana: es quien crea la superficie)
  ├── ImGui init (backend glfw + fachada de render; imgui.ini gestionado por el gestor)
  ├── GUIManager (crea el menú y las ventanas; posee FileManager)
  ├── GameScene(guiManager)
  │   ├── SceneRegistry            ← ownership de objetos
  │   ├── PhysicsEngine (PIMPL)    ← mundo Bullet vía BulletPhysicsAdapter
  │   ├── EditorController         ← mutaciones, gizmo, registro de física
  │   ├── SceneSerializer          ← persistencia
  │   └── EventBus                 ← notificaciones
  ├── GestorDeProyectos            ← ciclo de vida del proyecto activo (entrar/guardar/
  │                                  renombrar/eliminar/exportar + imgui.ini)
  ├── EditorConfig                 ← carga JSON y aplica a menú/GUI/escena
  ├── ApplicationStateMachine      ← MainMenu / Editing / Debugging / Playing / Exiting
  └── bucle principal
      ├── glfwPollEvents
      ├── EngineTime::update (deltaTime)
      ├── ImGui::NewFrame
      ├── refleja el estado del menú en la fachada MenuGUI (guardia de cambio)
      ├── refleja en la escena el modo (Depuración/Juego), start y pausa
      ├── GameScene::update(dt) SIEMPRE: al iniciar guarda un estado serializado
      │   de la escena; Reset lo restaura con la física reconstruida. Al terminar
      │   ambos modos vuelven al estado previo a la simulación.
      │   En ambos modos avanzan física/scripts/audio y F6 pausa sin reinicio.
      ├── pasada de la grilla (Grid + batch de líneas + shader de ancho en
      │   píxeles; solo fuera de Juego, color efectivo según el perfil de
      │   apariencia y referencia de contraste para la guía de eje)
      ├── dibujarGameObjects (MeshRenderer: malla en GPU + shader; único pipeline)
      ├── gizmo ImGuizmo y overlays auxiliares solo fuera de Juego
      ├── GUI() de GameScene + DockSpace siempre enviados durante edición y
      │   simulación; E solo oculta paneles, y las ventanas flotantes se
      │   reanclan a su último dock válido sin reconstruir los splits
      ├── GestorDeProyectos::sincronizar/eliminar (cambios de proyecto desde el menú)
      ├── ImGui::Render + cierre del frame (el backend envía y muestra)
      └── al salir: GestorDeProyectos::guardarProyectoCompleto (escena + config)
```

`main.cpp` es el composition root; las callbacks de teclado y ratón de GLFW
viven en el módulo `src/Input/EditorInput` (`EditorInput::registrarCallbacks`,
instancia única creada por `main`), que las traduce a acciones del editor y
mantiene la máquina de estado del movimiento de cámara. El estado del menú lo
gobierna el modelo del paquete `MenuGUI`,
sincronizado por frame desde `ApplicationStateMachine`. `GameScene` configura
internamente `GUIManager`, `SceneRegistry`, `EditorController`, `SceneSerializer` y
`EventBus`. El ciclo de vida del proyecto activo (determinar el proyecto al
arrancar, entrar/guardar/renombrar/eliminar, exportar y el `imgui.ini` del
proyecto) vive en `src/Proyectos/GestorDeProyectos`, que inyecta
`EditorConfig`, `GameScene`, `GUIManager` y la fachada `MenuGUI`; `main` queda
solo como orquestador de arranque y bucle.

---

## 4. Relaciones entre los módulos

### Aplicación y escena

- `main.cpp` construye los objetos principales y conecta sus referencias. Es el único composition root.
- `GestorDeProyectos` es la fachada del ciclo de vida del proyecto activo
  (arranque, entrar, guardar estado y escena Ctrl+S, renombrar, eliminar,
  exportar y el `imgui.ini` del proyecto). Se le inyectan `EditorConfig`,
  `GameScene`, `GUIManager` y la fachada `MenuGUI`; el `ImGuiIO` se fija
  después de `ImGui::CreateContext`. Las decisiones de "cuál proyecto" las lee
  de la fachada del menú (misma separación que la escena con la GUI).
- `GameScene` posee mediante `unique_ptr`: `SceneRegistry`, `PhysicsEngine`,
  `EditorController` y `SceneSerializer`. Recibe el `GUIManager` por inyección.
- `SceneRegistry` es el único propietario de los `GameObject`, mediante
  `std::vector<std::unique_ptr<GameObject>>`. El árbol y la lista son vistas no propietarias.
- `EditorController` recibe el registro, la física y el EventBus por inyección.
  Centraliza crear, eliminar, reparentar y modificar componentes; también es el
  **registro central de física** (alta/baja de `RigidBody`) y el dueño del
  `GizmoTarget` (objeto u offset de collider manipulado por el gizmo).
  Publica eventos en `EventBus` después de cada operación exitosa.
- `SceneRegistry::reparent` valida el movimiento: rechaza la raíz, el auto-reparento
  y cualquier ancestro del nuevo padre (no se pueden crear ciclos).
- `EventBus` implementa suscripción tipada mediante tokens (`size_t`). Soporta
  `ObjectCreated`, `ObjectDeleted`, `ObjectReparented`, `ComponentChanged`,
  `SceneCleared`, `ObjectSelected` y `ComponentStructureChanged` (alta o baja de
  componente; el inspector reconcilia solo con este último). No es global: vive
  dentro de `GameScene`.
- `LightSystem` es el dueño de los datos de las luces: cada frame escanea los
  objetos, toma los componentes `Light` y exporta la descripción de cada luz
  (tipo, posición, color, atenuación) que el renderer de la escena sube como
  uniforms del shader. No queda lógica de luz en el bucle ni en los componentes.
- `ApplicationStateMachine` modela `MainMenu`, `Editing`, `Debugging`, `Playing`
  (Juego) y `Exiting`. `OrquestadorEstadoGUI` decide Escape, Iniciar Estudio,
  F5/Depuración, Juego, pausa, Reset y término; `GameScene` conserva un snapshot
  serializado al iniciar y reconstruye física y referencias al restaurarlo.
  `menuActivo` solo gobierna paneles GUI; el flag de simulación refleja los dos
  modos en `GameScene`.
- `EditorController` posee un `GestorComandos` que envuelve cada mutación
  (crear/borrar/reparentar, cambios de transform, agregar/quitar componentes,
  limpiar escena) en un `IComando`. Las operaciones de la GUI van por el
   gestor, nunca directamente al `SceneRegistry`, de modo que Ctrl+Z/Ctrl+Y
   funcionan de forma transversal. La pila mantiene un máximo de 50 comandos;
   cada nueva acción invalida la pila de redo. La raíz de la escena (id=0)
   está excluida de delete/clear por diseño.
- Equivalencia GUI ↔ CLI (contrato): toda operación de la GUI de edición debe
  pasar por un `IComando` con parámetros explícitos (los que ya existen:
  crear/borrar/reparentar, transform, agregar/quitar componentes, limpiar
  escena), de modo que cada acción del editor tenga un equivalente invocable
  sin GUI. Esto habilita un editor por línea de comandos que manipula la
  creación del videojuego con los mismos comandos, sin interactuar con la
  GUI. Regla: ninguna mutación de escena desde la GUI salta el gestor; un
  comando nuevo se agrega con su clase en `Comandos/` y su caso en
  `tests/ComandosTests.cpp`.
- Settings manipulables por código: los datos públicos que muestran los
  paneles de componentes (`Settings*`) deben tener su contraparte escribible
  por código: setters del componente y/o entradas en las tablas de scripts
  (`ApiScriptGameObject`/`ScriptServices`, append-only). Así lo que se ajusta
  con sliders/checkboxes en el inspector también se genera y manipula desde
  scripts, y la creación de objetos por código no depende de la GUI.

### Entidades, objetos y componentes

- `Entity` contiene una `ListaDE<Component*>` y un `Transform` base. Define la interfaz abstracta de serialización (métodos `serialize*`/`deserialize*`).
- `GameObject` extiende `Entity` e implementa `Comparable<GameObject>`. Añade:
  id entero, nombre (`inputName[25]`), color auxiliar (`auxColor`), estado (`bool`),
  ciclo `update(dt)` y la lógica de serialización binaria concreta.
- `Modelos3D` extiende `GameObject`. Carga geometría (vértices, normales, índices)
  mediante Assimp y serializa adicionalmente la ruta del archivo del modelo. No
  tiene método de dibujado: la entidad no conoce la capa de Rendering, y es
  `SceneRenderer` quien la pinta vía `MeshRenderer`.
- Los componentes concretos son: `Transform`, `Color`, `Model`, `Material`, `Light`,
  `AudioSource`, `InterfaceComponent`, `Grid`, `Script`, `EsfereCollider`,
  `CubeCollider`, `MallaCollider` y `RigidBody`. Todos heredan de `Component`
  y serializan sus datos binarios.
- `ComponentFactory` centraliza la creación por nombre de tipo tanto desde la GUI
  como durante la deserialización (`"CameraComponent"` acepta el alias `"Camera"`).
- `Component::onLoaded(GameObject&)` es el hook polimórfico de post-carga:
  `deserializeEntityComponents` aplica `loadComponent()` y luego el hook, sin
  despachar por nombre de tipo. `Color` lo usa para reflejar su valor en
  `GameObject::auxColor` (el buffer del inspector); cualquier componente nuevo
  con dependencias post-carga lo implementa en vez de abrir el objetos de carga.
- `Transform` provee transformaciones locales y globales usando GLM; expone
  descomposición de matrices para ImGuizmo y blinda su cadena contra NaN
  (valores no finitos no entran al estado ni al render).
- La cadena física: los colliders crean la `btCollisionShape` (reconstruida si
  cambia el radio), `RigidBody` la conecta al mundo de `PhysicsEngine`, y el sync
  de transformaciones entre objeto, collider y cuerpo se hace con matrices
  compuestas (global T del dueño + offset del collider), no con deltas sueltos.

### Jerarquía y contenedores

- `ArbolEnlazado<GameObject*>` representa la jerarquía padre-hijo dentro de
  `SceneRegistry`. No libera los objetos apuntados (la ownership está en el vector).
- `ListaDE<GameObject*>` es una vista lineal reconstruible de `SceneRegistry`,
  usada por GUI, render, iluminación y actualización. Tampoco es propietaria.
- `TNodo` contiene sus hijos mediante una `ListaDE<TNodo<E>*>`;
  `DNodo` implementa los nodos de listas doblemente enlazadas y `BNodo` los del
  binario (con destructor recursivo de subárbol).
- Las estructuras genéricas son header-only para permitir la instanciación de plantillas.
- Estado de las estructuras (ver `tests/EstructurasTests.cpp`):
  - **Usadas por el motor**: `ListaDE`, `PriorityListaDE`, `ArbolEnlazado`.
    ListaDE y ArbolEnlazado fueron corregidas (inicialización de nodos,
    `addFirst`, liberación de nodos en `remove` y destructores, orden de
    borrado en `deleteRoot`/`deleteInternalNode`/`deleteExternalNode`) y su
    comportamiento está cubierto por la suite.
  - **Disponibles y probadas, sin consumidores todavía**: `MinHeap`, `MaxHeap`,
    `ListMergeSort` y `ArbolBinarioEnlazado`. Fueron implementadas (antes eran
    stubs vacíos que el README daba por hechos) y validadas con las mismas
    pruebas, aptas para su uso por el motor cuando hagan falta.

### GUI

- `GeneralUserInterface` define la interfaz común (`initGUI`, `contentGUI`, `printGUI`, `endGUI`).
- `GUIManager` actúa como fábrica y registro centralizado de todas las ventanas
  del editor. Expone métodos tipados (`getMenuGUI`, `getTreeFilesGUI`, etc.),
  guarda/restaura el estado abierto/cerrado de las ventanas y **posee el
  `FileManager`** del proyecto.
- El explorador de archivos es una arquitectura de tres piezas: `FileManager`
  (fachada dueña del modelo `GestorDeArchivos` y de las operaciones de dominio
  —crear/renombrar/eliminar/copiar/**mover**— **más toda la E/S nativa del
  sistema**:
  diálogos de selección de carpeta/archivo, abrir con la app predeterminada,
  listado de directorio con symlink-safe y plantillas de scripts C++/Java),
  `FileSelection` (estado de navegación compartido) y las vistas
  `TreeFilesInterface`/`ContentFolderInterface`, que solo conversan con la
  fachada (sin `system()`/`popen()`/`ShellExecute*` ni `directory_iterator`
  propios).
  `FileSystemWatcher` avisa de cambios externos (inotify) para re-escanear.
  El **arrastre** lo resuelve `SoltarEnCarpeta.h`, compartido por las dos
  vistas: dentro del grid soltar mueve (con `Ctrl` copia) y sobre una fila del
  árbol siempre mueve (cortar y pegar, sin opción de copia); al mover, publica
  `ArchivosReubicados` para que el gestor de proyectos reescriba y guarde las
  referencias de la escena; el cotejo de ese prefijo vive en
  `PathUtils::rutaBajo` y trata `/` y `\` como el mismo separador en Windows,
  porque el explorador trae las rutas de `std::filesystem` con `\` y
  la escena las resuelve con `/`. El **renombre por click derecho** (grid y
  árbol) vive en `RenombrarElemento.h`: mismo modal en los dos paneles y un
  único camino de disco que, si el cambio se hizo, publica el mismo
  `ArchivosReubicados`; quien renombra sigue siendo dueño de su navegación (el
  árbol reubica `rutaVisible` y la expansión por ruta de la rama antes del
  rescaneo). `GestorDeArchivos::renombrar` también se niega a pisar un destino
  existente (mismo criterio que mover, permitido solo el MISMO elemento, p. ej.
  para cambios de mayúsculas); `GestorDeArchivos::mover` usa `rename`, que es
  atómico, y solo cae a copiar+borrar si el destino está en otro volumen;
  rechaza pisar un destino ocupado y meter una carpeta en sí misma. Al cargar
  la escena, `GameScene::loadScene` invoca el mismo módulo para sanar las
  referencias rotas (busca el nombre bajo la raíz del proyecto): con una sola
  coincidencia repara y guarda; con varias o con ninguna avisa en el log y no
  adivina.
- `SceneSelectedInterface` observa la escena inyectada y delega las
  operaciones de edición a `EditorController`. Usa `GameObjectFactory` para crear objetos.
- `SceneObjectTree` dibuja el árbol de objetos (con drag & drop para reparentar).
- `SettingsObjectInterface` inspecciona un `GameObject` seleccionado y crea/actualiza
  los paneles `Settings*` específicos de cada componente presente. El checkbox
  **"Gizmo activo"** de `SettingsTransform` enciende/apaga el gizmo de ese
  transform (del objeto o del offset del collider) sin deseleccionar.
- `SceneMenuBarInterface` ofrece los controles de Depuración/Juego y, durante
  una simulación, Pausa/Reset/Terminar; las transiciones pasan por el orquestador.
- `MenuGUI` es la fachada del paquete `MenusGUI` (menú principal): `MenuModel`
  (lógica pura) + `MenuView` (ImGui) + `StartMenuPresenter` (puente motor).
  Ver `src/GUI/MenusGUI/README.md`.

### Persistencia

- `SceneSerializer` es el único responsable del archivo `BBDDObjetos.txt` y de los
  `.db` individuales. Guarda en preorden: escribe el binario de cada objeto y
  registra la ruta; inserta `=>` para abrir un bloque de hijos y `<=` para cerrarlo.
- La carga reconstruye la escena **de forma recursiva** (`loadPreOrder` con
  look-ahead de marcadores), restaurando padres e hijos; inserta cada objeto
  directamente en `SceneRegistry` vía `EditorController`. El índice se abre en
  **binario** (el `seekg(tellg())` del look-ahead no es fiable en modo texto
  con CRLF, ver `DocuTecnicoBugs.md` §7) y cada línea se valida contra el
  patrón estricto `ObjectN<entero>.db`: lo que no cumple — o un hijo con
  id 0, que leería `ObjectN0.db`, el binario de la raíz — se salta con aviso.
- `Binario` encapsula los streams binarios usados por las entidades.
- `GameObject::saveEntity/loadEntity` coordina la serialización binaria propia
  (atributos globales, locales, componentes).
- La configuración del editor se persiste en JSON con **una sola fuente de
  verdad**: `Configuracion/ConfigPersistence.{h,cpp}` (JSON puro, sin estado)
  sobre `Configuracion/ProjectPaths.{h,cpp}` (rutas), con
  `EditorConfig.{h,cpp}` como fachada estable que expone `datos()` y
  `cargar*/guardar*` a main, escenas y tests. El **ciclo de vida de los
  proyectos** (`asegurarEstructuraProyecto`, `asegurarEstructuraBase`,
  `renombrarProyecto`, `eliminarProyecto`) no se implementa en EditorConfig:
  son **delegaciones a `ProjectManager`**, único dueño del CRUD, las
  migraciones de estructura antigua y los fallbacks de copia entre
  dispositivos. La política de qué proyecto queda abierto al arrancar (menú
  salvo `--proyecto`, y si es primer arranque) vive aislada y sin
  disco ni UI en `Configuracion/ProyectoInicial.h`. La orquestación de todo el flujo de proyectos sobre esta
  fachada (qué hará al arrancar, entrar, guardar, renombrar, eliminar,
  exportar y cuál es el `imgui.ini` vigente) vive en
  `Proyectos/GestorDeProyectos` (extraído de `main.cpp`). Guarda dos archivos
  junto al binario (Linux y Windows), y cada campo va a uno u otro según si es
  una preferencia de la máquina o del proyecto abierto:
  `<directorioEjecutable>/MotorGrafico/Configuraciones/Configuracion.json`
  (**general**: último proyecto, idioma, sensibilidad de cámara, sensibilidad de
  movimiento y perfil de apariencia) y
  `Proyects/<proyecto>/Memory/ConfiguracionProyecto.json` (**por proyecto**:
  operación y sistema de coordenadas del gizmo, ventana de cámaras, estado
  abierto/cerrado de las ventanas de `GUIManager` y cámara activa por id).
  Tolerante a archivos ausentes o corruptos: los defaults
  viven en `EditorConfig.h`/`ConfigPersistence.h`.
  La raíz `MotorGrafico` no es siempre `<directorioEjecutable>/MotorGrafico`:
  `ProjectPaths::directorioBase()` la resuelve probando si puede escribirse y, si
  no, cae a la carpeta de datos del usuario (`%APPDATA%` en Windows,
  `$XDG_DATA_HOME` o `~/.local/share` en Linux). Ver "Resolución de la raíz de
  datos" más abajo.
  - **Escritura atómica**: `ConfigPersistence::escribirJson` escribe a
    `<archivo>.tmp` y renombra encima; un corte a mitad de escritura no deja
    el JSON cortado ni temporales colgados.
  - **Guardado diferido de la general**: `EditorConfig::solicitarGuardadoGeneral`
    encola los cambios en vivo de Opciones y `volcarGuardadoGeneral()` (llamado
    por main una vez por frame) escribe como máximo una vez cada
    `kIntervaloEscritura` (250 ms); `guardarGeneral()` (Ctrl+S, salida, reset)
    vuelca el pendiente sin esperar.
  - El layout `imgui.ini` también se guarda junto al proyecto (no en el CWD).
- Carpeta de un proyecto: `Proyects/<proyecto>/` con `Memory/` (escena,
  configuración del proyecto e `imgui.ini`) y la raíz de
  assets `src<proyecto>/` (meshes, texturas, materiales, `Sonidos/`,
  `Scripts/`). Todas esas rutas salen de `ProjectPaths`
  (`directorioSrc`, `directorioSonidos`, `directorioScripts`), incluido el
  exportador del juego standalone, que las pide ahí mediante
  `Exportador/RutasExportacion.h` (lógica pura, header-only) en vez de
  componerlas desde la carpeta del proyecto: componiéndolas a mano las copias
  se saltaban en silencio y el juego exportado salía sin sonidos, sin scripts
  del usuario y sin configuración.
- Limitación conocida: la serialización binaria no tiene versionado ni validación
  de tamaños; un cambio en la estructura de atributos invalida escenas guardadas.

### Scripts dinámicos

- `Script` (componente) orquesta el ciclo de vida del módulo y guarda los valores de
  los campos (`SerializeField`) en la serialización binaria de la escena.
- `ScriptRuntime` registra los backends por extensión, compila/carga el fuente y
  aplica hot reload comparando la fecha de modificación (conserva y reinyecta los
  valores por nombre de campo).
- `IScriptBehaviour` define la interfaz: `onStart`/`onUpdate`/`onStop` y
  `camposReflejados()`; el motor inyecta la tabla `MotorScript::ApiScriptGameObject`
  (nombre, transform completo con getters de rotacion/escala, log y control
  horizontal/salto sobre `RigidBody`) y la tabla
  `MotorScript::ScriptServices` (audio, busqueda de objetos por nombre, id o
  etiqueta y
  consulta de teclado via `InputScripts`, inyectadas por `GameScene` al iniciar
  la simulacion) para que el script no enlace contra el motor. Ambas tablas siguen
  versionado APPEND-ONLY con campo `version` final para guardas en runtime.
  Las posiciones de la API de transform son locales. Durante el cierre, GameScene
  conserva los servicios hasta completar `onStop`, detiene los handles de audio
  iniciados por scripts y luego desconecta el contexto, sin afectar la propiedad
  de audio de los componentes. `InputScripts` expone los edges del teclado y el
  delta del mouse en pixeles durante el frame actual.
-   `BehaviourReflection` implementa la reflexión por macros (`REFLECT_INICIO`,
  `CAMPO`, `ARRAY`, `GRUPO`, `GRUPOS`, `COMPONENTE`, `FIN`), la conversión de
  valores tipados y la serialización binaria autodescriptiva de los campos.
  `CAMPO`, `ARRAY`, `GRUPO`, `GRUPOS`, `COMPONENTE`, `FIN`), la conversión de valores
  tipados y la serialización binaria autodescriptiva de los campos.
- Una referencia a un componente de otro objeto (`TagTipo::Componente`, macro
  `REFLECT_COMPONENTE(campo, "Tipo")`) se serializa por el **nombre del objeto
  dueño**. El motor la resuelve con dos ganchos que inyecta `GameScene`
  (`fijarResolverComponentes`): nombre+tipo → puntero, y puntero → nombre dueño.
- La API cubre propiedades fisicas (masa, gravedad por cuerpo, friccion,
  freeze), gravedad global y gestion de objetos (crear/clonar/destruir,
  agregar collider y cuerpo) con cola diferida a fin de frame; `ClonadorObjetos`
  (copia profunda por roundtrip de serializacion) lo comparten los scripts y
  el Ctrl+C/V del editor (`EditorController::duplicarObjeto`).
- `BackendCpp` compila el `.cpp` a `.so`/`.dll` con el compilador configurado y lo
  carga con `dlopen`/`LoadLibrary`; `BackendJava` (opcional, `-DFUNSHI_JAVA=ON`)
  compila con `javac` y ejecuta sobre un JVM cargado dinámicamente vía JNI.
  Java accede a las mismas funciones de objeto y servicios de escena mediante
  las funciones nativas de `Nativo`, sin enlazarse directamente con los módulos.
  Los caches C++ y Java incluyen la version del runtime de scripts, por lo que
  un cambio de API/runtime recompila los fuentes y no ofrece ABI binaria entre
  versiones del motor.
  La eleccion del toolchain Java vive en `Behaviour/Backends/ResolucionJdk.h`, que
  es codigo puro (sin JNI, para poder probarlo en un test headless): recorre las
  raices candidatas en un orden fijo y **empareja** `libjvm` y `javac` de una misma
  raiz, para que el `.class` se compile con el mismo JDK que lo ejecuta (con dos
  toolchains distintos, un `javac` mas nuevo que la JVM produce
  `UnsupportedClassVersionError`, que el motor|reportaba como "clase no
  encontrada"). `FUNSHI_LIBJVM` y `JAVAC` siguen mandando por encima de la
  busqueda; si la raiz elegida no trae `javac` (es un JRE) no se busca otro
  compilador. La biblioteca que entrega `FindJNI` llega horneada como la ruta
  del **archivo**, no como una raiz: se resuelve aparte y solo si ese archivo
  existe en la maquina, porque pasada por la lista de raices se descartaba
  siempre (buscaba `<archivo>/lib/server/libjvm.so`). El texto que cruza la
  frontera JNI (`NewStringUTF`) se convierte a
  UTF-8 con `std::filesystem::path::u8string`, que en Windows convierte la
  codificacion ANSI nativa a UTF-8; las rutas que van a `JavaVMOption` y a
  `Proceso::ejecutar` se dejan estrechas a proposito, porque esas APIs usan la
  codificacion nativa.
  Los flags con los que se compila el script viven en
  `Behaviour/Backends/ComandoCompilacionCpp.h` y cumplen el mismo contrato de
  CRT que el engine (`/MD` o `/MDd` según la configuración, más `/EHsc`): la
  reflexión cruza la frontera de la `.dll` con objetos STL que aloca un módulo y
  libera el otro, así que un CRT distinto (p. ej. el estático `/MT`) separa los
  heaps y corrompe la memoria.
- El editor (`SettingsScript`) dibuja los campos reflejados (escalares, arrays,
  grupos, referencias a `GameObject` y a componentes como el `InterfaceComponent`,
  asignables arrastrando desde su inspector) y dispara la recompilación.
- Qué compilador se invoca lo decide `BackendCpp::compilador()` con el mismo
  criterio que el resto: `FUNSHI_CXX` del entorno si está (manda siempre), el
  valor que hornea CMake **solo si existe en disco** —la ruta horneada es la del
  equipo que compiló, y en el del usuario no existe—, y si no `cl` del PATH
  cuando hay un toolset MSVC en la máquina o `g++` en el resto de plataformas.
  El entorno de MSVC sale de `vcvars64EnRaices`, que busca `vcvars64.bat` a dos
  niveles bajo las raíces de Visual Studio (`<raiz>/<año>/<edición>` y también
  las Build Tools, que cuelgan igual); con Visual Studio presente se invoca
  `cl` a secas porque el propio entorno de vcvars lo pone en el PATH, así que el
  motor no queda atado a la versión instalada. Las dos decisiones viven como
  funciones puras en `ComandoCompilacionCpp.h` (`elegirCompilador` y
  `vcvars64EnRaices`, esta con las raíces por parámetro) para poder probarlas
  con rutas falsas, sin depender de un Windows.
- Las cabeceras que el script incluye al compilarse (`Behaviour/IScriptBehaviour.h`
  y las tres que arrastra) se resuelven con `RutaCabecerasScript`: el motor recibe
  la carpeta de fuentes por `FUNSHI_SRC_DIR`, que el paquete deja como el nombre
  relativo `include` y el editor resuelve contra la carpeta del ejecutable (el
  build de desarrollo lo sobreescribe con la ruta absoluta del checkout). Sin esa
  carpeta el backend avisa antes de invocar al compilador, en vez de dejar que
  falle con un `-I` inexistente.

### Interfaces de juego web

Las interfaces gráficas de los juegos (HUD) se escriben en HTML/CSS/JS y se
montan sobre un motor web embebido. El subsistema vive en `src/WebUI/` y sigue
el patrón Strategy + registro de los backends de audio y de scripts:

- El `InterfaceComponent` guarda solo la ruta del `.html` (con su colocación:
  ancla, desplazamiento, tamaño y `z`); el motor web resuelve los recursos
  relativos (CSS, JS, imágenes) por sí mismo.
- `IWebUIBackend` es el contrato del motor web: `ejecutarSubproceso` (rol de
  subproceso del motor web, antes de todo en `main`), `inicializar` (arranque
  una vez por proceso, con la ventana lista), `bombear` (bucle de mensajes por frame),
  `cargar`, `redimensionar`, `actualizar`, `establecerEstado` (entrega el
  estado JSON a la página), `leerSalida` (recoge `FunshiUI.salida`), `render`
  (que devuelve el handle de textura para ImGui), el input neutral (ratón con
  `BotonRatonWebUI`, rueda, teclado por código GLFW y foco), `descargar` y
  `apagar`.
- `WebUIRuntime` es la fachada de despacho: mantiene un registro de backends y
  selecciona el activo (por configuración, con fallback al nulo ante un nombre
  desconocido). Expone el ciclo global (`ejecutarSubproceso`, `inicializar`,
  `bombear`, `apagar`, que `main.cpp` y `main_juego.cpp` invocan) y reenvía el
  ciclo de cada vista al backend activo. Es la única puerta que usa el resto
  del motor.
- `BackendNullWebUI` corre el ciclo completo sin motor web ni backend grafico, para tests
  headless y CI.
- `BackendCefWebUI` es el backend real sobre CEF 154 (Chromium): una ventana
  off-screen (OSR) por vista, bitmap BGRA a textura vía `updateTexture2D`,
  salida JS por sentinela en `console.log`, teclado por mapa GLFW→VK. Vive en
  su propia unidad C++20 (`BackendCef.cpp`, pimpl sin headers CEF en el resto)
  y solo se compila con `-DFUNSHI_CEF=ON` (requiere `-DFUNSHI_WEBUI=ON`); CMake
  descarga la distribución pineada bajo el build dir y copia el runtime junto
  al binario. Sin CEF el runtime cae al nulo.
- `EstadoInterfaces` guarda, por interfaz viva, dos árboles JSON: el **estado**
  que el motor entrega a la página y la **salida** que la página devuelve. Se
  manejan por rutas estilo JSON Pointer. Es logica pura (pimpl + nlohmann), sin
  backend grafico, y los scripts la manipulan por la tabla de servicios.
- `VistasWebUI` gobierna las vistas vivas de una escena (una por interfaz
  activa): las da de alta, recarga al cambiar la ruta del `.html` o al cambiar
  el archivo en disco (sello `mtime`, patrón `cambioElFuente` de scripts),
  redimensiona, entrega el estado pendiente, recoge la salida, avanza y
  descarta las que dejan de estar activas.
- `GameScene` recolecta las interfaces activas (`InterfacesJuego`), sincroniza
  `VistasWebUI` con el tamaño del área de juego y su `EstadoInterfaces`, y
  dibuja cada vista con su textura vía el *draw list* de ImGui, respetando el
  orden por `z`. En el editor además pinta la ventana dockable `Vista de
  interfaz` con la vista de la interfaz seleccionada (preview + input local
  con foco por hover).

---

## 5. Dependencias externas

| Dependencia | Uso | Integración |
|---|---|---|
| GLFW | Ventana y eventos de input | `find_package(glfw3)` |
| Bullet Physics | Física y colisiones (btDiscreteDynamicsWorld) | `find_package(Bullet)` con fallback a targets clásicos |
| Assimp | Carga de modelos 3D (opcional via `USE_ASSIMP`) | `find_package(assimp)` |
| GLM | Matemáticas de Transform y cámaras (`GLM_ENABLE_EXPERIMENTAL`) | Usa `External/glm` si existe; si no, `find_package(glm)` del sistema |
| nlohmann/json | `EditorConfig` (persistencia de configuración) | Vendoriado en `External/nlohmann` |
| Dear ImGui | Interfaz del editor | Integrada en `src/ImGui/` |
| ImGuizmo | Gizmos de transformación | Integrada en `FunshiEngineGL/ImGuizmo/` |
| ncurses / X11 | Enlace en Linux | `find_package(Curses)` + X11/Xrandr/Xi |

El motor no depende de ninguna API gráfica: el renderizado lo resuelve un
backend intercambiable que se instala en tiempo de ejecución (el que viene en
este repositorio cumple el contrato y no dibuja). Por eso no hay OpenGL ni
Vulkan entre las dependencias: el backend que se agregue trae las suyas.

En Windows se enlaza además `dbghelp` (stack traces de `RuntimeException`).

---

## 6. Organización del código

La convención general es un par `.h`/`.cpp` por clase. Las excepciones son:

- Estructuras de datos genéricas, que deben ser header-only para la instanciación de plantillas.
- Componentes pequeños y algunas interfaces GUI, parcialmente inline por el acoplamiento actual.
- Headers de terceros, que conservan la organización original.

`CMakeLists.txt` descubre automáticamente los `.cpp` y `.h` bajo `src/` mediante
`GLOB_RECURSE CONFIGURE_DEPENDS`. Los nuevos archivos de implementación se incorporan
al build sin enumerarlos manualmente. Los archivos de ImGui se recopilan por separado
desde `src/ImGui/` y los de ImGuizmo desde `ImGuizmo/` (fuera de `src/`).

Además del ejecutable, el proyecto define **veintinueve targets de prueba
headless** registrados en CTest (`scripts-java-tests` solo se registra con
`-DFUNSHI_JAVA=ON`; nueve de ellos viven bajo `BUILD_ENGINE=ON`
—`comandos-tests`, `escena-serializacion-tests`, `catalogo-componentes-tests`,
`tilemap-tests`, `interfaces-juego-tests`,
`prefab-tests`, `manifiesto-assets-tests`, `orquestador-estado-tests` y
`tema-tests`—, y el resto compila también con `BUILD_ENGINE=OFF`):

- `filemanager-tests` (190 en Linux; 181 en Windows con MinGW, 183 con MSVC): ejercita `GestorDeArchivos`/`FileManager`/`FileSystemWatcher`
  contra un proyecto temporal, sin ventanas ni pila gráfica; incluye el arrastre
  con invalidación explícita de caché del grid en carpeta origen y destino, el
  renombre por click derecho de las vistas del explorador y la creación de
  carpetas (`CrearCarpeta`, compartida por árbol y grid: valida el nombre, avisa
  del nombre repetido sin cerrar el modal y sube el contador una sola vez).
  También la lógica
  pura de `SelectorArchivoCubemap` (filtro de extensiones del cubemap y aviso
  de caras faltantes o de resolución dispares), que al vivir fuera del dibujo del
  modal se ejercita aquí sin crear contexto de ImGui.
- `proceso-tests` (24): el runner de procesos sin shell `Proceso`: round-trip
  de argv byte a byte (el binario se relanza a sí mismo copiado en una carpeta
  con espacios, con argumentos hostiles), exit codes, truncado del log, `cwd`,
  entorno extra, tabla de `citar()` y, en Windows, la receta cruda de `cmd.exe`
  del harvest de vcvars.
- `configuracion-tests` (174): round-trip del JSON de `EditorConfig` (general y
  por proyecto, con `ConfigPersistence`/`ProjectPaths`), carga tolerante ante
  archivos ausentes/corruptos/parciales, prioridad de las claves modernas sobre
  el `menu/*` legacy, `restablecer`, escritura atómica y guardado diferido, la
  política de proyecto inicial `ProyectoInicial::resolver`, los colores del cielo
  (se conservan tal como se guardaron —un cielo claro incluido— y solo se
  acotan los componentes fuera de `[0, 1]`), el cotejo de prefijos `rutaBajo` (en
  Windows `/` y `\` equivalen y se ignoran mayúsculas y minúsculas; en Linux se
  mantiene sensible al caso), y la elección de la raíz de datos
  (`ProjectPathsDetalle::elegirRaizDeDatos`: junto al ejecutable si ahí se puede
  escribir; si no, la carpeta de datos del usuario, migrando lo que ya hubiera
  junto al ejecutable).
- `eventbus-tests` (17): suscripción/publicación/unsubscribe del canal tipado de GUI.
- `menu-tests` (40): lógica pura del menú (traducción, observer de cambios —sin
  notificar al reaplicar una apariencia idéntica— y reset).
- `assetmanager-tests` (95): caché Flyweight de meshes (rutas `AssetPath`, con los prefijos
  UNC y `\\?` preservados al normalizar) y geometría de `Mesh` con `computeBounds`,
  `computeNormals` —incluido el modo `soloFaltantes` para assets que mezclan sub-mallas con
  y sin normales— y `computeTangents`; registro compartido con un loader artificial.
- `texturemanager-tests` (15): caché Flyweight de imágenes CPU (sin entrar la pila gráfica).
- `estructuras-tests` (87): `ListaDE`, `ArbolEnlazado`, `PriorityListaDE`,
  `MinHeap`/`MaxHeap`, `ListMergeSort` y `ArbolBinarioEnlazado`.
- `rendering-tests` (187): geometría de las líneas del pipeline moderno
  (`LineBuilder`): expansión de cada segmento al quad que ensancha el shader,
  color por extremo (difuminado de la grilla), polilíneas, aristas con índices
  fuera de rango y caja de 12 aristas; más la guía de eje (`GuiaEje`): origen y
  dirección en mundo, dirección local rotada con el objeto, normalización frente
  a la escala, colores por eje, el color efectivo que aleja el eje de una grilla
  de su mismo color sin perder la convención (X roja, Y verde, Z azul) ni su
  opacidad, recorte de la recta hasta el horizonte con la
  distancia 3D a la cámara, difuminado por vértice y los rechazos defensivos
  (eje inválido, NaN/Inf, eje degenerado, horizonte degenerado, recta fuera del
  horizonte); más el difuminado del piso (`Difuminado`): el inicio derivado en
  proporción constante del radio elegido, el radio acotado (finito, enorme,
  negativo y nulo) y la forma de la curva, que no depende del radio; más los
  colores efectivos del cielo degradado (`Cielo`): los dos extremos con B/N y
  tema resueltos; y la identidad del cubemap del Skybox (`CacheCubemap`): la
  clave que decide cada cuanto volver a subirlo a GPU cambia solo si cambia una
  ruta o su fecha de modificación. Solo CPU, sin pila gráfica.
- `contrato-render-tests` (575): el contrato de render. La tabla de uniforms
  por tipo de programa (`ContratoRender.h`) es completa, no repite nombres y
  no declara ninguno que no exista en el namespace de uniforms; cada tipo
  expone los uniforms que su semántica exige (el difuminado radial de las
  líneas, el recorte de uv y el tinte de los sprites, el modelo de iluminación
  de las mallas, el degradado del cielo); los renderers escriben los uniforms
  con las constantes del contrato y nunca con literales; el backend en blanco
  cumple el contrato entero sin dibujar (todo handle inválido, ningún
  localizador de uniform, diagnóstico de GPU presente) y el motor arranca y
  omite las pasadas; y el backend se puede instalar y reemplazar, con el motor
  pidiendo los programas por tipo. El límite de frame también es del contrato:
  el backend se inicializa con la ventana nativa, y `beginFrame`/`endFrame`
  abren y cierran cada frame. No hay GPU: el backend de mentira solo
  registra las llamadas, así que el resultado no depende del driver.
- `scripts-tests` (173): reflexión `SerializeField` (escalares, arrays, grupos
  anidados y referencias a componentes por `REFLECT_COMPONENTE`, con su
  resolución por nombre del objeto dueño) y su round-trip binario; el contrato
  de flags con el que
  `BackendCpp` compila los scripts (CRT, `/EHsc`, familia de compilador, los
  ARGV armados sin shell ni redirección); la resolución de la carpeta de
  cabeceras del script (`RutaCabecerasScript`: absoluta del checkout o nombre
  relativo `include` resuelto contra la carpeta del ejecutable en la app
  instalada); el descubrimiento del toolset MSVC de la máquina del usuario
  (`vcvars64EnRaices`, con las raíces por parámetro para poder probarla con un
  árbol falso: dos niveles, que es donde cuelgan las Build Tools, y primera
  raíz por preferencia) y la decisión de qué compilador se invoca
  (`elegirCompilador`: override del entorno, valor horneado solo si existe en
  disco, `cl` del PATH si hay toolset o `g++`); el harvest del entorno de vcvars
  (receta cruda de `cmd`, parser UTF-16, bloque multi-sz); y el contrato del
  sondeo de toolchain (dispositivo nulo `NUL`/`/dev/null` abierto por el
  runner, sin `std::system`); el emparejamiento de `libjvm` y `javac` de una
  misma raíz de JDK (`ResolucionJdk`, con sus overrides por entorno y la
  dedución de la raíz desde la ruta de la biblioteca en los layouts de POSIX y
  Windows, que el compilador que se pasa al proceso sea ejecutable, y que la
  biblioteca horneada por CMake —que es la ruta del archivo, no una raíz— solo
  se use si ese archivo existe en la máquina, como último recurso) y la paridad
  del requisito de JDK del instalador (que exige
  `jvm.dll` **y** `javac.exe`).
- `scripts-runtime-tests`: compila un `.cpp` real con `BackendCpp`, lo carga con
  `dlopen`/`LoadLibrary` y ejecuta el ciclo + hot reload, comprueba que un
  segundo componente sobre el **mismo** fuente reutiliza el artefacto ya al
  día en vez de volver a enlazarlo, y que sin las cabeceras del motor el
  backend avisa con su propio mensaje (SKIP 77 solo si el
  sondeo del compilador del build falla; con MSVC el entorno del toolset lo
  obtiene `BackendCpp` del `vcvars64.bat`).
- `scripts-java-tests`: end-to-end del backend Java (JNI); solo con `FUNSHI_JAVA=ON`.
- `model-serialization-tests` (16): serialización binaria del componente `Model`
  (path con prefijo de longitud; regresión del core al cargar escenas con paths
  largos).
- `audio-tests` (16): `AudioEngine`/`AudioClipsManager` con `NullAudioBackend`
  (contrato de la cola de comandos: clips, handles, encolado, detención, volumen).
- `interface-component-tests` (16): serialización binaria del `InterfaceComponent`
  (ruta del `.html` + colocación): round-trip del formato nuevo y lectura del
  formato viejo (solo el nombre) sin consumir bytes de más, que desalinearía el
  resto del objeto.
- `barra-progreso-tests` (8): `BarraProgresoTexto` (formato de la barra de la
  ventana Estado, acotado ante `hecha > total` y `total == 0`).
- `webui-runtime-tests` (57): `WebUIRuntime`, `VistasWebUI` y `EstadoInterfaces`
  sobre el backend nulo: selección de backend con fallback al nulo ante un nombre
  desconocido, ciclo de vida de una vista sin motor web (`cargar`→`actualizar`→
  `render`→`descargar`), el gestor de vistas de una escena (alta, recarga al
  cambiar la ruta del `.html` o al cambiar el archivo en disco por `mtime`,
  redimensionado y descarte de las que dejan de estar activas), el estado por
  interfaz por rutas JSON Pointer y el intercambio del estado pendiente/olvido
  en el ciclo de vistas.
- `tema-tests` (28): `TemaEditor` (aplicación del perfil `Apariencia`): el acento
  llega a todos los roles de ImGui y el modo B/N deja la paleta monocroma.
- `comandos-tests` (77): undo/redo del editor (7 comandos, cadena de redo
  múltiple, límite de 50 entradas y descripción del comando aplicado).
- `manifiesto-assets-tests` (37): manifiesto `SceneAssets.json` (JSON round-trip de
  malla, texturas, script y las seis caras del cubemap, tolerancia a manifiestos
  corruptos y precedencia sobre el `.db`).
- `orquestador-estado-tests`: transiciones de Depuración/Juego, pausa, solicitud
  consumible de Reset, fin/Escape por estado, teclas del editor y atajos frente
  a ImGui.
- `escena-serializacion-tests` (151): round-trip completo de escena (guardar →
  recargar → conservar nombre, id y jerarquía), defensas del índice de escena
  (líneas corruptas saltadas con aviso, auto-sanado de hijos con id 0),
  apertura avisada de archivos `Binario` inexistente sin `std::remove()`
  destructivo, la reescritura de referencias al mover/renombrar (script,
  malla y textura bajo el prefijo reubicado, los demás intactos), el
  sanado de referencias rotas al cargar (una sola coincidencia del nombre bajo
  la raíz → repara; varias o ninguna → no adivina y avisa), la resolucion del
  componente `Model` con la matriz mundial del objeto y la separacion del
  evento estructural (`ComponentStructureChanged`) del de propiedad en el bus
  de escena: editar un campo no reconstruye los paneles del Inspector y
  agregar un componente crea unicamente el que falta. Cubre ademas el
  reparentado: preserva la pose mundial reescribiendo el local como
  `inverse(padre) × mundo` (los descendientes no saltan), refresca el cuerpo
  fisico del objeto y deja el local consistente en disco.
- `diagnostic-registry-tests` (26): `RegistroDiagnosticos`: registro de eventos
  de diagnóstico (nivel, módulo, timestamp UTC), historial JSONL con retención y
  lectura acotada, deduplicación por clave y el volcado de `RuntimeException`.
- `catalogo-componentes-tests` (52): la tabla única que declara, por componente,
  su dimensión (2D/3D), requisitos, creador y panel: descriptores completos y sin
  nombres repetidos, `ComponentFactory` creando desde el catálogo (tolerando el
  prefijo de MSVC y el alias retrocompatible), tipos desconocidos y requisitos
  incumplidos devolviendo nullptr, y el filtro por dimensión y los submenús.
- `tilemap-tests` (57): tileset JSON estilo Tiled (parseo, geometría de la
  grilla, flag de solidez por tile), capas y pintado por celda, horneado de la
  malla (una malla por tilemap, un quad por tile), round-trip binario y la shape
  compuesta del `TilemapCollider`.
- `interfaces-juego-tests` (9): la recolección de interfaces de juego (HUD):
  `InterfacesJuego::recolectarDeEscena` sobre la lista de objetos de la escena
  (lista nula o vacía sin resultados, objetos sin `InterfaceComponent` que no
  aportan, una escena con varias interfaces que las devuelve TODAS ordenadas por
  z y las interfaces sin ruta o inactivas omitidas sin cortar el recorrido).
- `prefab-tests` (82): sistema de Prefabs: round-trip guardar → instanciar
  conservando nombre, tag, jerarquía, componentes y `Transform`, más la caché de
  `PrefabLibrary` y la validación de archivos corruptos; la API de scripts
  `listarPrefabs` con storage estable (válido hasta la llamada siguiente).
- `terminal-emulator-tests` (24): cuadrícula VT del `TerminalEmulator`: CR/LF,
  posicionamiento y consulta del cursor (`H`/`DSR`), secuencias SGR (color,
  negrita, fondo) y borrado; CSI 1K con cursor en wrap pendiente, rejilla
  recién redimensionada a las dimensiones por defecto, y pantalla alternativa
  con resize (cursor restaurado acotado).
- `terminal-session-tests` (14): E/S real del pseudo-terminal (`forkpty` en
  POSIX, ConPTY en Windows): rechazos deterministas, arranque, eco del comando y
  su salida por el PTY, y que `detener()` no se cuelga ni pierde la salida
  emitida durante el cierre (SKIP 77 si ConPTY no está disponible).

Las **nueve** suites que dependen del engine (`comandos-tests`,
`escena-serializacion-tests`, `catalogo-componentes-tests`, `tilemap-tests`,
`interfaces-juego-tests`, `prefab-tests`, `manifiesto-assets-tests`,
`orquestador-estado-tests` y `tema-tests`) enlazan `funshi_engine` y solo se
registran con `BUILD_ENGINE=ON`. El resto compila sus fuentes directamente
(`interface-component-tests`, `barra-progreso-tests` y `webui-runtime-tests`
entre ellos) y corre también con `BUILD_ENGINE=OFF`.

La opción `BUILD_ENGINE=OFF` compila solo las pruebas (útil en CI y plataformas
sin las librerías gráficas), y `ENABLE_ASAN` (ON por defecto en Debug) activa
ASan+UBSan en GCC/Clang.

---

## 7. Flujo de datos principal

```text
main.cpp
  │
  ├─ input GLFW ──► EditorInput (callbacks + máquina de teclas) ──► cámara activa (movimiento continuo)
  │                                       ──► tecla E: toggleEditorInterfaces()
  │                                       ──► 1/T, 2/R, 3/U: operación del gizmo (y apagan la guía)
  │                                       ──► X/Y/Z: guía de eje del objeto seleccionado
  │                                       ──► Escape: en simulación termina; en editor
  │                                         vuelve al menú (regla en el orquestador)
  │
  ├─ GameScene::GUI()
  │     ├─ SceneMenuBarInterface ──► Depuración/Juego o Pausa/Reset/Terminar
  │     │                          ──► OrquestadorEstadoGUI
  │     ├─ SceneSelectedInterface ──► EditorController (crear/borrar/reparentar GO)
  │     │                         ──► EventBus.publish(ObjectCreated/Deleted/Selected)
  │     ├─ SceneObjectTree ──► selección y reparentado por drag & drop
  │     └─ GUIManager paneles
  │           └─ SettingsObjectInterface ──► EditorController (add/removeComponent,
  │                                          sync colliders, "Gizmo activo")
  │
  ├─ GameScene::update(dt)  (siempre, también con start en false: es donde viven los
  │                          flancos de transición; física/scripts se auto-gatean)
  │     ├─ transición editor→simulación: empuja la pose visual a los cuerpos Bullet
  │     ├─ transición simulación→edición: cola vacía, servicios de script
  │     │   desconectados, audio cortado y onStop a cada script
  │     │   (ambos modos restauran el estado previo a la simulación)
  │     ├─ si start y gizmo libre: PhysicsEngine::stepSimulation(dt)
  │     │               └─ btDiscreteDynamicsWorld::stepSimulation
  │     └─ scripts: IScriptBehaviour::onUpdate (si compilados)
  │
  └─ GameScene::gameScene()
        ├─ LightSystem::beginFrame() [solo CPU: datos de luz para el shader]
        ├─ pasada de la grilla (Grid + batch de líneas; solo fuera de Juego)
        ├─ dibujarGameObjects (MeshRenderer shader; único pipeline)
        ├─ marcadores de luz y cámara (wireframes auxiliares, batch de líneas)
        ├─ ImGuizmo::Manipulate sobre el GizmoTarget activo (objeto o collider)
        ├─ dibujarViewportsPrevios (render target de cámaras) + paneles ImGui
        ├─ pintarInterfacesJuego (overlay HUD; también corre sin GUI de editor)
        └─ pickObject con el mouse para seleccionar en el viewport
```

---

## 8. Patrones aplicados y límites actuales

| Patrón | Aplicación actual | Efecto arquitectónico |
|---|---|---|
| RAII / ownership explícito | `unique_ptr` en `GameScene`, `SceneRegistry`, `GUIManager`, `FileManager`; colliders y cuerpos Bullet gestionados por `Collider`/`RigidBody` | Propietarios claros y liberación automática. |
| Composite | Árbol n-ario de `GameObject` en `ArbolEnlazado` | Modela la jerarquía padre-hijo y las transformaciones anidadas. |
| Registry / Repository | `SceneRegistry` | Centraliza búsqueda, ownership y operaciones de escena. |
| Controller | `EditorController` | La GUI no coordina mutaciones directamente; la física se registra aquí. |
| Factory | `ComponentFactory`, `GameObjectFactory` | Reduce `new` concretos en GUI y serialización. |
| Facade | `PhysicsEngine` (PIMPL) y `FileManager` | Ocultan Bullet y el filesystem detrás de operaciones del dominio. |
| Adapter | `BulletPhysicsAdapter` | Permite sustituir el backend de física sin modificar `PhysicsEngine`. |
| Strategy | `IPhysicsBackend` | Contrato de backend inyectado por constructor (con nullptr se puede montar la fachada sin mundo). |
| MVP | Paquete `MenusGUI` (`MenuModel`/`MenuView`/`StartMenuPresenter`) | Lógica del menú testeable, independiente de ImGui/GLFW. |
| Observer | `EventBus` (token-based) | Desacopla notificaciones de escena y selección entre subsistemas. |
| State | `ApplicationStateMachine` | Explicita los modos principales de la aplicación. |
| Flyweight / Cache | `AssetManager` (meshes) y `TextureManager` (imágenes) | Recursos CPU compartidos por ruta (`AssetPath`) con loader inyectable y liberación por refcount. |

No están implementados todavía:
- `Command` para undo/redo.
- `Prototype` para duplicación y prefabs.
- `Visitor` para inspectores extensibles.

### 8.1 Resolución de la raíz de datos

Todos los datos del usuario (proyectos, escenas, configuraciones, `imgui.ini`)
cuelgan de una única raíz, `MotorGrafico`, y toda la jerarquía de rutas sale de
`ProjectPaths::directorioBase()`. Esa función **resuelve** la raíz en vez de
calcularla:

1. Parte de `<directorioEjecutable>/MotorGrafico`, la ubicación histórica.
2. Comprueba si puede **escribir** ahí. No basta con que el directorio exista y
   sea legible: se abre y se cierra un archivo de prueba, porque en
   `C:\Program Files` la carpeta es legible y aun así el proceso no puede crear
   nada dentro (la creó el instalador con los ACL de `Program Files`; el
   ejecutable no lleva manifiesto de elevación).
3. Si no puede, cae a la carpeta de datos del usuario: `%APPDATA%\FunshiEngineGL`
   en Windows, `$XDG_DATA_HOME/FunshiEngineGL` o `~/.local/share/FunshiEngineGL`
   en Linux y macOS. El instalador de Windows es por usuario
   (`PrivilegesRequired=lowest`), así que en una instalación normal este punto
   2 se cumple y la carpeta queda en `%LOCALAPPDATA%\Programs\FunshiEngineGL`:
   el paso 3 es la red que cubre una copia en `Program Files`, una carpeta en
   solo lectura o el motor lanzado como administrador.
4. Si tampoco hay carpeta de usuario utilizable, conserva la ruta histórica: es
   preferible una ruta conocida que falle de forma visible a dejar al motor sin
   rutas válidas.

La decisión se cachea por proceso (es una decisión de inicio, no por llamada) y
se expone con `ProjectPaths::datosEnRutaDeUsuario()`,
`directorioBaseOriginal()` y `migrarDatosDesdeRutaOriginal()`. Esta última copia
una sola vez lo que hubiera quedado en la ruta histórica, y solo si el destino
está vacío.

Por qué importa: sin este paso, las escrituras fallaban **en silencio** (nadie
comprobaba el retorno de `crearArchivo` ni de los demás), de modo que el motor
leía bien pero no podía guardar nada. Esa divergencia entre lo leído y lo que
se creía haber escrito desincronizaba el árbol de `SerializeField` de los
scripts y terminaba en un `std::bad_variant_access` al abrir el inspector.

Las rutas de assets dentro de la escena se guardan **relativas** a la raíz de
assets del proyecto (`EditorConfig::relativizarRuta`) y se absolutizan al
cargar, así que mover la raíz no invalida las escenas existentes.

---

## 9. Observaciones de arquitectura

- `SceneRegistry` establece ownership único mediante `std::vector<unique_ptr<GameObject>>`.
  GUI, selección, física y vistas lineales solo mantienen referencias raw no propietarias.
- `PhysicsEngine` es una fachada PIMPL con el backend inyectado (`IPhysicsBackend`):
  el header no expone Bullet y la fachada puede existir sin mundo (útil para pruebas
  y para el editor sin simulación).
- `GameScene` sigue siendo un coordinador amplio: mezcla ciclo de frame, render,
  GUI, física, gizmo y vistas previas. La extracción de `SceneRenderer`,
  `PhysicsSystem` y `ScriptSystem` aliviaría esa responsabilidad.
- `ApplicationStateMachine` explicita los estados, pero las transiciones se realizan
  desde `main.cpp` y conviven con flags de UI legados (`menuActivo`, `start`).
- `EventBus` transporta `GameObject*` crudos. Un consumidor no debe conservar esos
  punteros después de recibir `ObjectDeleted`. `EditorController::deleteGameObject`
  publica el evento **antes** de destruir el objeto, para que los observadores
  invaliden sus referencias a tiempo (fix de un use-after-free histórico).
- La manipulación del gizmo pausa `stepSimulation` mientras el usuario arrastra y
  la física solo corre en Depuración o Juego; el sync collider↔rigidbody↔objeto usa la matriz
  global compuesta del dueño, de modo que mover un collider no desincroniza el cuerpo.
- Las rutas de usuario (`<directorioEjecutable>/MotorGrafico`) están
  centralizadas en `EditorConfig` para la configuración y el layout, pero los assets
  del proyecto todavía se resuelven a mano; `PathUtils.h` comparte el separador
  (`PATH_SEP`) y la regla única de cotejo de rutas (`rutaBajo`/`esSeparadorPath`):
  en Windows `/` y `\` equivalen, que es lo que permite reescribir las
  referencias de la escena cuando el explorador trae rutas con `\` y
  `ProjectPaths` arma las suyas con `/`.
- La serialización binaria no tiene versionado ni validación formal de tamaños. Un
  cambio en la estructura de atributos invalida escenas guardadas.
- `SettingsObjectInterface` y algunos componentes todavía incluyen y construyen
  detalles concretos; el siguiente paso de desacoplamiento es completar el uso de
  `EditorController` y descriptors de componentes.
- El renderer es de un solo pipeline: `MeshRenderer` (malla en GPU + shaders)
  para mallas y el batch de líneas (GPU + shader de ancho en píxeles) para
  grilla, marcadores y gizmos de collider. No hay modo inmediato ni estado fijo:
  la iluminación viaja como uniforms y las transformaciones como uniforms de
  matriz.
- La ventana no tiene contexto gráfico: `Ventana::initVentana` la crea con
  `GLFW_NO_API` y deja la superficie en manos del backend de render, que la
  pide al instalarse. El motor arranca con el backend en blanco, de modo que
  sin backend instalado el editor abre, recorre la escena y omite las pasadas
  de dibujo en vez de fallar en el primer draw. El backend que se implemente
  es el que fija el dialecto de los shaders (el fuente deja de estar en el
  motor) y la versión de la API que exige.

---

## 10. Capas de la aplicación

```text
Aplicación:      main.cpp, MiAPP, ApplicationStateMachine, GUIManager, EditorConfig
Dominio:         GameScene, SceneRegistry, EditorController, GameObject, Component,
                 EventBus, FileManager, LightSystem
Infraestructura: BulletPhysicsAdapter, GLFW (Ventana) y el backend de render
                 intercambiable, Assimp (Modelos3D),
                 Binario/SceneSerializer, Script (dlopen/LoadLibrary), FileSystemWatcher
Dependencias:    ImGui, ImGuizmo, nlohmann/json, GLM (integradas o vendoriadas)
```

Relaciones clave:

```text
GUI → EditorController → SceneRegistry
                       └─ PhysicsEngine → IPhysicsBackend → BulletPhysicsAdapter → Bullet
                       └─ EventBus → suscriptores (SceneSelectedInterface, etc.)
Vistas del explorador → FileManager → GestorDeArchivos / FileSystemWatcher
SceneSerializer → SceneRegistry / EditorController
GameScene → coordina todos los subsistemas del frame
```

---

## 11. Pruebas y CI

- `tests/FileManagerTests.cpp`: construcción y re-resolución del árbol de archivos,
  operaciones de dominio (crear, renombrar, copiar, eliminar archivo y carpeta,
  búsqueda) y `FileSystemWatcher` (detección de cambios externos: inotify en
  Linux; en el resto, el polling de mtime con su semilla en el ctor y el
  intervalo inyectable para probarlo).
- `tests/EditorConfigTests.cpp`: round-trip del JSON (general, por proyecto y
  legacy `menu/*`), tolerancia a archivos ausentes/corruptos/parciales,
  escritura atómica (sin temporales colgados) y guardado diferido
  (`solicitarGuardadoGeneral`/`volcarGuardadoGeneral` con `kIntervaloEscritura`).
- `tests/AssetManagerTests.cpp` y `tests/TextureManagerTests.cpp`: caches
  Flyweight con loader artificial; validan rutas normalizadas, compartición y ciclo
  de vida de los recursos.
- `tests/EstructurasTests.cpp`: suite de las estructuras propias. Cubre la regresión
  de `ListaDE::addFirst`, la liberación de nodos en `remove`, el borrado de
  `ArbolEnlazado` (hoja, nodo interno y raíz), `PriorityListaDE`, la extracción
  ordenada de `MinHeap`/`MaxHeap`, la estabilidad de `ListMergeSort` y el árbol
  binario (addLeft/addRight, childsOf, preorden RID, borrado de hoja e interno).
- `tests/ModelSerializationTests.cpp`: serialización binaria del componente
  `Model` (path con prefijo de longitud). Cubre la regresión del core al cargar
  escenas: verifica que un path más largo que el buffer de lectura no desalinee
  el stream, además de round-trip corto/largo/vacío y archivos truncados.
- `tests/SceneSerializationTests.cpp`: round-trip completo de escena (guardar →
  recargar → conservar nombre, id y jerarquía), el nombre por defecto de los
  objetos nuevos (`GameObjectFactory` + `NombreUnico.h`, que no puede repetirse
  en el árbol), el guardado con el árbol vacío (archivo vacío **con aviso en el
  log**, nunca un trunc silencioso) y el reporte de fallos de apertura en
  `Binario` (sin `std::remove` destructivo previo, con valor de retorno `bool` y
  propagación en `saveEntity`/`loadEntity`). También verifica que el puntero a
  una cámara eliminada durante la UI se invalide antes de usarlo en el gizmo.
  Es la suite que faltaba: hasta ahora solo existía `ModelSerializationTests`,
  que cubre el componente `Model` aislado.
- `tests/TemaEditorTests.cpp`: aplicación del perfil `Apariencia` al estilo de ImGui
  (`TemaEditor::aplicarEstilo`, solo contexto de ImGui, sin pila gráfica). Cubre la
  regresión "el color de acento no llega a toda la interfaz": con un acento no azul
  verifica que **ningún** rol de la paleta conserve el azul de fábrica de Dear ImGui
  (`FrameBg` —campos y pista del slider—, `Tab`/`TabDimmed`, `Border`/`Separator`,
  `TableHeaderBg`, `TextLink`, `DragDropTarget`), que el acento por defecto mantenga
  el aspecto y las transparencias históricas, que un acento translúcido no apague
  los roles de primer plano (el alpha del perfil no participa del tema), que
  aplicar el mismo perfil dos veces sea idempotente y que el modo blanco y negro
  deje la paleta monocroma.
- `tests/ComandosTests.cpp`: los 8 comandos del editor (`CrearObjetoComando`,
  `BorrarObjetoComando`, `ReparentarComando`, `TransformComando`,
  `AgregarComponenteComando`, `QuitarComponenteComando`, `LimpiarEscenaComando`,
  `DuplicarObjetoComando`)
  con deshacer/rehacer, la cadena de redo múltiple, el límite del historial, la
  descripción que el historial devuelve para avisar en la barra de estado y el
  registro de cuerpos en fisica al borrar/duplicar/agregar/quitar.
- Los veinte targets compilan en cualquier plataforma y se ejecutan con `ctest`.
- `.github/workflows/ci.yml` compila el engine completo en Ubuntu (Release, sin
  ASan) y ejecuta las pruebas; además ejecuta las headless en
  Linux/Windows con `BUILD_ENGINE=OFF` y el backend Java en Ubuntu con JDK.
- `.github/workflows/release.yml` y `windows-release.yml` también ejecutan la
  suite (y `estructuras-tests`) al generar los instaladores por tag.

Los bugs de la Fase 2 (cámaras/vistas previas) y sus fixes están documentados en
[CAMARAS_VISTAS_PREVIAS.md](CAMARAS_VISTAS_PREVIAS.md).

---

## 12. Pendientes conocidos

- [ ] Sistema de animaciones.
- [ ] `CommandManager` para undo/redo.
- [ ] Filtros y navegación avanzada del historial de diagnósticos en el editor.
- [ ] Resolver IDs duplicados al crear objetos; limpiar binarios huérfanos al eliminar.
- [ ] Terminar los popups del inspector.
- [x] Prefabs y duplicación de objetos.
- [ ] Portabilidad de rutas de assets (centralizar `HOME` / rutas de Windows).
- [ ] Versionado y validación de la serialización binaria.
- [ ] Extraer `SceneRenderer`, `PhysicsSystem` y `ScriptSystem` de `GameScene`.
- [ ] Encapsular las estructuras internas de `SceneRegistry` (eliminar getters raw de compatibilidad).
- [ ] Vistas previas de cámara seleccionables con clic (hoy son pasivas).

---

## 13. Análisis: expresiones regulares (veredicto)

Se buscó uso de `std::regex` / `boost::regex` en el código del motor (búsqueda
por `regex|regular_expression` en `src/`, tests y herramientas). **No existe
ningún consumidor de expresiones regulares** (la única aparición es un comentario
dentro de `ImGui/imgui.cpp`, código de terceros), y los puntos donde un motor
suele necesitarlas están resueltos con otras herramientas:

- **Rutas de assets**: `AssetPath` normaliza con `std::filesystem` (carga sobre
  `std::path` con separadores y síndromes `.`/`..` del propio API), sin patrones.
- **Extensión de archivos**: predicados directos (`entrada.path().extension()`,
  comparaciones de `std::string`) en `GestorDeArchivos` y `AssetManager`.
- **Configuración del editor**: `ConfigPersistence` (invocada por la fachada
  `EditorConfig`) parsea JSON con **nlohmann/json** (librería ya vendoriada en
  `External/`); las claves se validan por acceso estructurado, no por patrones.
- **Serialización de escenas**: `SceneSerializer` usa un formato binario en
  preorden con marcadores literales `=>`/`<=`, decididos con comparaciones de
  `std::string` exactas (búsqueda del look-ahead), no con matching; el patrón
  de las líneas del índice (`ObjectN<entero>.db`) se valida a mano, sin
  regex.
- **Nombres de tipos**: `ComponentFactory`/`TypeUtils` comparan huesos
  exactamente (`"CameraComponent"` y alias `"Camera"`), sin patrones.

**Conclusión**: no se incorpora ninguna dependencia de regex. Las necesidades
actuales de parsing son estructuradas (filesystem, JSON, binario, comparación
literal) y una implementación con `std::regex` añadiría coste de compilación y
de runtime sin cubrir ningún caso real. Si más adelante se necesitara validar
nombres de archivos o campos con patrones (p. ej. prefijos de prefabs), la
decisión sería revistarla puntualmente con `std::regex` (sin dependencia extra).

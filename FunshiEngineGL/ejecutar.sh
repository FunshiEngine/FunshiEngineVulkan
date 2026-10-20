#!/usr/bin/env bash
# Lanza FunshiEngineVK SIN abrir una terminal atras de la ventana:
# compila si hace falta y ejecuta el editor desacoplado (nohup, en segundo
# plano). Toda la salida de consola la captura la propia app en logs/.
#
# SOLO LINUX: en Windows el binario es build/FunshiEngineVK.exe (con .exe) y
# no hay forma portatil de lanzarlo desacoplado con el mismo truco; en Windows
# lanzar build\FunshiEngineVK.exe con la carpeta de trabajo en la raiz del
# proyecto (ver DOCUMENTACION.md, seccion 1.1).
set -e
cd "$(dirname "$0")"

echo ">>> Compilando (si hay cambios)..."
cmake --build build --target FunshiEngineVK -j"$(nproc)"

BIN="build/FunshiEngineVK"
if [ ! -x "$BIN" ]; then
    echo "ERROR: no se encontro el ejecutable $BIN"
    exit 1
fi

nohup "$BIN" >/dev/null 2>&1 &
echo ">>> FunshiEngineVK lanzado (pid $!). Logs en: $(dirname "$BIN")/logs/"
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
#ifndef TERMINAL_EMULATOR_H
#define TERMINAL_EMULATOR_H

#include <string>
#include <vector>

class TerminalEmulator {
public:
    struct Celda {
        std::string texto = " ";
        int primerPlano = 7;
        int fondo = 0;
        bool negrita = false;
        bool fondoDefinido = false;
    };

    void redimensionar(unsigned columnas, unsigned filas);
    void procesar(const std::string& bytes);
    std::string tomarRespuestas();
    const std::vector<Celda>& celdas() const { return pantalla_; }
    unsigned columnas() const { return columnas_; }
    unsigned filas() const { return filas_; }
    unsigned cursorX() const { return cursorX_; }
    unsigned cursorY() const { return cursorY_; }
    bool cursorVisible() const { return cursorVisible_; }
    bool enPantallaAlternativa() const { return alternativaActiva_; }

private:
    enum class Estado { Texto, Escape, Csi, Osc, OscEscape };
    unsigned columnas_ = 80;
    unsigned filas_ = 24;
    unsigned cursorX_ = 0;
    unsigned cursorY_ = 0;
    unsigned cursorPrincipalX_ = 0;
    unsigned cursorPrincipalY_ = 0;
    unsigned margenSuperior_ = 0;
    unsigned margenInferior_ = 23;
    int primerPlano_ = 7;
    int fondo_ = 0;
    bool negrita_ = false;
    bool fondoDefinido_ = false;
    bool cursorVisible_ = true;
    bool wrap_ = true;
    bool alternativaActiva_ = false;
    unsigned bytesUtf8Restantes_ = 0;
    std::string secuenciaUtf8_;
    Estado estado_ = Estado::Texto;
    std::string secuencia_;
    std::vector<Celda> pantalla_;
    std::vector<Celda> pantallaPrincipal_;
    std::string respuestas_;

    void poner(unsigned char byte);
    void ajustarCursorEscritura();
    void ejecutarCsi(char final);
    void limpiarLinea(unsigned modo);
    void limpiarPantalla(unsigned modo);
    void desplazarArriba(unsigned lineas);
    void desplazarAbajo(unsigned lineas);
    // Índice lineal acotado a la rejilla: ningún acceso a celdas puede salir
    // del vector aunque un CSI llegue con coordenadas pasadas de borde.
    unsigned indice(unsigned x, unsigned y) const;
    unsigned parametro(std::size_t indice, unsigned defecto) const;
    std::vector<unsigned> parametros() const;
};

#endif

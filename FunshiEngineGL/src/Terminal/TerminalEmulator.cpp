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
#include "TerminalEmulator.h"

#include <algorithm>
#include <cstdlib>

namespace {
TerminalEmulator::Celda celdaVacia() { return {}; }
}

void TerminalEmulator::redimensionar(unsigned columnas, unsigned filas) {
    columnas = std::max(1u, std::min(columnas, 500u));
    filas = std::max(1u, std::min(filas, 300u));
    // La salida temprana solo aplica si la rejilla ademas esta almacenada: el
    // estado por defecto declara 80x24 con el vector vacio, y un
    // redimensionar(80, 24) sobre ese estado debe asignar la matriz, no
    // devolver sin crearla (los accesos a celdas asumen size == filas*columnas).
    if (columnas == columnas_ && filas == filas_ &&
        pantalla_.size() == static_cast<std::size_t>(columnas) * filas)
        return;
    std::vector<Celda> nueva(static_cast<std::size_t>(columnas) * filas);
    const unsigned conservarColumnas = std::min(columnas, columnas_);
    const unsigned conservarFilas = std::min(filas, filas_);
    if (!pantalla_.empty()) {
        for (unsigned y = 0; y < conservarFilas; ++y)
            for (unsigned x = 0; x < conservarColumnas; ++x)
                nueva[static_cast<std::size_t>(y) * columnas + x] =
                    pantalla_[static_cast<std::size_t>(y) * columnas_ + x];
    }
    if (!pantallaPrincipal_.empty() && alternativaActiva_) {
        std::vector<Celda> principal(static_cast<std::size_t>(columnas) * filas);
        for (unsigned y = 0; y < conservarFilas; ++y)
            for (unsigned x = 0; x < conservarColumnas; ++x)
                principal[static_cast<std::size_t>(y) * columnas + x] =
                    pantallaPrincipal_[static_cast<std::size_t>(y) * columnas_ + x];
        pantallaPrincipal_.swap(principal);
    }
    columnas_ = columnas;
    filas_ = filas;
    pantalla_.swap(nueva);
    margenSuperior_ = 0;
    margenInferior_ = filas_ - 1;
    cursorX_ = std::min(cursorX_, columnas_ - 1);
    cursorY_ = std::min(cursorY_, filas_ - 1);
    // El cursor de la pantalla principal se restaura tal cual al salir de la
    // alternativa (1049l); sin esta cota puede quedar fuera de la rejilla
    // redimensionada y los CSI posteriores saldrían del vector.
    cursorPrincipalX_ = std::min(cursorPrincipalX_, columnas_ - 1);
    cursorPrincipalY_ = std::min(cursorPrincipalY_, filas_ - 1);
}

std::string TerminalEmulator::tomarRespuestas() {
    std::string resultado;
    resultado.swap(respuestas_);
    return resultado;
}

void TerminalEmulator::procesar(const std::string& bytes) {
    for (unsigned char byte : bytes) {
        switch (estado_) {
            case Estado::Texto:
                if (byte == 0x1b) estado_ = Estado::Escape;
                else poner(byte);
                break;
            case Estado::Escape:
                if (byte == '[') {
                    secuencia_.clear();
                    estado_ = Estado::Csi;
                } else if (byte == ']') {
                    estado_ = Estado::Osc;
                } else if (byte == 'c') {
                    pantalla_.assign(pantalla_.size(), celdaVacia());
                    cursorX_ = cursorY_ = 0;
                    primerPlano_ = 7; fondo_ = 0; negrita_ = false;
                    estado_ = Estado::Texto;
                } else {
                    estado_ = Estado::Texto;
                }
                break;
            case Estado::Csi:
                if (byte >= 0x40 && byte <= 0x7e) {
                    ejecutarCsi(static_cast<char>(byte));
                    secuencia_.clear();
                    estado_ = Estado::Texto;
                } else if (secuencia_.size() < 128) {
                    secuencia_.push_back(static_cast<char>(byte));
                } else {
                    secuencia_.clear(); estado_ = Estado::Texto;
                }
                break;
            case Estado::Osc:
                if (byte == 0x07) estado_ = Estado::Texto;
                else if (byte == 0x1b) estado_ = Estado::OscEscape;
                break;
            case Estado::OscEscape:
                estado_ = byte == '\\' ? Estado::Texto : Estado::Osc;
                break;
        }
    }
}

void TerminalEmulator::poner(unsigned char byte) {
    if (bytesUtf8Restantes_ > 0) {
        if ((byte & 0xc0) == 0x80) {
            secuenciaUtf8_.push_back(static_cast<char>(byte));
            if (--bytesUtf8Restantes_ == 0) {
                ajustarCursorEscritura();
                Celda& celda = pantalla_[indice(cursorX_, cursorY_)];
                celda.texto = secuenciaUtf8_;
                celda.primerPlano = primerPlano_;
                celda.fondo = fondo_;
                celda.negrita = negrita_;
                celda.fondoDefinido = fondoDefinido_;
                ++cursorX_;
            }
            return;
        }
        bytesUtf8Restantes_ = 0;
        secuenciaUtf8_.clear();
    }
    if (byte == '\r') { cursorX_ = 0; return; }
    if (byte == '\n') {
        if (cursorY_ == margenInferior_) desplazarArriba(1);
        else cursorY_ = std::min(cursorY_ + 1, filas_ - 1);
        return;
    }
    if (byte == '\b') { if (cursorX_) --cursorX_; return; }
    if (byte == '\t') { cursorX_ = std::min(((cursorX_ / 8) + 1) * 8, columnas_ - 1); return; }
    if (byte < 0x20 || byte == 0x7f) return;
    if (byte >= 0xc2 && byte <= 0xf4) {
        bytesUtf8Restantes_ = byte <= 0xdf ? 1u : (byte <= 0xef ? 2u : 3u);
        secuenciaUtf8_.assign(1, static_cast<char>(byte));
        return;
    }
    if (byte >= 0x80) return;
    ajustarCursorEscritura();
    Celda& celda = pantalla_[indice(cursorX_, cursorY_)];
    celda.texto.assign(1, static_cast<char>(byte));
    celda.primerPlano = primerPlano_;
    celda.fondo = fondo_;
    celda.negrita = negrita_;
    celda.fondoDefinido = fondoDefinido_;
    ++cursorX_;
}

void TerminalEmulator::ajustarCursorEscritura() {
    if (cursorX_ < columnas_) return;
    if (!wrap_) { cursorX_ = columnas_ - 1; return; }
    cursorX_ = 0;
    if (cursorY_ == margenInferior_) desplazarArriba(1);
    else cursorY_ = std::min(cursorY_ + 1, filas_ - 1);
}

unsigned TerminalEmulator::indice(unsigned x, unsigned y) const {
    return std::min(y, filas_ - 1) * columnas_ + std::min(x, columnas_ - 1);
}

std::vector<unsigned> TerminalEmulator::parametros() const {
    std::vector<unsigned> resultado;
    std::size_t inicio = 0;
    while (inicio <= secuencia_.size()) {
        const std::size_t fin = secuencia_.find(';', inicio);
        std::string parte = secuencia_.substr(inicio, fin == std::string::npos ? fin : fin - inicio);
        if (!parte.empty() && (parte[0] == '?' || parte[0] == '>')) parte.erase(0, 1);
        resultado.push_back(parte.empty() ? 0u : static_cast<unsigned>(std::strtoul(parte.c_str(), nullptr, 10)));
        if (fin == std::string::npos) break;
        inicio = fin + 1;
    }
    return resultado;
}

unsigned TerminalEmulator::parametro(std::size_t indice, unsigned defecto) const {
    const auto valores = parametros();
    return indice >= valores.size() || valores[indice] == 0 ? defecto : valores[indice];
}

void TerminalEmulator::ejecutarCsi(char final) {
    const bool privado = !secuencia_.empty() && (secuencia_[0] == '?' || secuencia_[0] == '>');
    const auto valores = parametros();
    const unsigned n = parametro(0, 1);
    switch (final) {
        case 'A': cursorY_ = cursorY_ > n ? cursorY_ - n : 0; break;
        case 'B': cursorY_ = std::min(cursorY_ + n, filas_ - 1); break;
        case 'C': cursorX_ = std::min(cursorX_ + n, columnas_ - 1); break;
        case 'D': cursorX_ = cursorX_ > n ? cursorX_ - n : 0; break;
        case 'E': cursorY_ = std::min(cursorY_ + n, filas_ - 1); cursorX_ = 0; break;
        case 'F': cursorY_ = cursorY_ > n ? cursorY_ - n : 0; cursorX_ = 0; break;
        case 'G': cursorX_ = std::min(parametro(0, 1) - 1, columnas_ - 1); break;
        case 'H': case 'f': cursorY_ = std::min(parametro(0, 1) - 1, filas_ - 1); cursorX_ = std::min(parametro(1, 1) - 1, columnas_ - 1); break;
        case 'J': limpiarPantalla(valores.empty() ? 0 : valores[0]); break;
        case 'K': limpiarLinea(valores.empty() ? 0 : valores[0]); break;
        case 'S': desplazarArriba(n); break;
        case 'T': desplazarAbajo(n); break;
        case 'm':
            if (valores.empty()) { primerPlano_ = 7; fondo_ = 0; fondoDefinido_ = false; negrita_ = false; }
            for (std::size_t i = 0; i < valores.size(); ++i) {
                const unsigned v = valores[i];
                if (v == 0) { primerPlano_ = 7; fondo_ = 0; fondoDefinido_ = false; negrita_ = false; }
                else if (v == 1) negrita_ = true;
                else if (v == 22) negrita_ = false;
                else if (v >= 30 && v <= 37) primerPlano_ = static_cast<int>(v - 30);
                else if (v >= 90 && v <= 97) primerPlano_ = static_cast<int>(v - 90 + 8);
                else if (v >= 40 && v <= 47) { fondo_ = static_cast<int>(v - 40); fondoDefinido_ = true; }
                else if (v >= 100 && v <= 107) { fondo_ = static_cast<int>(v - 100 + 8); fondoDefinido_ = true; }
                else if (v == 39) primerPlano_ = 7;
                else if (v == 49) { fondo_ = 0; fondoDefinido_ = false; }
            }
            break;
        case 'n':
            if (!privado && !valores.empty() && valores[0] == 5) respuestas_ += "\x1b[0n";
            else if (!valores.empty() && valores[0] == 6) {
                respuestas_ += privado ? "\x1b[?" : "\x1b[";
                respuestas_ += std::to_string(cursorY_ + 1) + ";" + std::to_string(cursorX_ + 1) + "R";
            }
            break;
        case 'c': respuestas_ += "\x1b[?1;2c"; break;
        case 'r': margenSuperior_ = std::min(parametro(0, 1) - 1, filas_ - 1); margenInferior_ = std::min(parametro(1, filas_) - 1, filas_ - 1); if (margenInferior_ < margenSuperior_) { margenSuperior_ = 0; margenInferior_ = filas_ - 1; } cursorX_ = cursorY_ = 0; break;
        case 'h': case 'l':
            if (privado) for (unsigned v : valores) {
                if (v == 25) cursorVisible_ = final == 'h';
                if (v == 7) wrap_ = final == 'h';
                if (v == 1049 || v == 47 || v == 1047) {
                    if (final == 'h' && !alternativaActiva_) { pantallaPrincipal_ = pantalla_; cursorPrincipalX_ = cursorX_; cursorPrincipalY_ = cursorY_; pantalla_.assign(pantalla_.size(), celdaVacia()); alternativaActiva_ = true; cursorX_ = cursorY_ = 0; }
                    else if (final == 'l' && alternativaActiva_) { pantalla_ = pantallaPrincipal_; pantallaPrincipal_.clear(); alternativaActiva_ = false; cursorX_ = cursorPrincipalX_; cursorY_ = cursorPrincipalY_; }
                }
            }
            break;
        case 's': break;
        case 'u': break;
        default: break;
    }
}

void TerminalEmulator::limpiarLinea(unsigned modo) {
    const unsigned desde = modo == 1 || modo == 2 ? 0 : cursorX_;
    // En wrap pendiente cursorX_ vale columnas_: el extremo derecho se acota
    // para no llegar al tamaño de la rejilla (la posicion de cursor como
    // indice solo es valida con cursorX_ < columnas_).
    const unsigned hasta = modo == 0 || modo == 2
                               ? columnas_ - 1
                               : std::min(cursorX_, columnas_ - 1);
    for (unsigned x = desde; x <= hasta; ++x)
        pantalla_[indice(x, cursorY_)] = celdaVacia();
}

void TerminalEmulator::limpiarPantalla(unsigned modo) {
    if (modo == 2 || modo == 3) pantalla_.assign(pantalla_.size(), celdaVacia());
    else if (modo == 0) {
        limpiarLinea(0);
        for (unsigned y = cursorY_ + 1; y < filas_; ++y)
            for (unsigned x = 0; x < columnas_; ++x)
                pantalla_[indice(x, y)] = celdaVacia();
    } else if (modo == 1) {
        limpiarLinea(1);
        for (unsigned y = 0; y < cursorY_; ++y)
            for (unsigned x = 0; x < columnas_; ++x)
                pantalla_[indice(x, y)] = celdaVacia();
    }
}

void TerminalEmulator::desplazarArriba(unsigned lineas) {
    lineas = std::min(lineas, margenInferior_ - margenSuperior_ + 1);
    for (unsigned n = 0; n < lineas; ++n) {
        for (unsigned y = margenSuperior_; y < margenInferior_; ++y)
            for (unsigned x = 0; x < columnas_; ++x)
                pantalla_[static_cast<std::size_t>(y) * columnas_ + x] = pantalla_[static_cast<std::size_t>(y + 1) * columnas_ + x];
        for (unsigned x = 0; x < columnas_; ++x) pantalla_[static_cast<std::size_t>(margenInferior_) * columnas_ + x] = celdaVacia();
    }
}

void TerminalEmulator::desplazarAbajo(unsigned lineas) {
    lineas = std::min(lineas, margenInferior_ - margenSuperior_ + 1);
    for (unsigned n = 0; n < lineas; ++n) {
        for (unsigned y = margenInferior_; y > margenSuperior_; --y)
            for (unsigned x = 0; x < columnas_; ++x)
                pantalla_[static_cast<std::size_t>(y) * columnas_ + x] = pantalla_[static_cast<std::size_t>(y - 1) * columnas_ + x];
        for (unsigned x = 0; x < columnas_; ++x) pantalla_[static_cast<std::size_t>(margenSuperior_) * columnas_ + x] = celdaVacia();
    }
}

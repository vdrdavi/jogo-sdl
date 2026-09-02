#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "gfx3d/Math3D.hpp"

namespace jogo {

/// Malha low poly com uma cor por face (sombreamento flat, sem texturas).
struct Mesh {
    struct Face {
        int a{0};
        int b{0};
        int c{0};
        SDL_FColor cor{1.0f, 1.0f, 1.0f, 1.0f};
    };

    std::vector<Vec3> vertices;
    std::vector<Face> faces;
};

/// Garante que toda face aponte para fora, comparando a normal com a direcao
/// do centro da malha ate o centro da face. Evita depender da ordem em que os
/// indices foram digitados.
void orientarFacesParaFora(Mesh& malha);

/// Caca triangular low poly, com o nariz em -Z (a frente de Mat3).
Mesh criarNaveLowPoly();

/// Rocha irregular a partir de uma semente. Os vertices sao normalizados para
/// o maior raio valer 1, entao a escala com que a malha e desenhada ja e o raio
/// da esfera de colisao.
Mesh criarAsteroideLowPoly(Uint32 semente);

}  // namespace jogo

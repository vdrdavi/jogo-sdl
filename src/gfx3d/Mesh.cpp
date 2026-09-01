#include "gfx3d/Mesh.hpp"

namespace jogo {
namespace {

constexpr SDL_FColor kCasco{0.46f, 0.56f, 0.72f, 1.0f};
constexpr SDL_FColor kCascoEscuro{0.18f, 0.22f, 0.34f, 1.0f};
constexpr SDL_FColor kCabine{0.30f, 0.95f, 1.00f, 1.0f};
constexpr SDL_FColor kMotor{1.0f, 0.55f, 0.25f, 1.0f};

}  // namespace

void orientarFacesParaFora(Mesh& malha) {
    if (malha.vertices.empty()) {
        return;
    }

    Vec3 centro{};
    for (const Vec3& v : malha.vertices) {
        centro += v;
    }
    centro = centro * (1.0f / static_cast<float>(malha.vertices.size()));

    for (Mesh::Face& face : malha.faces) {
        const Vec3& a = malha.vertices[static_cast<std::size_t>(face.a)];
        const Vec3& b = malha.vertices[static_cast<std::size_t>(face.b)];
        const Vec3& c = malha.vertices[static_cast<std::size_t>(face.c)];

        const Vec3 normal = cross(b - a, c - a);
        const Vec3 centroDaFace = (a + b + c) * (1.0f / 3.0f);
        if (dot(normal, centroDaFace - centro) < 0.0f) {
            std::swap(face.b, face.c);
        }
    }
}

Mesh criarNaveLowPoly() {
    Mesh nave;
    // O nariz fica em -Z: a mesma convencao de "frente" usada por Mat3.
    nave.vertices = {
        Vec3{0.0f, 0.00f, -2.60f},    // 0 nariz
        Vec3{-1.70f, -0.10f, 1.10f},  // 1 ponta da asa esquerda
        Vec3{1.70f, -0.10f, 1.10f},   // 2 ponta da asa direita
        Vec3{0.0f, 0.60f, 0.40f},     // 3 dorso
        Vec3{0.0f, -0.42f, 0.30f},    // 4 ventre
        Vec3{0.0f, 0.12f, 1.55f},     // 5 cauda
        Vec3{-0.42f, 0.30f, -0.55f},  // 6 cabine esquerda
        Vec3{0.42f, 0.30f, -0.55f},   // 7 cabine direita
    };

    nave.faces = {
        // dorso, com a cabine destacada perto do nariz
        {0, 6, 3, kCasco},
        {0, 3, 7, kCasco},
        {0, 7, 6, kCabine},
        {6, 1, 3, kCasco},
        {7, 3, 2, kCasco},
        // ventre
        {0, 4, 1, kCascoEscuro},
        {0, 2, 4, kCascoEscuro},
        {1, 4, 5, kCascoEscuro},
        {2, 5, 4, kCascoEscuro},
        // traseira: os motores
        {1, 5, 3, kCasco},
        {2, 3, 5, kCasco},
        {3, 5, 4, kMotor},
    };

    orientarFacesParaFora(nave);
    return nave;
}

}  // namespace jogo

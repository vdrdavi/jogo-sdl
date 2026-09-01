#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "gfx3d/Renderer3D.hpp"

namespace jogo {

/// Campo de estrelas procedural. As estrelas nascem de um gerador com semente
/// dentro de um cubo e sao reposicionadas por wrap em torno da camera, o que da
/// um campo infinito com memoria constante.
class Starfield {
public:
    void gerar(Uint32 semente, int quantidade, float raio);

    /// Mantem o cubo centrado na camera; chamar sempre que a nave se mover.
    void centralizar(Vec3 posicao);

    /// `velocidade` alonga o rastro das estrelas; `tempo` faz o cintilar.
    void desenhar(SDL_Renderer* renderer, const Renderer3D& cena, Vec3 velocidade, float tempo);

    float raio() const { return raio_; }

private:
    struct Estrela {
        Vec3 posicao{};
        SDL_FColor cor{1.0f, 1.0f, 1.0f, 1.0f};
        float brilho{1.0f};
        float fase{0.0f};
    };

    std::vector<Estrela> estrelas_;
    std::vector<SDL_Vertex> buffer_;
    float raio_{320.0f};
};

}  // namespace jogo

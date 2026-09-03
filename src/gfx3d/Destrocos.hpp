#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "core/Aleatorio.hpp"
#include "gfx3d/Mesh.hpp"
#include "gfx3d/Renderer3D.hpp"

namespace jogo {

/// O que sobra de uma malha depois que ela se abre: um caco por face, mais um
/// punhado de faiscas.
///
/// A nave nao e trocada por outro modelo de "nave quebrada". Cada face vira um
/// **tetraedro** -- os tres vertices da face mais o centro da malha --, e por
/// isso os cacos, juntos e parados, ainda sao exatamente a nave: no instante da
/// destruicao nada pisca nem troca de forma, so comeca a se afastar. O
/// tetraedro tambem resolve um problema do rasterizador: um triangulo solto e
/// descartado quando visto por tras (culling por normal) e piscaria a cada
/// tombo; um solido fechado nunca some.
///
/// Isto e apresentacao, nao simulacao: os cacos nao colidem com nada, nao
/// param, e ninguem depende deles. Quem os guarda e a cena que os desenha.
class Destrocos {
public:
    /// Estilhaca `malha` como ela esta no mundo (posicao/rotacao/escala em que
    /// era desenhada). `velocidade` e a da nave no instante: os cacos herdam o
    /// movimento que ela tinha, senao a explosao ficaria para tras da camera.
    void gerar(const Mesh& malha, Vec3 posicao, const Mat3& rotacao, float escala,
               Vec3 velocidade, Uint32 semente);

    /// Passo fixo: os cacos seguem em linha reta (no vazio nada os segura) e
    /// tombam; as faiscas envelhecem.
    void atualizar(float dt);

    void submeter(Renderer3D& cena) const;

    /// As faiscas nao sao malhas: sao brilhos aditivos projetados na tela, do
    /// mesmo jeito que o escapamento do motor.
    void desenharFaiscas(SDL_Renderer* renderer, const Renderer3D& cena) const;

    bool vazio() const { return cacos_.empty(); }

private:
    struct Caco {
        Mesh malha;
        Vec3 posicao{};
        Vec3 velocidade{};
        /// Orientacao que o caco tinha na nave; o tombo e aplicado por cima
        /// dela, entao no primeiro quadro o conjunto ainda e a nave inteira.
        Mat3 base{};
        float yaw{0.0f};
        float pitch{0.0f};
        float giroYaw{0.0f};
        float giroPitch{0.0f};
    };

    struct Faisca {
        Vec3 posicao{};
        Vec3 velocidade{};
        float vida{0.0f};
        float duracao{1.0f};
        SDL_FColor cor{1.0f, 0.8f, 0.4f, 1.0f};
    };

    std::vector<Caco> cacos_;
    std::vector<Faisca> faiscas_;
    Aleatorio rng_{0x51ED5EEDu};
};

}  // namespace jogo

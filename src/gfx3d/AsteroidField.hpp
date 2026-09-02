#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "core/Aleatorio.hpp"
#include "gfx3d/Mesh.hpp"
#include "gfx3d/Renderer3D.hpp"

namespace jogo {

/// Campo de asteroides com o mesmo truque do Starfield: as rochas vivem em um
/// cubo que envolve a nave por wrap, o que da um campo infinito com memoria
/// constante. Para a colisao cada rocha e apenas uma esfera -- as malhas sao
/// normalizadas com raio 1, entao a escala com que sao desenhadas ja e o raio.
class AsteroidField {
public:
    struct Asteroide {
        Vec3 posicao{};
        float raio{1.0f};
        // Tombo constante, guardado como angulos: recompor a matriz a cada
        // quadro nao acumula erro, ao contrario de ir multiplicando rotacoes.
        float yaw{0.0f};
        float pitch{0.0f};
        float giroYaw{0.0f};
        float giroPitch{0.0f};
        std::size_t malha{0};
    };

    /// `raio` e a meia-aresta do cubo e tambem o alcance de desenho.
    void gerar(Uint32 semente, int quantidade, float raio);

    /// Faz as rochas tombarem; passo fixo.
    void atualizar(float dt);

    /// Mantem o cubo centrado na nave; chamar sempre que ela se mover.
    void centralizar(Vec3 posicao);

    void submeter(Renderer3D& cena) const;

    /// Indice da primeira rocha que encosta na esfera dada, ou -1.
    int colisao(Vec3 posicao, float raio) const;

    /// Manda a rocha para outro canto do cubo, longe de `referencia`. E o que
    /// sobra de uma rocha atingida: o campo nunca perde nem ganha pedras.
    void reposicionar(int indice, Vec3 referencia);

    const Asteroide& asteroide(int indice) const {
        return asteroides_[static_cast<std::size_t>(indice)];
    }
    int quantidade() const { return static_cast<int>(asteroides_.size()); }
    float raio() const { return raio_; }

private:
    /// Ponto no cubo em torno de `centro`, a pelo menos `minimo` dele.
    Vec3 sortear(Vec3 centro, float minimo);

    std::vector<Mesh> malhas_;
    std::vector<Asteroide> asteroides_;
    Aleatorio rng_{0x9E3779B9u};
    float raio_{160.0f};
};

}  // namespace jogo

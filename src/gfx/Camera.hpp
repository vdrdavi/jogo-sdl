#pragma once

#include <SDL3/SDL.h>

namespace jogo {

/// Camera 2D: converte entre coordenadas de mundo e as coordenadas logicas do
/// renderer (as mesmas usadas por SDL_SetRenderLogicalPresentation).
class Camera {
public:
    Camera(float larguraLogica, float alturaLogica)
        : viewport_{larguraLogica, alturaLogica} {}

    SDL_FPoint posicao() const { return posicao_; }
    void definirPosicao(SDL_FPoint p) { posicao_ = p; }

    float zoom() const { return zoom_; }
    void definirZoom(float z);

    SDL_FPoint viewport() const { return viewport_; }

    SDL_FPoint mundoParaTela(SDL_FPoint mundo) const;
    SDL_FPoint telaParaMundo(SDL_FPoint tela) const;
    /// Retangulo do mundo atualmente visivel (util para culling).
    SDL_FRect areaVisivel() const;

    /// Segue um alvo com suavizacao exponencial, estavel em passo fixo.
    void seguir(SDL_FPoint alvo, float dt, float suavidade = 8.0f);

    /// Impede que a camera mostre fora dos limites do mundo.
    void limitarA(const SDL_FRect& limitesDoMundo);

private:
    SDL_FPoint posicao_{0.0f, 0.0f};  // centro da camera, em mundo
    SDL_FPoint viewport_{640.0f, 360.0f};
    float zoom_{1.0f};
};

}  // namespace jogo

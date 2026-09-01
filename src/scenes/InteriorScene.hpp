#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "audio/Audio.hpp"
#include "gfx/Camera.hpp"
#include "gfx/Sprite.hpp"
#include "scene/Scene.hpp"

namespace jogo {

/// Interior da nave: o jogador anda pelo convés e, no painel de pilotagem da
/// parede superior, aperta E para assumir os controles (abre a FlightScene).
class InteriorScene : public Scene {
public:
    InteriorScene();

    void aoEntrar(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

private:
    static constexpr int kTile = 16;
    static constexpr int kLarguraMapa = 20;  // em tiles
    static constexpr int kAlturaMapa = 13;
    static constexpr float kVelocidade = 96.0f;

    // Tiles do atlas textures/interior.png
    enum Tile : Uint8 { kPiso = 0, kGrade = 1, kPisoEscuro = 2, kParede = 3, kFaixa = 4, kJanela = 5 };

    void gerarConves();
    bool solido(int tx, int ty) const;
    /// Move o jogador resolvendo colisao contra os tiles, um eixo por vez.
    void moverComColisao(SDL_FPoint deslocamento);
    SDL_FRect caixaDoJogador(SDL_FPoint centro) const;
    SDL_FRect limitesDoMundo() const;
    bool pertoDoConsole() const;

    std::vector<Uint8> mapa_;
    Camera camera_;

    SDL_FPoint posicao_{0.0f, 0.0f};
    SDL_FPoint posicaoAnterior_{0.0f, 0.0f};
    SDL_FlipMode espelho_{SDL_FLIP_NONE};
    float tempo_{0.0f};

    SDL_FRect console_{0.0f, 0.0f, 48.0f, 32.0f};
    SDL_FRect zonaDoConsole_{0.0f, 0.0f, 0.0f, 0.0f};

    Sprite jogador_;
    Sprite consoleSprite_;
    SDL_Texture* tiles_{nullptr};
    Audio::SomId somConfirmar_{0};
};

}  // namespace jogo

#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "audio/Audio.hpp"
#include "gfx/Camera.hpp"
#include "gfx/Sprite.hpp"
#include "scene/Scene.hpp"
#include "sim/Flight.hpp"

namespace jogo {

/// Interior da nave: o jogador anda pelo convés e, no painel de pilotagem da
/// parede superior, aperta E para assumir os controles (abre a FlightScene).
///
/// E daqui que o voo e tocado: a nave nao para de voar porque o piloto saiu da
/// cabine, entao o Flight vive nesta cena (que existe pela viagem inteira) e e
/// atualizado em piloto automatico enquanto o jogador anda la dentro. Uma
/// batida chega aqui como sacudida da camera e um baque abafado pelo casco.
class InteriorScene : public Scene {
public:
    InteriorScene();

    void aoEntrar(Context& ctx) override;
    void aoSair(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

private:
    static constexpr int kTile = 16;
    static constexpr int kLarguraMapa = 20;  // em tiles
    static constexpr int kAlturaMapa = 13;
    static constexpr float kVelocidade = 96.0f;
    static constexpr float kAmplitudeTremor = 4.2f;  // em unidades de mundo

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
    Flight voo_;

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

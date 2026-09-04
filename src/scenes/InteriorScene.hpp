#pragma once

#include <SDL3/SDL.h>

#include "audio/Audio.hpp"
#include "gfx/Animacao.hpp"
#include "gfx/Camera.hpp"
#include "gfx/Sprite.hpp"
#include "scene/Scene.hpp"
#include "scenes/Transicao.hpp"
#include "sim/Flight.hpp"
#include "world/MapaDeTiles.hpp"

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
    void aoRetomar(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

    /// A viagem que esta cena guarda. A cabine e o painel do casco a recebem
    /// pelo construtor; quem pergunta aqui e a tela de depuracao, que procura o
    /// voo percorrendo a pilha de cenas.
    Flight& voo() { return voo_; }

private:
    static constexpr int kTile = MapaDeTiles::kTile;
    static constexpr float kVelocidade = 96.0f;
    static constexpr float kAmplitudeTremor = 4.2f;  // em unidades de mundo
    static constexpr float kZoom = 2.0f;
    /// Para onde o zoom vai enquanto a cortina fecha sobre o painel.
    static constexpr float kZoomConsole = 3.1f;

    /// Clipes da folha textures/player.png: uma linha por estado, quatro
    /// quadros por linha. O ritmo do andar sai da velocidade -- a 96 u/s um
    /// ciclo de 8 quadros por segundo cobre 48 unidades, uma passada e meia do
    /// personagem, que e o que faz o pe parecer preso no chao.
    static constexpr Clipe kParado{0, 4, 2.5f};
    static constexpr Clipe kAndando{1, 4, 8.0f};

    /// Move o jogador resolvendo colisao contra os tiles, um eixo por vez.
    void moverComColisao(SDL_FPoint deslocamento);
    SDL_FRect caixaDoJogador(SDL_FPoint centro) const;
    SDL_FRect limitesDoMundo() const;
    bool pertoDoConsole() const;
    /// Inclina a camera sobre o painel na mesma medida em que a cortina fecha.
    void aproximarDoConsole(float dt);
    /// Escolhe o clipe pelo que o jogador esta fazendo e anda o temporizador.
    void animarJogador(SDL_FPoint direcao, float dt);

    MapaDeTiles mapa_;
    Camera camera_;
    Flight voo_;
    Transicao transicao_;

    /// A vista externa ja foi pedida para mostrar o fim da nave. Sem esta
    /// trava, dois passos fixos no mesmo quadro (a pilha so aplica os comandos
    /// no fim dele) empilhariam duas cabines.
    bool entregouADestruicao_{false};

    SDL_FPoint posicao_{0.0f, 0.0f};
    SDL_FPoint posicaoAnterior_{0.0f, 0.0f};
    SDL_FlipMode espelho_{SDL_FLIP_NONE};
    float tempo_{0.0f};

    SDL_FRect console_{0.0f, 0.0f, 48.0f, 32.0f};
    SDL_FRect zonaDoConsole_{0.0f, 0.0f, 0.0f, 0.0f};

    Sprite jogador_;
    Animacao animacaoJogador_;
    Sprite consoleSprite_;
    SDL_Texture* tiles_{nullptr};
    Audio::SomId somConfirmar_{0};
};

}  // namespace jogo

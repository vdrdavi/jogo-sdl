#include "scenes/GameOverScene.hpp"

#include <cmath>

#include "core/App.hpp"
#include "gfx/BitmapFont.hpp"
#include "gfx/Draw.hpp"
#include "input/Input.hpp"

namespace jogo {
namespace {

constexpr SDL_Color kCorFundo{5, 6, 14, 255};
constexpr SDL_Color kCorTitulo{235, 110, 105, 255};
constexpr SDL_Color kCorTexto{186, 196, 210, 255};
constexpr SDL_Color kCorApagada{110, 120, 138, 255};

}  // namespace

void GameOverScene::aoEntrar(Context& ctx) {
    somConfirmar_ = ctx.audio.carregar("audio/confirm.wav");
    // A cortina chegou fechada da cena anterior; e dela que esta tela aparece.
    transicao_.iniciarEntrada(kAberturaCortina);
}

void GameOverScene::atualizar(Context& ctx, float dt) {
    tempo_ += dt;
    transicao_.avancar(dt);

    // So depois de a tela ter aparecido: a tecla que o jogador ainda estava
    // segurando quando a nave se abriu nao pode pular o fim.
    if (transicao_.ativa()) {
        return;
    }
    if (ctx.input.acaoPressionada(Acao::Confirmar) || ctx.input.acaoPressionada(Acao::Voltar) ||
        ctx.input.acaoPressionada(Acao::Pausar)) {
        ctx.audio.tocar(somConfirmar_);
        // Dois degraus: esta tela e a partida que ficou congelada embaixo dela.
        // O aoSair da InteriorScene e quem encerra a viagem (e o ambiente).
        ctx.cenas.desempilhar();
        ctx.cenas.desempilhar();
    }
}

void GameOverScene::desenhar(Context& ctx, float /*alpha*/) {
    const float largura = static_cast<float>(App::kLarguraLogica);
    const float altura = static_cast<float>(App::kAlturaLogica);
    const float meio = largura * 0.5f;

    draw::retanguloTela(ctx.renderer, SDL_FRect{0.0f, 0.0f, largura, altura}, kCorFundo);

    ctx.fonte.desenharCentralizado(ctx.renderer, "NAVE PERDIDA", meio, 118.0f, kCorTitulo, 3.0f);
    ctx.fonte.desenharCentralizado(ctx.renderer, "o casco cedeu no campo de asteroides", meio,
                                   178.0f, kCorTexto, 1.0f);
    ctx.fonte.desenharCentralizado(ctx.renderer, "e a viagem terminou onde ela ia parar", meio,
                                   198.0f, kCorApagada, 1.0f);

    // O convite pulsa devagar, para a tela nao parecer travada.
    const float pulso = 0.5f + 0.5f * std::sin(tempo_ * 2.4f);
    const SDL_Color corConvite{static_cast<Uint8>(140.0f + pulso * 90.0f),
                               static_cast<Uint8>(150.0f + pulso * 90.0f),
                               static_cast<Uint8>(165.0f + pulso * 90.0f), 255};
    const char* convite =
        ctx.input.temGamepad() ? "A: voltar ao menu" : "Enter ou Esc: voltar ao menu";
    ctx.fonte.desenharCentralizado(ctx.renderer, convite, meio, 258.0f, corConvite, 1.0f);

    transicao_.desenhar(ctx.renderer);
}

}  // namespace jogo

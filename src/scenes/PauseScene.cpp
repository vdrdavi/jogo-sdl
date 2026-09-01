#include "scenes/PauseScene.hpp"

#include "core/App.hpp"
#include "gfx/BitmapFont.hpp"
#include "gfx/Draw.hpp"
#include "input/Input.hpp"

namespace jogo {

void PauseScene::aoEntrar(Context& ctx) {
    somVoltar_ = ctx.audio.carregar("audio/back.wav");
}

void PauseScene::atualizar(Context& ctx, float dt) {
    tempo_ += dt;

    if (ctx.input.acaoPressionada(Acao::Pausar) || ctx.input.acaoPressionada(Acao::Confirmar)) {
        ctx.audio.tocar(somVoltar_);
        ctx.cenas.desempilhar();
        return;
    }
    if (ctx.input.teclaPressionada(SDL_SCANCODE_M)) {
        ctx.audio.tocar(somVoltar_);
        // Sai da pausa e da partida, voltando ao menu que ficou na base da pilha.
        ctx.cenas.desempilhar();
        ctx.cenas.desempilhar();
    }
}

void PauseScene::desenhar(Context& ctx, float /*alpha*/) {
    const SDL_FRect tela{0.0f, 0.0f, static_cast<float>(App::kLarguraLogica),
                         static_cast<float>(App::kAlturaLogica)};
    draw::retanguloTela(ctx.renderer, tela, SDL_Color{8, 10, 16, 170});

    const float meio = static_cast<float>(App::kLarguraLogica) * 0.5f;
    ctx.fonte.desenharCentralizado(ctx.renderer, "PAUSADO", meio, 140.0f,
                                   SDL_Color{255, 255, 255, 255}, 3.0f);
    ctx.fonte.desenharCentralizado(ctx.renderer, "Esc ou Start: continuar", meio, 200.0f,
                                   SDL_Color{186, 196, 210, 255}, 1.0f);
    ctx.fonte.desenharCentralizado(ctx.renderer, "M: voltar ao menu", meio, 220.0f,
                                   SDL_Color{186, 196, 210, 255}, 1.0f);
}

}  // namespace jogo

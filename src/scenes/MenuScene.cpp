#include "scenes/MenuScene.hpp"

#include <cmath>

#include "core/App.hpp"
#include "gfx/BitmapFont.hpp"
#include "gfx/Draw.hpp"
#include "input/Input.hpp"
#include "scenes/InteriorScene.hpp"

namespace jogo {
namespace {

constexpr SDL_Color kCorTitulo{240, 216, 120, 255};
constexpr SDL_Color kCorItem{176, 186, 200, 255};
constexpr SDL_Color kCorItemAtivo{255, 255, 255, 255};
constexpr SDL_Color kCorRodape{110, 120, 138, 255};

}  // namespace

void MenuScene::aoEntrar(Context& ctx) {
    somMover_ = ctx.audio.carregar("audio/blip.wav");
    somConfirmar_ = ctx.audio.carregar("audio/confirm.wav");
}

void MenuScene::atualizar(Context& ctx, float dt) {
    tempo_ += dt;

    const int total = static_cast<int>(Opcao::Contagem);
    if (ctx.input.acaoPressionada(Acao::Cima)) {
        selecao_ = (selecao_ + total - 1) % total;
        ctx.audio.tocar(somMover_);
    }
    if (ctx.input.acaoPressionada(Acao::Baixo)) {
        selecao_ = (selecao_ + 1) % total;
        ctx.audio.tocar(somMover_);
    }

    if (ctx.input.acaoPressionada(Acao::Confirmar)) {
        ctx.audio.tocar(somConfirmar_);
        switch (static_cast<Opcao>(selecao_)) {
            case Opcao::Jogar:
                ctx.cenas.empilhar(std::make_unique<InteriorScene>());
                break;
            case Opcao::TelaCheia:
                ctx.app.alternarTelaCheia();
                break;
            case Opcao::Sair:
            case Opcao::Contagem:
                ctx.app.sair();
                break;
        }
    }

    if (ctx.input.acaoPressionada(Acao::Voltar)) {
        ctx.app.sair();
    }
}

void MenuScene::desenhar(Context& ctx, float /*alpha*/) {
    const float meio = static_cast<float>(App::kLarguraLogica) * 0.5f;

    // O espacamento sai da altura de linha da fonte, para o layout acompanhar
    // uma eventual troca do atlas.
    const float yTitulo = 52.0f;
    const float ySubtitulo = yTitulo + ctx.fonte.alturaLinha(3.0f) + 8.0f;
    const float yPrimeiroItem = 165.0f;
    const float espacoItem = ctx.fonte.alturaLinha(2.0f) + 10.0f;

    ctx.fonte.desenharCentralizado(ctx.renderer, "JOGO SDL3", meio, yTitulo, kCorTitulo, 3.0f);
    ctx.fonte.desenharCentralizado(ctx.renderer, "base de projeto", meio, ySubtitulo, kCorRodape,
                                   1.0f);

    for (std::size_t i = 0; i < kRotulos.size(); ++i) {
        const bool ativo = static_cast<int>(i) == selecao_;
        const float y = yPrimeiroItem + static_cast<float>(i) * espacoItem;
        ctx.fonte.desenharCentralizado(ctx.renderer, kRotulos[i], meio, y,
                                       ativo ? kCorItemAtivo : kCorItem, ativo ? 2.0f : 1.5f);
        if (ativo) {
            // Cursor pulsante a esquerda do item selecionado.
            const float pulso = 3.0f * std::sin(tempo_ * 6.0f);
            const float largura = ctx.fonte.medir(kRotulos[i], 2.0f).x;
            ctx.fonte.desenhar(ctx.renderer, ">", meio - largura * 0.5f - 32.0f + pulso, y,
                               kCorTitulo, 2.0f);
        }
    }

    const char* dica = ctx.input.temGamepad()
                           ? "setas/analogico: navegar   A: confirmar   B: sair"
                           : "setas ou WASD: navegar   Enter: confirmar   Esc: sair";
    ctx.fonte.desenharCentralizado(ctx.renderer, dica, meio,
                                   static_cast<float>(App::kAlturaLogica) - 34.0f, kCorRodape,
                                   1.0f);
}

}  // namespace jogo

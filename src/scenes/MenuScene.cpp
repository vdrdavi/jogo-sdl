#include "scenes/MenuScene.hpp"

#include <algorithm>
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

bool telaCheia(const Context& ctx) {
    SDL_Window* janela = ctx.app.janela();
    return janela != nullptr && (SDL_GetWindowFlags(janela) & SDL_WINDOW_FULLSCREEN) != 0;
}

}  // namespace

std::string MenuScene::rotulo(const Context& ctx, Opcao opcao) {
    switch (opcao) {
        case Opcao::Jogar:
            return "Jogar";
        case Opcao::Volume:
            // Arredonda para o inteiro mais proximo: com o passo de 5% o texto
            // anda de 5 em 5 sem casa decimal aparecendo por erro de float.
            return "Volume: " + std::to_string(static_cast<int>(ctx.app.volume() * 100.0f +
                                                               0.5f)) +
                   "%";
        case Opcao::TelaCheia:
            return telaCheia(ctx) ? "Tela cheia: sim" : "Tela cheia: nao";
        case Opcao::Sair:
        case Opcao::Contagem:
            break;
    }
    return "Sair";
}

bool MenuScene::ajustar(Context& ctx, Opcao opcao, int passo) {
    switch (opcao) {
        case Opcao::Volume: {
            const float antes = ctx.app.volume();
            const float agora =
                std::clamp(antes + static_cast<float>(passo) * kPassoVolume, 0.0f, 1.0f);
            if (agora == antes) {
                return false;  // ja esta no fim da faixa: nada muda, nada soa
            }
            ctx.app.definirVolume(agora);
            return true;
        }
        case Opcao::TelaCheia:
            ctx.app.alternarTelaCheia();
            return true;
        case Opcao::Jogar:
        case Opcao::Sair:
        case Opcao::Contagem:
            return false;
    }
    return false;
}

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

    // O blip toca depois da mudanca de volume, e por isso ja sai no volume novo:
    // o som e a propria previa do ajuste.
    const int passo = (ctx.input.acaoPressionada(Acao::Direita) ? 1 : 0) -
                      (ctx.input.acaoPressionada(Acao::Esquerda) ? 1 : 0);
    if (passo != 0 && ajustar(ctx, static_cast<Opcao>(selecao_), passo)) {
        ctx.audio.tocar(somMover_);
    }

    if (ctx.input.acaoPressionada(Acao::Confirmar)) {
        ctx.audio.tocar(somConfirmar_);
        switch (static_cast<Opcao>(selecao_)) {
            case Opcao::Jogar:
                ctx.cenas.empilhar(std::make_unique<InteriorScene>());
                break;
            case Opcao::Volume:
            case Opcao::TelaCheia:
                ajustar(ctx, static_cast<Opcao>(selecao_), 1);
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
    const std::size_t itens = static_cast<std::size_t>(Opcao::Contagem);

    // O espacamento sai da altura de linha da fonte, para o layout acompanhar
    // uma eventual troca do atlas. Sem titulo, o bloco de itens e o unico
    // conteudo da tela: a posicao dele vem da propria altura, senao ele fica
    // onde o titulo o empurrava e a tela nasce vazia em cima.
    const float espacoItem = ctx.fonte.alturaLinha(2.0f) + 10.0f;
    const float alturaBloco =
        espacoItem * static_cast<float>(itens - 1) + ctx.fonte.alturaLinha(2.0f);
    const float yPrimeiroItem = (static_cast<float>(App::kAlturaLogica) - alturaBloco) * 0.5f;

    for (std::size_t i = 0; i < itens; ++i) {
        const bool ativo = static_cast<int>(i) == selecao_;
        const float y = yPrimeiroItem + static_cast<float>(i) * espacoItem;
        const std::string texto = rotulo(ctx, static_cast<Opcao>(i));
        ctx.fonte.desenharCentralizado(ctx.renderer, texto, meio, y,
                                       ativo ? kCorItemAtivo : kCorItem, ativo ? 2.0f : 1.5f);
        if (ativo) {
            // Cursor pulsante a esquerda do item selecionado.
            const float pulso = 3.0f * std::sin(tempo_ * 6.0f);
            const float largura = ctx.fonte.medir(texto, 2.0f).x;
            ctx.fonte.desenhar(ctx.renderer, ">", meio - largura * 0.5f - 32.0f + pulso, y,
                               kCorTitulo, 2.0f);
        }
    }

    const char* dica = ctx.input.temGamepad()
                           ? "direcional: navegar e ajustar   A: confirmar   B: sair"
                           : "setas ou WASD: navegar e ajustar   Enter: confirmar   Esc: sair";
    ctx.fonte.desenharCentralizado(ctx.renderer, dica, meio,
                                   static_cast<float>(App::kAlturaLogica) - 34.0f, kCorRodape,
                                   1.0f);
}

}  // namespace jogo

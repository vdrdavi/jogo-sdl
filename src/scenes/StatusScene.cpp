#include "scenes/StatusScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/App.hpp"
#include "gfx/BitmapFont.hpp"
#include "gfx/Draw.hpp"
#include "input/Input.hpp"
#include "scenes/FlightScene.hpp"

namespace jogo {
namespace {

constexpr SDL_Color kCorVeu{6, 9, 16, 190};
constexpr SDL_Color kCorVidro{10, 18, 30, 235};
constexpr SDL_Color kCorBorda{60, 110, 150, 255};
constexpr SDL_Color kCorTitulo{150, 230, 255, 255};
constexpr SDL_Color kCorTexto{198, 226, 245, 255};
constexpr SDL_Color kCorApagada{92, 110, 130, 255};
constexpr SDL_Color kCorTrilho{14, 26, 40, 255};
constexpr SDL_Color kCorPerda{235, 110, 105, 255};

/// Faixas do casco: a cor e a palavra saem da mesma fronteira, para o texto
/// nunca dizer "integro" sobre uma barra ja alaranjada. A fronteira do critico
/// e a do Flight, a mesma que liga a sirene e a luz de emergencia -- o
/// mostrador nao pode dizer AVARIADO com o alarme tocando.
struct Faixa {
    SDL_Color cor;
    const char* palavra;
};

Faixa faixaDo(float casco) {
    if (casco > 0.6f) {
        return Faixa{SDL_Color{120, 220, 150, 255}, "INTEGRO"};
    }
    if (casco > Flight::kCascoCritico) {
        return Faixa{SDL_Color{245, 190, 110, 255}, "AVARIADO"};
    }
    return Faixa{kCorPerda, "CRITICO"};
}

/// Suavizacao exponencial estavel em passo fixo.
float aproximar(float atual, float alvo, float taxa, float dt) {
    return atual + (alvo - atual) * (1.0f - std::exp(-taxa * dt));
}

}  // namespace

StatusScene::StatusScene(Flight& voo) : voo_(voo) {}

void StatusScene::aoEntrar(Context& ctx) {
    somVoltar_ = ctx.audio.carregar("audio/back.wav");
    // O mostrador abre no valor real: a viagem ja aconteceu, e uma varredura do
    // zero ate o casco atual seria enfeite fingindo ser medicao.
    ponteiro_ = voo_.casco();
}

void StatusScene::atualizar(Context& ctx, float dt) {
    // O passo do voo vem antes da saida, como no conves: fechar o painel nao
    // pode custar um passo a viagem. E o mesmo passo que esta cena da quando um
    // painel se abre por cima dela, entao vem de la inteiro.
    acompanhar(ctx, dt);

    // O mostrador chegou a zero: nao ha diagnostico a fazer em uma nave que
    // acabou de se romper. Esta cena se troca pela vista externa -- trocar, e
    // nao empilhar, deixa a pilha igualzinha a dos outros caminhos ate o fim
    // (MenuScene > InteriorScene > FlightScene).
    if (voo_.destruida() && !entregouADestruicao_) {
        entregouADestruicao_ = true;
        ctx.cenas.substituir(std::make_unique<FlightScene>(voo_));
        return;
    }
    if (entregouADestruicao_) {
        return;
    }

    if (ctx.input.acaoPressionada(Acao::Voltar) || ctx.input.acaoPressionada(Acao::Pausar) ||
        ctx.input.acaoPressionada(Acao::Diagnostico)) {
        ctx.audio.tocar(somVoltar_);
        ctx.cenas.desempilhar();
    }
}

void StatusScene::acompanhar(Context& ctx, float dt) {
    tempo_ += dt;

    // O diagnostico nao interrompe a viagem: le o casco de uma nave que segue
    // voando sozinha -- inclusive contra a proxima pedra.
    voo_.atualizar(ctx, dt, Flight::Comando{});

    ponteiro_ = aproximar(ponteiro_, voo_.casco(), kTaxaPonteiro, dt);
    // A perseguicao exponencial chega perto e nunca encosta; sem este encaixe
    // sobraria para sempre uma lasca de vermelho de menos de um pixel na barra.
    if (std::fabs(ponteiro_ - voo_.casco()) < 0.001f) {
        ponteiro_ = voo_.casco();
    }
}

void StatusScene::desenhar(Context& ctx, float /*alpha*/) {
    const float larguraTela = static_cast<float>(App::kLarguraLogica);
    const float alturaTela = static_cast<float>(App::kAlturaLogica);
    const float meio = larguraTela * 0.5f;

    draw::retanguloTela(ctx.renderer, SDL_FRect{0.0f, 0.0f, larguraTela, alturaTela}, kCorVeu);

    // A moldura do painel. As medidas verticais saem da altura da linha da
    // fonte, e nao de constantes: trocar a fonte muda a metrica da celula. A
    // altura e a soma do que vai dentro, na mesma ordem do desenho abaixo (se
    // mexer em um, mexa no outro), mais a margem de cima e a de baixo.
    const float linha = ctx.fonte.alturaLinha(1.0f);
    const float margem = 14.0f;
    const float alturaConteudo = linha * 2.0f + kAlturaBarra + linha +
                                 ctx.fonte.alturaLinha(2.0f) + 4.0f + linha * 1.6f + linha;
    const SDL_FRect vidro{meio - 190.0f, 88.0f, 380.0f, alturaConteudo + margem * 2.0f};
    draw::retanguloTela(ctx.renderer, vidro, kCorVidro);
    draw::retanguloTela(ctx.renderer, vidro, kCorBorda, false);

    float y = vidro.y + margem;
    ctx.fonte.desenharCentralizado(ctx.renderer, "DIAGNOSTICO DO CASCO", meio, y, kCorTitulo, 1.0f);
    y += linha * 2.0f;

    // A barra: trilho, o casco que restou e -- entre ele e o ponteiro, que
    // ainda esta descendo -- o pedaco que a ultima rocha levou. A cor e a
    // palavra saem do ponteiro, e nao do casco, para nao contradizerem o numero
    // enquanto a queda esta sendo mostrada.
    const Faixa faixa = faixaDo(ponteiro_);
    const SDL_FRect trilho{vidro.x + 22.0f, y, vidro.w - 44.0f, kAlturaBarra};
    draw::retanguloTela(ctx.renderer, trilho, kCorTrilho);
    draw::retanguloTela(
        ctx.renderer,
        SDL_FRect{trilho.x, trilho.y, trilho.w * std::max(voo_.casco(), ponteiro_), trilho.h},
        kCorPerda);
    draw::retanguloTela(ctx.renderer,
                        SDL_FRect{trilho.x, trilho.y, trilho.w * voo_.casco(), trilho.h},
                        faixa.cor);
    draw::retanguloTela(ctx.renderer, trilho, kCorBorda, false);
    y += trilho.h + linha;

    // O numero acompanha o ponteiro, nao o casco: e o mesmo movimento da barra
    // dito em digitos, e assim os dois nunca se contradizem no meio da queda.
    char leitura[32];
    std::snprintf(leitura, sizeof(leitura), "%d%%",
                  static_cast<int>(ponteiro_ * 100.0f + 0.5f));
    ctx.fonte.desenharCentralizado(ctx.renderer, leitura, meio, y, faixa.cor, 2.0f);
    y += ctx.fonte.alturaLinha(2.0f) + 4.0f;

    // Enquanto o baque decai, a palavra pisca: o mostrador reage a batida que
    // acabou de acontecer sem que a cena precise de um temporizador proprio.
    const bool piscando = voo_.batida() > 0.0f && std::sin(tempo_ * 26.0f) < 0.0f;
    ctx.fonte.desenharCentralizado(ctx.renderer, faixa.palavra, meio, y,
                                   piscando ? kCorApagada : faixa.cor, 1.0f);
    y += linha * 1.6f;

    ctx.fonte.desenharCentralizado(ctx.renderer, "cada rocha custa um pedaco do casco", meio, y,
                                   kCorApagada, 1.0f);

    const char* dica = ctx.input.temGamepad() ? "B ou Y: voltar ao conves"
                                              : "Esc ou Q: voltar ao conves";
    ctx.fonte.desenharCentralizado(ctx.renderer, dica, meio, vidro.y + vidro.h + 12.0f, kCorTexto,
                                   1.0f);
}

}  // namespace jogo

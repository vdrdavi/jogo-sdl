#include "scenes/RepairScene.hpp"

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
constexpr SDL_Color kCorZona{86, 190, 130, 255};
constexpr SDL_Color kCorAgulha{240, 226, 180, 255};
constexpr SDL_Color kCorFaisca{255, 248, 214, 255};
constexpr SDL_Color kCorPerda{235, 110, 105, 255};

Uint8 misturarCanal(Uint8 a, Uint8 b, float t) {
    const float valor = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
    return static_cast<Uint8>(std::clamp(valor, 0.0f, 255.0f));
}

SDL_Color misturar(SDL_Color a, SDL_Color b, float t) {
    return SDL_Color{misturarCanal(a.r, b.r, t), misturarCanal(a.g, b.g, t),
                     misturarCanal(a.b, b.b, t), misturarCanal(a.a, b.a, t)};
}

}  // namespace

RepairScene::RepairScene(Flight& voo)
    // A serie nao pode ser decorada: com semente fixa, a mesma sequencia de
    // zonas sairia em toda viagem e o compasso viraria uma coreografia. O
    // relogio do SDL basta -- aqui nao ha nada a reproduzir depois, ao
    // contrario do campo de rochas, que e sorteado por semente de proposito.
    : voo_(voo), rng_(static_cast<Uint32>(SDL_GetTicks()) ^ 0x50DA17u) {}

void RepairScene::aoEntrar(Context& ctx) {
    somSolda_ = ctx.audio.carregar("audio/solda.wav");
    somErro_ = ctx.audio.carregar("audio/falha.wav");
    somVoltar_ = ctx.audio.carregar("audio/back.wav");

    batidaAnterior_ = voo_.batida();
    perderSerie();
    // Sem o macarico frio de saida: a serie comeca do zero, mas quem acabou de
    // chegar na bancada nao errou nada ainda.
    trava_ = 0.0f;
}

void RepairScene::sortearZona() {
    // A zona nunca nasce debaixo do ponteiro -- seria um acerto de graca. Com a
    // folga bem menor que a barra o sorteio converge em uma ou duas voltas; o
    // teto de tentativas so existe para o laco nao depender da sorte.
    for (int tentativa = 0; tentativa < 8; ++tentativa) {
        zonaCentro_ = rng_.entre(zonaMeia_, 1.0f - zonaMeia_);
        if (std::fabs(zonaCentro_ - ponteiro_) > kFolgaDaZona) {
            return;
        }
    }
}

void RepairScene::perderSerie() {
    zonaMeia_ = kZonaInicial * 0.5f;
    velocidade_ = kVelocidadeInicial;
    pontos_ = 0;
    trava_ = kTravaDoErro;
    sortearZona();
}

void RepairScene::avancarPonteiro(float dt) {
    ponteiro_ += sentido_ * velocidade_ * dt;
    // Quica nas pontas: o excedente volta para dentro da barra, entao a
    // varredura nao perde um pedaco de passo na virada.
    if (ponteiro_ > 1.0f) {
        ponteiro_ = 2.0f - ponteiro_;
        sentido_ = -1.0f;
    } else if (ponteiro_ < 0.0f) {
        ponteiro_ = -ponteiro_;
        sentido_ = 1.0f;
    }
}

void RepairScene::soldar(Context& ctx) {
    if (std::fabs(ponteiro_ - zonaCentro_) <= zonaMeia_) {
        voo_.reparar(kGanhoPorPonto);
        ++pontos_;
        // Acertar aperta o proximo acerto: a zona encolhe e o ponteiro acelera.
        // A serie longa e que paga bem, e e ela que fica insustentavel -- quem
        // decide quando descer da bancada e o jogador, nao um cronometro.
        zonaMeia_ = std::max(zonaMeia_ * kEncolhimento, kZonaMinima * 0.5f);
        velocidade_ = std::min(velocidade_ * kAceleracao, kVelocidadeMaxima);
        realceAcerto_ = 1.0f;
        ctx.audio.tocar(somSolda_);
        sortearZona();
        return;
    }

    // Errar nao custa casco: custa os segundos do macarico frio, e segundos sao
    // rocha. A punicao do minigame ja esta do lado de fora.
    realceErro_ = 1.0f;
    ctx.audio.tocar(somErro_);
    perderSerie();
}

void RepairScene::atualizar(Context& ctx, float dt) {
    ponteiroAnterior_ = ponteiro_;

    // O passo do voo vem antes de qualquer saida, como no conves: fechar a
    // bancada nao pode custar um passo a viagem. E o mesmo passo que esta cena
    // da quando um painel se abre por cima dela, entao vem de la inteiro.
    acompanhar(ctx, dt);

    // O casco cedeu com o piloto na bancada. Nao ha o que soldar em uma nave
    // que acabou de se abrir: esta cena se troca pela vista externa -- trocar,
    // e nao empilhar, deixa a pilha igual a dos outros caminhos ate o fim
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
        ctx.input.acaoPressionada(Acao::Interagir)) {
        ctx.audio.tocar(somVoltar_);
        ctx.cenas.desempilhar();
        return;
    }

    realceAcerto_ = std::max(0.0f, realceAcerto_ - kDecaimentoRealce * dt);
    realceErro_ = std::max(0.0f, realceErro_ - kDecaimentoRealce * dt);

    if (!voo_.reparavel()) {
        return;
    }

    if (trava_ > 0.0f) {
        trava_ = std::max(0.0f, trava_ - dt);
        return;
    }

    // O aperto e julgado contra a posicao em que o ponteiro estava, e nao contra
    // a de depois do avanco: e essa a que o jogador viu na tela quando decidiu
    // apertar.

    if (ctx.input.acaoPressionada(Acao::Confirmar)) {
        soldar(ctx);
        return;
    }
    avancarPonteiro(dt);
}

void RepairScene::acompanhar(Context& ctx, float dt) {
    tempo_ += dt;

    // A bancada nao interrompe a viagem: solda-se o casco de uma nave que segue
    // voando sozinha -- inclusive contra a proxima pedra.
    voo_.atualizar(ctx, dt, Flight::Comando{});

    // E a pedra desmancha a solda. Isto fica aqui, e nao no atualizar, porque e
    // reacao ao mundo e nao a entrada: a rocha bateu, e a serie se perde tenha
    // ou nao um painel aberto na frente da bancada.
    if (voo_.batida() > batidaAnterior_) {
        realceErro_ = 1.0f;
        perderSerie();
    }
    batidaAnterior_ = voo_.batida();
}

void RepairScene::desenhar(Context& ctx, float alpha) {
    const float larguraTela = static_cast<float>(App::kLarguraLogica);
    const float alturaTela = static_cast<float>(App::kAlturaLogica);
    const float meio = larguraTela * 0.5f;

    draw::retanguloTela(ctx.renderer, SDL_FRect{0.0f, 0.0f, larguraTela, alturaTela}, kCorVeu);

    // Como no diagnostico, as medidas verticais saem da altura da linha da
    // fonte: a altura do vidro e a soma do que vai dentro, na mesma ordem do
    // desenho abaixo (se mexer em um, mexa no outro), mais as duas margens.
    const float linha = ctx.fonte.alturaLinha(1.0f);
    const float margem = 14.0f;
    const float alturaConteudo = linha * 2.0f + kAlturaBarra + linha * 1.4f + linha * 1.7f + linha;
    const SDL_FRect vidro{meio - 190.0f, 92.0f, 380.0f, alturaConteudo + margem * 2.0f};
    draw::retanguloTela(ctx.renderer, vidro, kCorVidro);
    draw::retanguloTela(ctx.renderer, vidro, kCorBorda, false);

    float y = vidro.y + margem;
    ctx.fonte.desenharCentralizado(ctx.renderer, "BANCADA DE REPARO", meio, y, kCorTitulo, 1.0f);
    y += linha * 2.0f;

    // A barra: o trilho, a zona de acerto e a agulha por cima. Com o casco no
    // teto nao ha o que soldar, e a barra aparece apagada em vez de sumir --
    // some o alvo, nao o instrumento.
    const bool reparavel = voo_.reparavel();
    const SDL_FRect trilho{vidro.x + 22.0f, y, vidro.w - 44.0f, kAlturaBarra};
    draw::retanguloTela(ctx.renderer, trilho, kCorTrilho);

    if (reparavel) {
        const SDL_FRect zona{trilho.x + (zonaCentro_ - zonaMeia_) * trilho.w, trilho.y,
                             zonaMeia_ * 2.0f * trilho.w, trilho.h};
        draw::retanguloTela(ctx.renderer, zona,
                            misturar(kCorZona, kCorFaisca, realceAcerto_));

        // A agulha anda no passo fixo e e interpolada aqui: sem isto ela
        // avancaria a 60 Hz numa tela que desenha a 240.
        const float posicao = ponteiroAnterior_ + (ponteiro_ - ponteiroAnterior_) * alpha;
        // Com o macarico frio a agulha para e pisca: e o proprio instrumento
        // dizendo por que apertar agora nao adianta.
        const bool apagada = trava_ > 0.0f && std::sin(tempo_ * 24.0f) < 0.0f;
        draw::retanguloTela(ctx.renderer,
                            SDL_FRect{trilho.x + posicao * trilho.w - 1.0f, trilho.y - 3.0f, 2.0f,
                                      trilho.h + 6.0f},
                            apagada ? kCorApagada : (trava_ > 0.0f ? kCorPerda : kCorAgulha));
    }

    draw::retanguloTela(ctx.renderer, trilho, misturar(kCorBorda, kCorPerda, realceErro_), false);
    y += trilho.h + linha * 1.4f;

    const char* estado = nullptr;
    SDL_Color corEstado = kCorTexto;
    if (!reparavel) {
        estado = "CASCO NO LIMITE DO REPARO DE CAMPO";
        corEstado = kCorApagada;
    } else if (trava_ > 0.0f) {
        estado = "SOLDA FRIA";
        corEstado = kCorPerda;
    } else if (realceAcerto_ > 0.0f) {
        estado = "PONTO SOLDADO";
        corEstado = kCorZona;
    } else {
        estado = "ALINHE O PONTO DE SOLDA";
    }
    ctx.fonte.desenharCentralizado(ctx.renderer, estado, meio, y, corEstado, 1.0f);
    y += linha * 1.7f;

    // A leitura do casco fica junto do teto: sem os dois numeros lado a lado, o
    // reparo que para nos 75% parece um defeito.
    char leitura[64];
    std::snprintf(leitura, sizeof(leitura), "CASCO %d%%   LIMITE %d%%   SOLDAS %d",
                  static_cast<int>(voo_.casco() * 100.0f + 0.5f),
                  static_cast<int>(Flight::kCascoReparado * 100.0f + 0.5f), pontos_);
    ctx.fonte.desenharCentralizado(ctx.renderer, leitura, meio, y, kCorApagada, 1.0f);

    const char* dica = ctx.input.temGamepad() ? "A: soldar   B ou X: voltar ao conves"
                                              : "Espaco: soldar   Esc ou E: voltar ao conves";
    ctx.fonte.desenharCentralizado(ctx.renderer, dica, meio, vidro.y + vidro.h + 12.0f, kCorTexto,
                                   1.0f);
}

}  // namespace jogo

#include "scenes/FlightScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/App.hpp"
#include "gfx/BitmapFont.hpp"
#include "gfx/Draw.hpp"
#include "input/Input.hpp"
#include "scenes/GameOverScene.hpp"

namespace jogo {
namespace {

constexpr SDL_Color kCorEspaco{5, 6, 14, 255};
constexpr SDL_Color kCorHud{198, 226, 245, 255};
constexpr SDL_Color kCorPainel{8, 12, 24, 170};

/// Suavizacao exponencial estavel em passo fixo.
float aproximar(float atual, float alvo, float taxa, float dt) {
    return atual + (alvo - atual) * (1.0f - std::exp(-taxa * dt));
}

}  // namespace

FlightScene::FlightScene(Flight& voo)
    : voo_(voo),
      cena_(static_cast<float>(App::kLarguraLogica), static_cast<float>(App::kAlturaLogica)) {}

void FlightScene::aoEntrar(Context& ctx) {
    nave_ = criarNaveLowPoly();
    // Semente fixa: o mesmo setor estelar em toda partida.
    estrelas_.gerar(0xC0FFEEu, 1800, 150.0f);
    cena_.definirNevoa(SDL_FColor{kCorEspaco.r / 255.0f, kCorEspaco.g / 255.0f,
                                  kCorEspaco.b / 255.0f, 1.0f},
                       kNevoaInicio, voo_.rochas().raio());

    // A nave ja estava voando quando a cabine abriu, entao a camera nasce onde
    // ela estaria se ja viesse perseguindo: no regime, perseguir a taxa k um
    // alvo que corre a v deixa a camera velocidade/k atras dele. Sem isso a
    // cabine abre colada na nave e recua sozinha no primeiro meio segundo.
    const Mat3 rotacao = Flight::rotacaoDe(voo_.pose());
    camera_ = voo_.pose().posicao + rotacao * Vec3{0.0f, 1.4f, 5.0f} -
              rotacao.frente() * (voo_.velocidade() / kPerseguicaoCamera);
    cameraAnterior_ = camera_;
    cameraCima_ = rotacao.cima();
    estrelas_.centralizar(camera_);

    // A vista abre do painel: a cortina que o conves fechou levanta daqui, e o
    // campo de visao comeca fechado e alarga sozinho na suavizacao que ja
    // existia para o turbo. Quando esta cena e aberta porque o casco cedeu, nao
    // ha painel de onde abrir: o corte e seco de proposito -- foi o que
    // aconteceu com a nave.
    if (!voo_.destruida()) {
        transicao_.iniciarEntrada();
        fov_ = kFovBase - kAberturaFov;
    } else {
        fov_ = kFovBase;
    }

    somSaida_ = ctx.audio.carregar("audio/back.wav");
    voo_.definirAbafado(false);
}

void FlightScene::aoSair(Context&) {
    // O voo continua sem a cabine; o que muda e o casco voltar a abafar.
    voo_.definirAbafado(true);
}

void FlightScene::comecarDestruicao() {
    morrendo_ = true;
    tempoDeMorte_ = 0.0f;

    // Os cacos nascem exatamente onde a nave estava, com a rotacao e a escala
    // com que ela era desenhada e carregando a velocidade que ela tinha: no
    // primeiro quadro eles ainda sao a nave, e so entao ela se abre.
    const Mat3 rotacao = Flight::rotacaoDe(voo_.pose());
    destrocos_.gerar(nave_, voo_.pose().posicao, rotacao, 1.15f,
                     rotacao.frente() * voo_.velocidade(), 0x0DEADBEEu);
}

void FlightScene::atualizar(Context& ctx, float dt) {
    // O casco zerou: daqui para a frente esta cena so mostra o fim. Vale tanto
    // para quem estava pilotando quanto para quem foi trazido do conves, e a
    // condicao e o estado do voo -- nao um aviso que alguem precise mandar.
    if (!morrendo_ && voo_.destruida()) {
        comecarDestruicao();
    }

    if (morrendo_) {
        tempoDeMorte_ += dt;
        // A cortina so comeca depois de os destrocos correrem: apagar em cima
        // da explosao seria esconder justamente o que ha para ver.
        if (tempoDeMorte_ >= kTempoDestrocos && !transicao_.saindo()) {
            transicao_.iniciarSaida(kApagarDaMorte);
        }
    } else if (!transicao_.saindo() && (ctx.input.acaoPressionada(Acao::Voltar) ||
                                        ctx.input.acaoPressionada(Acao::Pausar))) {
        // Sair e fechar a cortina; a pilha so desempilha quando ela cobrir a
        // tela, e do outro lado o conves reabre a partir dela. Enquanto isso
        // nao chega, esta cena continua sendo quem da o passo do voo.
        ctx.audio.tocar(somSaida_);
        // O casco volta a abafar ja no comeco da cortina, para o som fechar
        // junto com a imagem; a rampa do Flight cuida de nao virar degrau.
        voo_.definirAbafado(true);
        transicao_.iniciarSaida();
    }

    // A mesma cortina, dois destinos: no fim da viagem ela entrega a tela de
    // fim; no fim da visita a cabine, devolve o conves.
    if (transicao_.avancar(dt)) {
        if (morrendo_) {
            ctx.cenas.substituir(std::make_unique<GameOverScene>());
        } else {
            ctx.cenas.desempilhar();
        }
    }

    // Com a cortina fechando, o piloto ja largou os controles: o voo segue no
    // automatico -- e recebe o passo do mesmo jeito, sem buraco na simulacao.
    // Uma nave destruida nao le comando nenhum, com ou sem cortina.
    Flight::Comando comando;
    if (!transicao_.saindo() && !morrendo_) {
        comando.eixo = ctx.input.eixoMovimento();
        comando.turbo = ctx.input.acaoAtiva(Acao::Confirmar);
    }
    voo_.atualizar(ctx, dt, comando);
    avancarVista(dt);
}

void FlightScene::acompanhar(Context& ctx, float dt) {
    // O painel aberto por cima nao tira a nave do espaco: o voo recebe o passo
    // daqui, em piloto automatico, como receberia da cortina fechando.
    voo_.atualizar(ctx, dt, Flight::Comando{});
    avancarVista(dt);
}

void FlightScene::avancarVista(float dt) {
    tempo_ += dt;

    // Os cacos correm soltos, sem depender de mais nada: e deriva desenhada, e
    // por isso avanca aqui junto com a camera que os persegue -- parados com a
    // vista andando, ficariam para tras no primeiro painel aberto.
    if (morrendo_) {
        destrocos_.atualizar(dt);
    }

    fov_ = aproximar(fov_, voo_.turbo() ? kFovTurbo : kFovBase, 4.0f, dt);

    // Camera de terceira pessoa com atraso: da peso as manobras.
    const Mat3 rotacao = Flight::rotacaoDe(voo_.pose());
    cameraAnterior_ = camera_;
    const Vec3 desejada = voo_.pose().posicao + rotacao * Vec3{0.0f, 1.4f, 5.0f};
    const float perseguicao = morrendo_ ? kPerseguicaoMorte : kPerseguicaoCamera;
    camera_ = lerp(camera_, desejada, 1.0f - std::exp(-perseguicao * dt));
    cameraCima_ = normalizar(lerp(cameraCima_, rotacao.cima(), 1.0f - std::exp(-6.0f * dt)));

    // Campo infinito: as estrelas sao reposicionadas em torno da camera.
    estrelas_.centralizar(camera_);
}

void FlightScene::desenhar(Context& ctx, float alpha) {
    // Estado interpolado entre os dois ultimos passos fixos.
    const Flight::Pose pose = voo_.interpolada(alpha);
    const Vec3 posicao = pose.posicao;
    const Mat3 rotacao = Flight::rotacaoDe(pose);

    Camera3D camera;
    camera.posicao = lerp(cameraAnterior_, camera_, alpha);
    camera.orientacao =
        Mat3::olhandoPara(posicao + rotacao.frente() * 14.0f - camera.posicao, cameraCima_);
    camera.fovGraus = fov_;

    // Tremor da batida: o olho sacode, a mira nao -- por isso a sacudida entra
    // depois da orientacao, deslocando so a posicao nos eixos da tela.
    if (voo_.batida() > 0.0f) {
        const float amplitude = kAmplitudeTremor * voo_.batida() * voo_.batida();
        camera.posicao += camera.orientacao.direita() * (std::sin(tempo_ * 61.0f) * amplitude) +
                          camera.orientacao.cima() * (std::sin(tempo_ * 43.0f) * amplitude);
    }

    cena_.definirCamera(camera);
    cena_.iniciarQuadro();

    // Vazio do espaco
    draw::retanguloTela(ctx.renderer,
                        SDL_FRect{0.0f, 0.0f, static_cast<float>(App::kLarguraLogica),
                                  static_cast<float>(App::kAlturaLogica)},
                        kCorEspaco);

    estrelas_.desenhar(ctx.renderer, cena_, rotacao.frente() * voo_.velocidade(), tempo_);

    voo_.rochas().submeter(cena_);
    // A nave ou o que sobrou dela: nunca as duas. Os cacos entram no mesmo lote
    // das rochas, entao o pintor os ordena junto com o resto da cena.
    if (morrendo_) {
        destrocos_.submeter(cena_);
    } else {
        cena_.submeter(nave_, posicao, rotacao, 1.15f);
    }
    cena_.desenhar(ctx.renderer);

    if (morrendo_) {
        destrocos_.desenharFaiscas(ctx.renderer, cena_);
    }

    // Escapamento: brilha mais forte no turbo -- e some junto com o motor.
    SDL_FPoint motor;
    float profundidade = 0.0f;
    if (!morrendo_ &&
        cena_.projetar(posicao + rotacao * Vec3{0.0f, 0.05f, 1.75f}, motor, &profundidade)) {
        const float intensidade = 0.35f + 0.65f * voo_.fatorTurbo();
        const float tremor = 0.9f + 0.1f * std::sin(tempo_ * 30.0f);
        draw::brilhoAditivo(ctx.renderer, motor, cena_.escalaEmTela(profundidade) * 0.55f * tremor,
                      SDL_FColor{1.0f * intensidade, 0.55f * intensidade, 0.22f * intensidade,
                                 1.0f});
    }

    // Clarao da batida por cima da cena, antes do HUD.
    if (voo_.batida() > 0.0f) {
        const Uint8 alfa =
            static_cast<Uint8>(std::clamp(voo_.batida() * voo_.batida() * 62.0f, 0.0f, 255.0f));
        draw::retanguloTela(ctx.renderer,
                            SDL_FRect{0.0f, 0.0f, static_cast<float>(App::kLarguraLogica),
                                      static_cast<float>(App::kAlturaLogica)},
                            SDL_Color{255, 150, 90, alfa});
    }

    // Da destruicao em diante nao ha o que pilotar nem o que medir: a mira e a
    // HUD saem de cena e sobra a vista.
    if (morrendo_) {
        transicao_.desenhar(ctx.renderer);
        return;
    }

    // Mira
    const float cx = static_cast<float>(App::kLarguraLogica) * 0.5f;
    const float cy = static_cast<float>(App::kAlturaLogica) * 0.5f;
    const SDL_Color corMira{120, 200, 230, 120};
    draw::retanguloTela(ctx.renderer, SDL_FRect{cx - 6.0f, cy - 0.5f, 4.0f, 1.0f}, corMira);
    draw::retanguloTela(ctx.renderer, SDL_FRect{cx + 2.0f, cy - 0.5f, 4.0f, 1.0f}, corMira);
    draw::retanguloTela(ctx.renderer, SDL_FRect{cx - 0.5f, cy - 6.0f, 1.0f, 4.0f}, corMira);
    draw::retanguloTela(ctx.renderer, SDL_FRect{cx - 0.5f, cy + 2.0f, 1.0f, 4.0f}, corMira);

    // HUD
    char linha[64];
    std::snprintf(linha, sizeof(linha), "VEL %3.0f u/s%s",
                  static_cast<double>(voo_.velocidade()), voo_.turbo() ? "  [TURBO]" : "");
    const SDL_FPoint tamanho = ctx.fonte.medir(linha, 1.0f);
    draw::retanguloTela(ctx.renderer, SDL_FRect{8.0f, 8.0f, tamanho.x + 16.0f, tamanho.y + 12.0f},
                        kCorPainel);
    ctx.fonte.desenhar(ctx.renderer, linha, 16.0f, 14.0f,
                       voo_.turbo() ? SDL_Color{255, 200, 130, 255} : kCorHud, 1.0f);

    const char* dica = ctx.input.temGamepad()
                           ? "analogico: pilotar   A: turbo   B: voltar   desvie das rochas"
                           : "WASD: pilotar   Espaco: turbo   Esc: voltar   desvie das rochas";
    const SDL_FPoint tamanhoDica = ctx.fonte.medir(dica, 1.0f);
    const float yDica = static_cast<float>(App::kAlturaLogica) - 24.0f;
    draw::retanguloTela(ctx.renderer,
                        SDL_FRect{cx - tamanhoDica.x * 0.5f - 8.0f, yDica - 4.0f,
                                  tamanhoDica.x + 16.0f, tamanhoDica.y + 8.0f},
                        kCorPainel);
    ctx.fonte.desenharCentralizado(ctx.renderer, dica, cx, yDica, kCorHud, 1.0f);

    transicao_.desenhar(ctx.renderer);
}

}  // namespace jogo

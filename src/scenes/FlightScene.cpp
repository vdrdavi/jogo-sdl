#include "scenes/FlightScene.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "core/App.hpp"
#include "gfx/BitmapFont.hpp"
#include "gfx/Draw.hpp"
#include "input/Input.hpp"

namespace jogo {
namespace {

constexpr SDL_Color kCorEspaco{5, 6, 14, 255};
constexpr SDL_Color kCorHud{198, 226, 245, 255};
constexpr SDL_Color kCorPainel{8, 12, 24, 170};

/// Suavizacao exponencial estavel em passo fixo.
float aproximar(float atual, float alvo, float taxa, float dt) {
    return atual + (alvo - atual) * (1.0f - std::exp(-taxa * dt));
}

float interpolarAngulo(float anterior, float atual, float alpha) {
    return anterior + (atual - anterior) * alpha;
}

/// Brilho radial aditivo (leque de triangulos) usado no escapamento do motor.
void brilhoAditivo(SDL_Renderer* renderer, SDL_FPoint centro, float raio, SDL_FColor cor) {
    constexpr int kSegmentos = 12;
    if (raio <= 0.5f) {
        return;
    }

    std::vector<SDL_Vertex> leque;
    leque.reserve(kSegmentos * 3);

    const SDL_FColor transparente{cor.r, cor.g, cor.b, 0.0f};
    for (int i = 0; i < kSegmentos; ++i) {
        const float a0 = 6.2831853f * static_cast<float>(i) / kSegmentos;
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / kSegmentos;

        const SDL_Vertex meio{centro, cor, {0.0f, 0.0f}};
        const SDL_Vertex borda0{
            SDL_FPoint{centro.x + std::cos(a0) * raio, centro.y + std::sin(a0) * raio},
            transparente,
            {0.0f, 0.0f}};
        const SDL_Vertex borda1{
            SDL_FPoint{centro.x + std::cos(a1) * raio, centro.y + std::sin(a1) * raio},
            transparente,
            {0.0f, 0.0f}};
        leque.insert(leque.end(), {meio, borda0, borda1});
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    SDL_RenderGeometry(renderer, nullptr, leque.data(), static_cast<int>(leque.size()), nullptr, 0);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

}  // namespace

FlightScene::FlightScene()
    : cena_(static_cast<float>(App::kLarguraLogica), static_cast<float>(App::kAlturaLogica)) {}

void FlightScene::aoEntrar(Context& ctx) {
    nave_ = criarNaveLowPoly();
    // Semente fixa: o mesmo setor estelar em toda partida.
    estrelas_.gerar(0xC0FFEEu, 1800, 150.0f);

    pose_ = Pose{};
    pose_.camera = Vec3{0.0f, 1.4f, 5.0f};
    poseAnterior_ = pose_;
    velocidade_ = kVelocidadeCruzeiro;
    fov_ = kFovBase;

    estrelas_.centralizar(pose_.camera);
    somSaida_ = ctx.audio.carregar("audio/back.wav");

    // Ruido marrom em loop: o "la fora" da cena de voo. Comeca mudo e sobe em
    // atualizar(), para a troca de cena nao estourar um rugido do nada.
    ambiente_ = 0.0f;
    somAmbiente_ = ctx.audio.carregar("audio/espaco.wav");
    vozAmbiente_ = ctx.audio.tocarEmLoop(somAmbiente_, ambiente_);
}

void FlightScene::aoSair(Context& ctx) {
    ctx.audio.parar(vozAmbiente_, kAmbienteSaida);
    vozAmbiente_ = 0;
}

float FlightScene::fatorTurbo() const {
    return (velocidade_ - kVelocidadeCruzeiro) / (kVelocidadeTurbo - kVelocidadeCruzeiro);
}

void FlightScene::atualizar(Context& ctx, float dt) {
    tempo_ += dt;
    poseAnterior_ = pose_;

    if (ctx.input.acaoPressionada(Acao::Voltar) || ctx.input.acaoPressionada(Acao::Pausar)) {
        ctx.audio.tocar(somSaida_);
        ctx.cenas.desempilhar();
        return;
    }

    const SDL_FPoint entrada = ctx.input.eixoMovimento();

    // Curva inclinada: o rolamento acompanha a guinada, como em um caca.
    pose_.roll = aproximar(pose_.roll, -entrada.x * 0.85f, 6.0f, dt);
    pose_.yaw -= entrada.x * kTaxaGiro * dt;
    pose_.pitch = std::clamp(pose_.pitch - entrada.y * kTaxaPitch * dt, -kLimitePitch, kLimitePitch);

    turbo_ = ctx.input.acaoAtiva(Acao::Confirmar);
    velocidade_ = aproximar(velocidade_, turbo_ ? kVelocidadeTurbo : kVelocidadeCruzeiro, 3.0f, dt);
    fov_ = aproximar(fov_, turbo_ ? kFovTurbo : kFovBase, 4.0f, dt);

    const Mat3 rotacao = Mat3::deEuler(pose_.yaw, pose_.pitch, pose_.roll);
    pose_.posicao += rotacao.frente() * velocidade_ * dt;

    // Camera de terceira pessoa com atraso: da peso as manobras.
    const Vec3 desejada = pose_.posicao + rotacao * Vec3{0.0f, 1.4f, 5.0f};
    pose_.camera = lerp(pose_.camera, desejada, 1.0f - std::exp(-9.0f * dt));
    cameraCima_ = normalizar(lerp(cameraCima_, rotacao.cima(), 1.0f - std::exp(-6.0f * dt)));

    // Campo infinito: as estrelas sao reposicionadas em torno da camera.
    estrelas_.centralizar(pose_.camera);

    // O ambiente entra do zero e depois acompanha o esforco do motor.
    const float alvoAmbiente =
        kAmbienteCruzeiro + (kAmbienteTurbo - kAmbienteCruzeiro) * fatorTurbo();
    ambiente_ = aproximar(ambiente_, alvoAmbiente, kTaxaAmbiente, dt);
    ctx.audio.ajustarGanho(vozAmbiente_, ambiente_);
}

void FlightScene::desenhar(Context& ctx, float alpha) {
    // Estado interpolado entre os dois ultimos passos fixos.
    const Vec3 posicao = lerp(poseAnterior_.posicao, pose_.posicao, alpha);
    const float yaw = interpolarAngulo(poseAnterior_.yaw, pose_.yaw, alpha);
    const float pitch = interpolarAngulo(poseAnterior_.pitch, pose_.pitch, alpha);
    const float roll = interpolarAngulo(poseAnterior_.roll, pose_.roll, alpha);
    const Mat3 rotacao = Mat3::deEuler(yaw, pitch, roll);

    Camera3D camera;
    camera.posicao = lerp(poseAnterior_.camera, pose_.camera, alpha);
    camera.orientacao =
        Mat3::olhandoPara(posicao + rotacao.frente() * 14.0f - camera.posicao, cameraCima_);
    camera.fovGraus = fov_;

    cena_.definirCamera(camera);
    cena_.iniciarQuadro();

    // Vazio do espaco
    draw::retanguloTela(ctx.renderer,
                        SDL_FRect{0.0f, 0.0f, static_cast<float>(App::kLarguraLogica),
                                  static_cast<float>(App::kAlturaLogica)},
                        kCorEspaco);

    estrelas_.desenhar(ctx.renderer, cena_, rotacao.frente() * velocidade_, tempo_);

    cena_.submeter(nave_, posicao, rotacao, 1.15f);
    cena_.desenhar(ctx.renderer);

    // Escapamento: brilha mais forte no turbo.
    SDL_FPoint motor;
    float profundidade = 0.0f;
    if (cena_.projetar(posicao + rotacao * Vec3{0.0f, 0.05f, 1.75f}, motor, &profundidade)) {
        const float intensidade = 0.35f + 0.65f * fatorTurbo();
        const float tremor = 0.9f + 0.1f * std::sin(tempo_ * 30.0f);
        brilhoAditivo(ctx.renderer, motor, cena_.escalaEmTela(profundidade) * 0.55f * tremor,
                      SDL_FColor{1.0f * intensidade, 0.55f * intensidade, 0.22f * intensidade,
                                 1.0f});
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
    std::snprintf(linha, sizeof(linha), "VEL %3.0f u/s%s", static_cast<double>(velocidade_),
                  turbo_ ? "  [TURBO]" : "");
    const SDL_FPoint tamanho = ctx.fonte.medir(linha, 1.0f);
    draw::retanguloTela(ctx.renderer, SDL_FRect{8.0f, 8.0f, tamanho.x + 16.0f, tamanho.y + 12.0f},
                        kCorPainel);
    ctx.fonte.desenhar(ctx.renderer, linha, 16.0f, 14.0f,
                       turbo_ ? SDL_Color{255, 200, 130, 255} : kCorHud, 1.0f);

    const char* dica = ctx.input.temGamepad()
                           ? "analogico: pilotar   A: turbo   B: voltar"
                           : "WASD: pilotar   Espaco: turbo   Esc: voltar";
    const SDL_FPoint tamanhoDica = ctx.fonte.medir(dica, 1.0f);
    const float yDica = static_cast<float>(App::kAlturaLogica) - 24.0f;
    draw::retanguloTela(ctx.renderer,
                        SDL_FRect{cx - tamanhoDica.x * 0.5f - 8.0f, yDica - 4.0f,
                                  tamanhoDica.x + 16.0f, tamanhoDica.y + 8.0f},
                        kCorPainel);
    ctx.fonte.desenharCentralizado(ctx.renderer, dica, cx, yDica, kCorHud, 1.0f);
}

}  // namespace jogo

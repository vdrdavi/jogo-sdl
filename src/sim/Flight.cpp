#include "sim/Flight.hpp"

#include <algorithm>
#include <cmath>

#include "core/Context.hpp"

namespace jogo {
namespace {

constexpr float kTaxaGiro = 1.15f;    // rad/s
constexpr float kTaxaPitch = 0.95f;   // rad/s
constexpr float kLimitePitch = 1.15f; // rad

// Campo de asteroides: o cubo com wrap e tambem o alcance de desenho.
constexpr float kRaioCampo = 110.0f;
constexpr int kQuantidadeRochas = 200;
constexpr float kRaioNave = 2.0f;

// Batida: a nave quase para e o baque decai por si.
constexpr float kVelocidadeAposBatida = 18.0f;
constexpr float kDecaimentoBatida = 3.4f;  // 1/s
/// Quanto do casco cada rocha leva embora: oito batidas do casco inteiro ao
/// zero, o bastante para o mostrador do painel andar de forma visivel sem que
/// uma distracao acabe a viagem.
constexpr float kDanoPorBatida = 0.125f;

// Ruido do casco: sempre presente, mais forte quando o motor abre.
constexpr float kAmbienteCruzeiro = 0.45f;
constexpr float kAmbienteTurbo = 0.9f;
constexpr float kTaxaAmbiente = 2.0f;   // 1/s
constexpr float kAmbienteSaida = 0.35f; // s de fade ao encerrar
/// Quanto do lado de fora atravessa o casco.
constexpr float kAbafamento = 0.34f;

/// Suavizacao exponencial estavel em passo fixo.
float aproximar(float atual, float alvo, float taxa, float dt) {
    return atual + (alvo - atual) * (1.0f - std::exp(-taxa * dt));
}

}  // namespace

void Flight::iniciar(Context& ctx, Uint32 semente) {
    pose_ = Pose{};
    poseAnterior_ = pose_;
    velocidade_ = kVelocidadeCruzeiro;
    turbo_ = false;
    batida_ = 0.0f;
    casco_ = 1.0f;
    abafado_ = true;

    rochas_.gerar(semente, kQuantidadeRochas, kRaioCampo);
    rochas_.centralizar(pose_.posicao);

    // O ambiente comeca mudo e sobe em atualizar(): entrar na viagem nao
    // estoura um rugido do nada.
    ambiente_ = 0.0f;
    somAmbiente_ = ctx.audio.carregar("audio/espaco.wav");
    somImpacto_ = ctx.audio.carregar("audio/impacto.wav");
    somDestruicao_ = ctx.audio.carregar("audio/destruicao.wav");
    vozAmbiente_ = ctx.audio.tocarEmLoop(somAmbiente_, ambiente_);
}

void Flight::encerrar(Context& ctx) {
    ctx.audio.parar(vozAmbiente_, kAmbienteSaida);
    vozAmbiente_ = 0;
}

float Flight::fatorTurbo() const {
    return (velocidade_ - kVelocidadeCruzeiro) / (kVelocidadeTurbo - kVelocidadeCruzeiro);
}

Flight::Pose Flight::interpolada(float alpha) const {
    Pose pose;
    pose.posicao = lerp(poseAnterior_.posicao, pose_.posicao, alpha);
    pose.yaw = poseAnterior_.yaw + (pose_.yaw - poseAnterior_.yaw) * alpha;
    pose.pitch = poseAnterior_.pitch + (pose_.pitch - poseAnterior_.pitch) * alpha;
    pose.roll = poseAnterior_.roll + (pose_.roll - poseAnterior_.roll) * alpha;
    return pose;
}

void Flight::atualizar(Context& ctx, float dt, const Comando& comando) {
    poseAnterior_ = pose_;

    // Uma nave perdida nao obedece a ninguem: sem motor e sem atrito no vazio,
    // o que sobra dela segue na direcao e na velocidade em que estava. Por isso
    // so a integracao da posicao fica de fora deste "se" -- e e ela que mantem
    // a camera com o que seguir enquanto os destrocos se abrem.
    if (!destruida()) {
        // Curva inclinada: o rolamento acompanha a guinada, como em um caca. Sem
        // comando isso tudo tende a zero e a nave segue reto.
        pose_.roll = aproximar(pose_.roll, -comando.eixo.x * 0.85f, 6.0f, dt);
        pose_.yaw -= comando.eixo.x * kTaxaGiro * dt;
        pose_.pitch =
            std::clamp(pose_.pitch - comando.eixo.y * kTaxaPitch * dt, -kLimitePitch,
                       kLimitePitch);

        turbo_ = comando.turbo;
        velocidade_ =
            aproximar(velocidade_, turbo_ ? kVelocidadeTurbo : kVelocidadeCruzeiro, 3.0f, dt);
    }
    pose_.posicao += rotacaoDe(pose_).frente() * velocidade_ * dt;

    // Campo infinito: as rochas sao reposicionadas em torno da nave, que e
    // quem colide com elas -- enquanto ha nave para colidir.
    rochas_.atualizar(dt);
    rochas_.centralizar(pose_.posicao);
    if (!destruida()) {
        checarColisao(ctx);
    }
    batida_ = std::max(0.0f, batida_ - kDecaimentoBatida * dt);

    // O ambiente entra do zero, acompanha o esforco do motor e cai quando o
    // casco fica no caminho. A rampa e aqui para a passagem entre o convés e a
    // cabine ser um swell, e nao um degrau -- e e a mesma rampa que, com a nave
    // perdida, leva o alvo a zero: a sequencia de destruicao termina em silencio,
    // que e de onde a tela de fim comeca.
    const float alvo =
        destruida() ? 0.0f
                    : (kAmbienteCruzeiro + (kAmbienteTurbo - kAmbienteCruzeiro) * fatorTurbo()) *
                          (abafado_ ? kAbafamento : 1.0f);
    ambiente_ = aproximar(ambiente_, alvo, kTaxaAmbiente, dt);
    ctx.audio.ajustarGanho(vozAmbiente_, ambiente_);
}

void Flight::checarColisao(Context& ctx) {
    const int atingida = rochas_.colisao(pose_.posicao, kRaioNave);
    if (atingida < 0) {
        return;
    }

    // A rocha vai para outro canto do cubo em vez de sumir: o campo mantem a
    // mesma densidade sem alocar nada.
    rochas_.reposicionar(atingida, pose_.posicao);
    velocidade_ = kVelocidadeAposBatida;
    batida_ = 1.0f;
    // O estrago nao se desfaz: o casco so cai, e para no zero.
    casco_ = std::max(0.0f, casco_ - kDanoPorBatida);

    // A ultima rocha soa diferente das outras, e o estouro sai daqui e nao da
    // cena: o casco pode ceder com o jogador no conves, no painel ou na cabine,
    // e o som do fim da nave nao pode depender de quem estava desenhando.
    if (destruida()) {
        ctx.audio.tocar(somDestruicao_);
    } else {
        ctx.audio.tocar(somImpacto_, abafado_ ? 0.55f : 1.0f);
    }
}

}  // namespace jogo

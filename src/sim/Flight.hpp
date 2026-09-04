#pragma once

#include <SDL3/SDL.h>

#include "audio/Audio.hpp"
#include "gfx3d/AsteroidField.hpp"
#include "gfx3d/Math3D.hpp"

namespace jogo {

struct Context;

/// O voo da nave: para onde ela vai, por onde passa e no que bate.
///
/// Isto e estado da viagem, nao de uma tela. A nave continua voando -- e
/// batendo -- enquanto o piloto anda la dentro, entao quem guarda o Flight e a
/// InteriorScene (a nave em que se anda e a mesma que voa) e a FlightScene so
/// pilota e desenha o mesmo estado. Sem comando o voo segue reto na velocidade
/// de cruzeiro: e o piloto automatico, sem nenhum codigo a mais.
///
/// O ambiente sonoro tambem vive aqui, pelo mesmo motivo: ele acompanha a
/// viagem, nao a cena. La dentro o casco o abafa (`definirAbafado`). O alarme
/// do casco critico segue a mesma regra: a nave e que soa, nao a tela.
class Flight {
public:
    /// Comando do piloto no passo; tudo zerado e piloto automatico.
    struct Comando {
        SDL_FPoint eixo{0.0f, 0.0f};
        bool turbo{false};
    };

    /// Estado interpolavel entre dois passos fixos.
    struct Pose {
        Vec3 posicao{};
        float yaw{0.0f};
        float pitch{0.0f};
        float roll{0.0f};
    };

    static constexpr float kVelocidadeCruzeiro = 62.0f;
    static constexpr float kVelocidadeTurbo = 185.0f;

    /// Ate aqui o casco esta em estado critico. A fronteira e uma so para o
    /// alarme, a luz de emergencia e a palavra do diagnostico nunca se
    /// contradizerem -- a sirene nao pode tocar sobre um mostrador que ainda
    /// diz AVARIADO.
    static constexpr float kCascoCritico = 0.3f;

    /// Comeca a viagem: sorteia o campo de rochas e acende o ambiente.
    void iniciar(Context& ctx, Uint32 semente);
    /// Encerra a viagem, apagando o ambiente em fade.
    void encerrar(Context& ctx);

    void atualizar(Context& ctx, float dt, const Comando& comando);

    /// Liga enquanto o jogador estiver no interior: o casco abafa o lado de fora.
    void definirAbafado(bool abafado) { abafado_ = abafado; }

#ifdef JOGO_DEBUG
    /// A trapaca de quem desenvolve, que o F4 liga e desliga (so na build de
    /// depuracao): a nave atravessa as rochas sem bater -- sem baque, sem som e
    /// sem estrago. E o jeito de olhar o campo, o ambiente ou uma cena demorada
    /// sem que a viagem acabe no meio da conferencia.
    ///
    /// Ela **nao conserta nada**: o casco fica no valor em que estava, com o
    /// alarme que estiver tocando, e uma nave ja perdida continua perdida --
    /// invencivel e nao levar dano novo, nao voltar do fim. E sobrevive a um
    /// `iniciar()`: quem ligou a trapaca nao a perde ao recomecar a viagem.
    void alternarInvencivel() { invencivel_ = !invencivel_; }
    bool invencivel() const { return invencivel_; }
#else
    /// Fora da build de depuracao a trapaca nao existe. Este `false` constante
    /// e o que apaga a checagem no passo do voo sem espalhar `#ifdef` por ele.
    static constexpr bool invencivel() { return false; }
#endif

    static Mat3 rotacaoDe(const Pose& pose) {
        return Mat3::deEuler(pose.yaw, pose.pitch, pose.roll);
    }

    const Pose& pose() const { return pose_; }
    Pose interpolada(float alpha) const;

    float velocidade() const { return velocidade_; }
    bool turbo() const { return turbo_; }
    /// 0 no cruzeiro, 1 no turbo: a medida de esforco do motor.
    float fatorTurbo() const;
    /// 1 no instante da batida, decai ate zero. Cada cena sacode do seu jeito.
    float batida() const { return batida_; }
    /// Integridade do casco em 0..1: comeca inteira e cai a cada batida. Como o
    /// resto da viagem, e estado do Flight -- a nave leva o estrago batendo com
    /// o piloto no conves tanto quanto na cabine.
    float casco() const { return casco_; }
    /// Casco zerado e nave perdida: nao ha um segundo estado para manter em dia.
    /// A partir daqui o Flight nao manobra, nao acelera e nao colide -- so
    /// carrega para a frente o que sobrou, para a camera ter o que seguir
    /// enquanto a cena mostra os destrocos.
    bool destruida() const { return casco_ <= 0.0f; }
    /// Casco no fim, e ainda ha nave para alarmar: uma nave ja perdida nao
    /// avisa mais ninguem.
    bool critico() const { return !destruida() && casco_ <= kCascoCritico; }
    /// O ciclo do alarme, de 0 a 1: fechado no vale, aberto no pico. E o mesmo
    /// numero que abre o ganho da sirene e acende a luz vermelha do conves --
    /// um so, para que luz e som nunca pisquem separados (veja atualizar()).
    float alarme() const { return alarme_; }

    const AsteroidField& rochas() const { return rochas_; }

private:
    void checarColisao(Context& ctx);

    Pose pose_;
    Pose poseAnterior_;
    AsteroidField rochas_;

    float velocidade_{kVelocidadeCruzeiro};
    float batida_{0.0f};
    float casco_{1.0f};
    bool turbo_{false};

    float ambiente_{0.0f};
    /// Onde o ciclo do alarme esta (rad) e quanto dele passa: a fase anda
    /// sempre, a intensidade e que entra e sai em rampa.
    float faseAlarme_{0.0f};
    float intensidadeAlarme_{0.0f};
    float alarme_{0.0f};
    bool abafado_{true};
#ifdef JOGO_DEBUG
    bool invencivel_{false};
#endif
    Audio::SomId somAmbiente_{0};
    Audio::SomId somImpacto_{0};
    Audio::SomId somDestruicao_{0};
    Audio::SomId somSirene_{0};
    Audio::VozId vozAmbiente_{0};
    Audio::VozId vozSirene_{0};
};

}  // namespace jogo

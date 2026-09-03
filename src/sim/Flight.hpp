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
/// viagem, nao a cena. La dentro o casco o abafa (`definirAbafado`).
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

    /// Comeca a viagem: sorteia o campo de rochas e acende o ambiente.
    void iniciar(Context& ctx, Uint32 semente);
    /// Encerra a viagem, apagando o ambiente em fade.
    void encerrar(Context& ctx);

    void atualizar(Context& ctx, float dt, const Comando& comando);

    /// Liga enquanto o jogador estiver no interior: o casco abafa o lado de fora.
    void definirAbafado(bool abafado) { abafado_ = abafado; }

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
    bool abafado_{true};
    Audio::SomId somAmbiente_{0};
    Audio::SomId somImpacto_{0};
    Audio::VozId vozAmbiente_{0};
};

}  // namespace jogo

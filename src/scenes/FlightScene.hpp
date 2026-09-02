#pragma once

#include <SDL3/SDL.h>

#include "audio/Audio.hpp"
#include "gfx3d/AsteroidField.hpp"
#include "gfx3d/Mesh.hpp"
#include "gfx3d/Renderer3D.hpp"
#include "gfx3d/Starfield.hpp"
#include "scene/Scene.hpp"

namespace jogo {

/// Visao 3D: a nave low poly voando por um campo de estrelas procedural.
/// Empilhada sobre a InteriorScene quando o jogador usa o painel; Esc volta.
class FlightScene : public Scene {
public:
    FlightScene();

    void aoEntrar(Context& ctx) override;
    void aoSair(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

private:
    static constexpr float kVelocidadeCruzeiro = 62.0f;
    static constexpr float kVelocidadeTurbo = 185.0f;
    static constexpr float kTaxaGiro = 1.15f;    // rad/s
    static constexpr float kTaxaPitch = 0.95f;   // rad/s
    static constexpr float kLimitePitch = 1.15f; // rad
    static constexpr float kFovBase = 62.0f;
    static constexpr float kFovTurbo = 80.0f;
    // Ruido do casco: sempre presente, mais forte quando o motor abre.
    static constexpr float kAmbienteCruzeiro = 0.45f;
    static constexpr float kAmbienteTurbo = 0.9f;
    static constexpr float kTaxaAmbiente = 2.0f;    // 1/s: sobe do zero ao entrar
    static constexpr float kAmbienteSaida = 0.35f;  // s de fade ao sair
    // Campo de asteroides: o cubo com wrap e tambem o alcance de desenho, e a
    // nevoa comeca antes dele para as rochas emergirem do vazio.
    static constexpr float kRaioCampo = 110.0f;
    static constexpr int kQuantidadeRochas = 200;
    static constexpr float kNevoaInicio = 45.0f;
    static constexpr float kRaioNave = 2.0f;
    // Batida: a nave quase para, sacode e clareia. Tudo decai por si.
    static constexpr float kVelocidadeAposBatida = 18.0f;
    static constexpr float kDecaimentoBatida = 3.4f;  // 1/s
    static constexpr float kAmplitudeTremor = 0.55f;  // unidades de mundo

    /// 0 no cruzeiro, 1 no turbo: liga o brilho do escapamento e o volume do
    /// ambiente a mesma medida de esforco do motor.
    float fatorTurbo() const;

    /// Esfera-esfera contra as rochas; a atingida sai de cena e a nave leva o
    /// tranco.
    void checarColisao(Context& ctx);

    /// Estado interpolavel entre dois passos fixos.
    struct Pose {
        Vec3 posicao{};
        float yaw{0.0f};
        float pitch{0.0f};
        float roll{0.0f};
        Vec3 camera{};
    };

    Pose pose_;
    Pose poseAnterior_;

    Renderer3D cena_;
    Starfield estrelas_;
    AsteroidField rochas_;
    Mesh nave_;

    Vec3 cameraCima_{0.0f, 1.0f, 0.0f};
    float velocidade_{kVelocidadeCruzeiro};
    float fov_{kFovBase};
    float tempo_{0.0f};
    float ambiente_{0.0f};
    /// 1 no instante da batida, decai ate zero: comanda o tremor e o clarao.
    float batida_{0.0f};
    bool turbo_{false};

    Audio::SomId somSaida_{0};
    Audio::SomId somAmbiente_{0};
    Audio::SomId somImpacto_{0};
    Audio::VozId vozAmbiente_{0};
};

}  // namespace jogo

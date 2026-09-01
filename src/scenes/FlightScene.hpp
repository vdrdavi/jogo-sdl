#pragma once

#include <SDL3/SDL.h>

#include "audio/Audio.hpp"
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
    Mesh nave_;

    Vec3 cameraCima_{0.0f, 1.0f, 0.0f};
    float velocidade_{kVelocidadeCruzeiro};
    float fov_{kFovBase};
    float tempo_{0.0f};
    bool turbo_{false};

    Audio::SomId somSaida_{0};
};

}  // namespace jogo

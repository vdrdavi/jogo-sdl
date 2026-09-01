#pragma once

#include "audio/Audio.hpp"
#include "scene/Scene.hpp"

namespace jogo {

/// Overlay de pausa: congela a cena de baixo (bloqueiaUpdate) mas deixa ela
/// visivel atras (bloqueiaRender == false).
class PauseScene : public Scene {
public:
    void aoEntrar(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

    bool bloqueiaRender() const override { return false; }

private:
    float tempo_{0.0f};
    Audio::SomId somVoltar_{0};
};

}  // namespace jogo

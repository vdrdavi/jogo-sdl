#pragma once

#include "audio/Audio.hpp"
#include "scene/Scene.hpp"

namespace jogo {

/// Overlay de pausa: congela a cena de baixo (bloqueiaUpdate) mas deixa ela
/// visivel atras (bloqueiaRender == false). Congela tambem o som, suspendendo
/// o dispositivo enquanto estiver em cena -- nao implementar `acompanhar` para
/// o mundo mais o silencio sao a mesma frase dita duas vezes.
class PauseScene : public Scene {
public:
    void aoEntrar(Context& ctx) override;
    void aoSair(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

    bool bloqueiaRender() const override { return false; }

private:
    float tempo_{0.0f};
    Audio::SomId somVoltar_{0};
};

}  // namespace jogo

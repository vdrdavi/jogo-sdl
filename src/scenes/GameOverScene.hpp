#pragma once

#include <SDL3/SDL.h>

#include "audio/Audio.hpp"
#include "scene/Scene.hpp"
#include "scenes/Transicao.hpp"

namespace jogo {

/// A tela de fim: a nave se perdeu no campo de asteroides.
///
/// Ela substitui a FlightScene depois que os destrocos se apagam, entao a pilha
/// fica MenuScene > InteriorScene > GameOverScene -- os mesmos dois degraus que
/// a pausa desfaz para voltar ao menu, seja qual for o caminho pelo qual o
/// casco cedeu (conves, painel do casco ou cabine).
class GameOverScene : public Scene {
public:
    void aoEntrar(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

private:
    /// A cortina abre devagar: o fim da nave nao volta a ser jogo em 0,2 s.
    static constexpr float kAberturaCortina = 0.9f;  // s

    Transicao transicao_;
    float tempo_{0.0f};
    Audio::SomId somConfirmar_{0};
};

}  // namespace jogo

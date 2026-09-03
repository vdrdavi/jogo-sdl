#pragma once

#include <SDL3/SDL.h>

#include "audio/Audio.hpp"
#include "scene/Scene.hpp"
#include "sim/Flight.hpp"

namespace jogo {

/// A outra opcao do painel: o diagnostico do casco, aberto com Q no console e
/// fechado com Esc. Mostra a integridade da nave, que cai a cada rocha.
///
/// Como a FlightScene, esta cena nao e dona do voo -- guarda uma referencia
/// para o Flight da InteriorScene, que sempre sobrevive a ela (a pilha so
/// desempilha do topo). E, como ela, bloqueia o update da cena de baixo e passa
/// a ser quem chama Flight::atualizar: a nave continua voando em piloto
/// automatico enquanto o painel esta aberto, ainda um passo por passo fixo, e
/// uma batida faz o mostrador cair na frente de quem esta lendo.
class StatusScene : public Scene {
public:
    explicit StatusScene(Flight& voo);

    void aoEntrar(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

    /// O conves fica visivel atras do painel, como na pausa.
    bool bloqueiaRender() const override { return false; }

private:
    static constexpr float kAlturaBarra = 16.0f;
    /// Com que taxa o ponteiro persegue o casco: lento o bastante para a queda
    /// ser vista, rapido o bastante para nao atrasar a leitura.
    static constexpr float kTaxaPonteiro = 5.0f;  // 1/s

    /// A nave diagnosticada; vive na cena de baixo.
    Flight& voo_;

    /// O que o mostrador exibe, perseguindo voo_.casco(). A diferenca entre os
    /// dois e o pedaco que acabou de ser arrancado, e e desenhada em vermelho.
    float ponteiro_{1.0f};
    float tempo_{0.0f};

    Audio::SomId somVoltar_{0};
};

}  // namespace jogo

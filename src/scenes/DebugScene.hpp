#pragma once

#include "scene/Scene.hpp"

namespace jogo {

class Flight;
class SceneStack;

/// A tela de depuracao, que so existe na build de depuracao (JOGO_DEBUG): F3
/// abre e fecha, de qualquer lugar do jogo. Mostra o que nao aparece em nenhum
/// HUD -- o desempenho (quadros por segundo, passo fixo, tamanho da pilha),
/// onde o jogo esta rodando (sistema, drivers de video e audio, renderer,
/// janela, caminhos de arquivo) e o estado da nave.
///
/// E um overlay como a PauseScene: congela quem esta embaixo e deixa a cena
/// aparecer atras do veu. Por congelar, herda a obrigacao de quem bloqueia o
/// update -- enquanto esta no topo, e ela quem da o passo do voo, uma vez por
/// passo fixo e em piloto automatico, para a viagem nao parar (nem andar duas
/// vezes) so porque alguem abriu o painel.
///
/// O voo nao e dela. Ela o encontra na pilha, e pode nao encontrar nenhum: no
/// menu ainda nao ha viagem, e a secao da nave diz isso.
class DebugScene : public Scene {
public:
    /// Abre a tela, ou fecha a que ja estiver aberta. E o gancho do F3 no App,
    /// e e estatica de proposito: quem sabe se a tela esta aberta e o proprio
    /// topo da pilha, nao uma segunda variavel para manter em dia.
    static void alternar(SceneStack& cenas);

    void aoEntrar(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

    /// A cena de baixo continua visivel atras do veu, como na pausa.
    bool bloqueiaRender() const override { return false; }

private:
    /// A viagem em curso, procurada na pilha ao entrar; nullptr quando nao ha
    /// nenhuma. O ponteiro vale enquanto esta tela existir: ela bloqueia o
    /// update de quem esta embaixo, entao ninguem abaixo dela mexe na pilha.
    Flight* voo_{nullptr};
};

}  // namespace jogo

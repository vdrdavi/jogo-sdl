#pragma once

#include <SDL3/SDL.h>

#include <memory>

#include "core/Context.hpp"

namespace jogo {

/// Uma tela do jogo (menu, partida, pausa...). Sempre vive dentro de uma
/// SceneStack, que decide quem recebe update e quem e desenhado.
class Scene {
public:
    virtual ~Scene() = default;

    virtual void aoEntrar(Context&) {}
    virtual void aoSair(Context&) {}
    /// A cena de cima desempilhou e esta voltou a ser o topo. E o gancho que
    /// falta ao par aoEntrar/aoSair: quem empilha sabe quando saiu de cena, mas
    /// nao quando volta (a InteriorScene reabre a cortina daqui).
    virtual void aoRetomar(Context&) {}

    /// Eventos brutos; so a cena do topo recebe.
    virtual void aoEvento(Context&, const SDL_Event&) {}

    /// Passo fixo de simulacao (StepTimer::kPassoFixo).
    virtual void atualizar(Context&, float dt) = 0;

    /// `alpha` e a fracao do passo ainda nao simulada, para interpolar o desenho.
    virtual void desenhar(Context&, float alpha) = 0;

    /// O passo de uma cena congelada por um overlay que quer deixar o mundo
    /// andar -- a tela de depuracao, que bloqueia o update sem parar a viagem.
    /// E o `atualizar` dela em piloto automatico: o passo do voo, se e ela quem
    /// o da, mais o que **persegue** a simulacao (camera, campo de visao,
    /// mostradores, o relogio das animacoes). Nao le entrada e nao mexe na
    /// pilha -- decisoes esperam o painel sair da frente.
    ///
    /// Nao implementar isto e dizer que a cena para de verdade quando alguem se
    /// poe na frente dela: e o que a pausa quer, e o que o menu significa. Por
    /// isso e cada cena que responde, e nao o overlay que adivinha por ela.
    virtual void acompanhar(Context&, float /*dt*/) {}

    /// false deixa a cena de baixo continuar simulando (HUD sobreposto).
    virtual bool bloqueiaUpdate() const { return true; }
    /// false deixa a cena de baixo aparecer atras (overlay translucido).
    virtual bool bloqueiaRender() const { return true; }
};

using ScenePtr = std::unique_ptr<Scene>;

}  // namespace jogo

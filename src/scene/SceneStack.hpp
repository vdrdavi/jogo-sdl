#pragma once

#include <SDL3/SDL.h>

#include <cstddef>
#include <vector>

#include "scene/Scene.hpp"

namespace jogo {

/// Pilha de cenas com transicoes adiadas: push/pop/replace so tomam efeito no
/// fim do quadro, para uma cena poder trocar a si mesma no meio do proprio
/// update sem invalidar a pilha que esta sendo percorrida.
class SceneStack {
public:
    void empilhar(ScenePtr cena);
    void desempilhar();
    void substituir(ScenePtr cena);
    void limpar();

    void aoEvento(Context& ctx, const SDL_Event& evento);
    void atualizar(Context& ctx, float dt);
    /// A mesma varredura de atualizar(), mas comecando **abaixo do topo** e
    /// chamando acompanhar(): e como um overlay que congela quem esta embaixo
    /// e mesmo assim deixa o mundo andar (a tela de depuracao, que segue dando
    /// o passo do voo) devolve o passo de apresentacao a quem congelou.
    void acompanharAbaixoDoTopo(Context& ctx, float dt);
    void desenhar(Context& ctx, float alpha);

    /// Aplica as transicoes pendentes; chamado pelo App no fim do quadro.
    void aplicarPendentes(Context& ctx);

    bool vazia() const { return pilha_.empty(); }
    Scene* topo() { return pilha_.empty() ? nullptr : pilha_.back().get(); }

    /// A pilha por dentro, da base (0) para o topo. O jogo se move sempre pelo
    /// topo; quem olha para baixo e a tela de depuracao (F3), que pode ser
    /// aberta em qualquer lugar e precisa achar a viagem em curso sem saber de
    /// antemao qual cena a guarda.
    std::size_t tamanho() const { return pilha_.size(); }
    Scene* em(std::size_t indice) { return pilha_[indice].get(); }

private:
    enum class Tipo { Empilhar, Desempilhar, Substituir, Limpar };
    struct Comando {
        Tipo tipo;
        ScenePtr cena;
    };

    std::vector<ScenePtr> pilha_;
    std::vector<Comando> pendentes_;
};

}  // namespace jogo

#pragma once

#include <SDL3/SDL.h>

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
    void desenhar(Context& ctx, float alpha);

    /// Aplica as transicoes pendentes; chamado pelo App no fim do quadro.
    void aplicarPendentes(Context& ctx);

    bool vazia() const { return pilha_.empty(); }
    Scene* topo() { return pilha_.empty() ? nullptr : pilha_.back().get(); }

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

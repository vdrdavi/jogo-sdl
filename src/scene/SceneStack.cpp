#include "scene/SceneStack.hpp"

namespace jogo {

void SceneStack::empilhar(ScenePtr cena) {
    pendentes_.push_back(Comando{Tipo::Empilhar, std::move(cena)});
}

void SceneStack::desempilhar() {
    pendentes_.push_back(Comando{Tipo::Desempilhar, nullptr});
}

void SceneStack::substituir(ScenePtr cena) {
    pendentes_.push_back(Comando{Tipo::Substituir, std::move(cena)});
}

void SceneStack::limpar() {
    pendentes_.push_back(Comando{Tipo::Limpar, nullptr});
}

void SceneStack::aoEvento(Context& ctx, const SDL_Event& evento) {
    if (!pilha_.empty()) {
        pilha_.back()->aoEvento(ctx, evento);
    }
}

void SceneStack::atualizar(Context& ctx, float dt) {
    // De cima para baixo, ate encontrar uma cena que bloqueie o update.
    for (std::size_t i = pilha_.size(); i-- > 0;) {
        pilha_[i]->atualizar(ctx, dt);
        if (pilha_[i]->bloqueiaUpdate()) {
            break;
        }
    }
}

void SceneStack::desenhar(Context& ctx, float alpha) {
    if (pilha_.empty()) {
        return;
    }
    // Descobre a cena mais alta que preenche a tela e desenha dali para cima.
    std::size_t inicio = 0;
    for (std::size_t i = pilha_.size(); i-- > 0;) {
        if (pilha_[i]->bloqueiaRender()) {
            inicio = i;
            break;
        }
    }
    for (std::size_t i = inicio; i < pilha_.size(); ++i) {
        pilha_[i]->desenhar(ctx, alpha);
    }
}

void SceneStack::aplicarPendentes(Context& ctx) {
    if (pendentes_.empty()) {
        return;
    }
    // Move a lista: uma cena criada dentro de aoEntrar/aoSair pode enfileirar
    // novos comandos, que ficam para o proximo quadro.
    std::vector<Comando> comandos = std::move(pendentes_);
    pendentes_.clear();

    for (Comando& comando : comandos) {
        switch (comando.tipo) {
            case Tipo::Empilhar:
                pilha_.push_back(std::move(comando.cena));
                pilha_.back()->aoEntrar(ctx);
                break;

            case Tipo::Desempilhar:
                if (!pilha_.empty()) {
                    pilha_.back()->aoSair(ctx);
                    pilha_.pop_back();
                }
                break;

            case Tipo::Substituir:
                if (!pilha_.empty()) {
                    pilha_.back()->aoSair(ctx);
                    pilha_.pop_back();
                }
                pilha_.push_back(std::move(comando.cena));
                pilha_.back()->aoEntrar(ctx);
                break;

            case Tipo::Limpar:
                while (!pilha_.empty()) {
                    pilha_.back()->aoSair(ctx);
                    pilha_.pop_back();
                }
                break;
        }
    }
}

}  // namespace jogo

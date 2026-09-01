#pragma once

#include <SDL3/SDL.h>

namespace jogo {

/// Acumulador de passo fixo: a simulacao sempre avanca em fatias iguais,
/// independente da taxa de quadros, e o resto vira o fator de interpolacao
/// (alpha) usado na renderizacao.
class StepTimer {
public:
    static constexpr float kPassoFixo = 1.0f / 60.0f;
    /// Limite por quadro: sem ele, um travamento longo geraria uma avalanche de
    /// updates ("spiral of death").
    static constexpr float kMaximoPorQuadro = 0.25f;

    void iniciar() {
        ultimoNs_ = SDL_GetTicksNS();
        acumulador_ = 0.0f;
    }

    /// Deve ser chamado uma vez por quadro; devolve o tempo real decorrido.
    float novoQuadro() {
        const Uint64 agoraNs = SDL_GetTicksNS();
        const Uint64 deltaNs = agoraNs - ultimoNs_;
        ultimoNs_ = agoraNs;

        float delta = static_cast<float>(static_cast<double>(deltaNs) / 1e9);
        if (delta > kMaximoPorQuadro) {
            delta = kMaximoPorQuadro;
        }
        acumulador_ += delta;
        return delta;
    }

    /// Consome uma fatia fixa do acumulador; usar em `while (timer.consumirPasso())`.
    bool consumirPasso() {
        if (acumulador_ < kPassoFixo) {
            return false;
        }
        acumulador_ -= kPassoFixo;
        return true;
    }

    /// Fracao do passo ainda nao simulada, em [0, 1): interpolacao do render.
    float alpha() const { return acumulador_ / kPassoFixo; }

private:
    Uint64 ultimoNs_{0};
    float acumulador_{0.0f};
};

}  // namespace jogo

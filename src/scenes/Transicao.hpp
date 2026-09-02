#pragma once

#include <SDL3/SDL.h>

namespace jogo {

/// A cortina que costura o convés a cabine: as duas cenas escurecem ate a mesma
/// cor e reabrem dela, entao o que era um corte seco vira uma passagem so.
///
/// Nao e uma cena, e estado que cada cena guarda e desenha por cima de si
/// mesma. Uma cena de overlay teria que sobreviver a troca da cena de baixo --
/// e a pilha so empilha e desempilha no topo -- e, cobrindo o topo, tomaria de
/// quem esta embaixo o passo de simulacao do voo.
class Transicao {
public:
    /// Quanto dura cada metade. A soma e o tempo entre apertar E e estar
    /// pilotando: curto o bastante para nao virar espera.
    static constexpr float kDuracaoSaida = 0.24f;
    static constexpr float kDuracaoEntrada = 0.20f;

    /// A cor da cortina e o vidro escuro do painel, entre o azul do conves e o
    /// vazio do espaco: e nela que as duas metades emendam.
    static constexpr SDL_Color kCor{14, 30, 48, 255};

    /// Fecha a cortina; quando termina, fica fechada ate alguem reabrir.
    void iniciarSaida();
    /// Abre a cortina a partir da tela cheia.
    void iniciarEntrada();

    /// Avanca um passo fixo. Devolve true no unico passo em que a saida acaba
    /// de cobrir a tela -- o instante de trocar de cena.
    bool avancar(float dt);

    bool ativa() const { return modo_ != Modo::Nenhuma; }
    bool saindo() const { return modo_ == Modo::Saindo; }

    /// 0 = tela limpa, 1 = tela inteira na cor da cortina.
    float cobertura() const;

    /// Por cima de tudo, inclusive do HUD.
    void desenhar(SDL_Renderer* renderer) const;

private:
    enum class Modo { Nenhuma, Saindo, Entrando };

    Modo modo_{Modo::Nenhuma};
    float t_{0.0f};  // progresso da metade atual, 0 a 1
};

}  // namespace jogo

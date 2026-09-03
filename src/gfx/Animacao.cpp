#include "gfx/Animacao.hpp"

namespace jogo {

void Animacao::tocar(const Clipe& clipe) {
    const bool outro = clipe.linha != clipe_.linha;
    clipe_ = clipe;
    if (outro) {
        reiniciar();
    }
    // Um clipe mais curto que o anterior pode deixar o quadro fora da linha.
    if (clipe_.quadros > 0 && quadro_ >= clipe_.quadros) {
        quadro_ = 0;
    }
}

void Animacao::reiniciar() {
    tempo_ = 0.0f;
    quadro_ = 0;
}

void Animacao::atualizar(float dt) {
    if (clipe_.quadros <= 1 || clipe_.fps <= 0.0f) {
        return;
    }
    const float duracao = 1.0f / clipe_.fps;

    // O laco desconta a duracao em vez de zerar o acumulador: o que sobrou de um
    // quadro conta para o proximo, senao um clipe rapido demais para o passo
    // fixo (fps > 60) andaria devagar e a cadencia dependeria do dt.
    tempo_ += dt;
    while (tempo_ >= duracao) {
        tempo_ -= duracao;
        quadro_ = (quadro_ + 1) % clipe_.quadros;
    }
}

SDL_FRect Animacao::recorte() const {
    return SDL_FRect{static_cast<float>(quadro_) * celula_.x,
                     static_cast<float>(clipe_.linha) * celula_.y, celula_.x, celula_.y};
}

}  // namespace jogo

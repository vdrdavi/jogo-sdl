#include "scenes/Transicao.hpp"

#include <algorithm>

#include "core/App.hpp"
#include "gfx/Draw.hpp"

namespace jogo {
namespace {

/// Smoothstep: sai e chega parado, entao a cortina nao "pula" no comeco nem
/// bate no fim. Com a rampa linear a passagem parecia mais curta do que e.
float suavizar(float t) {
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

void Transicao::iniciarSaida() {
    modo_ = Modo::Saindo;
    t_ = 0.0f;
}

void Transicao::iniciarEntrada() {
    modo_ = Modo::Entrando;
    t_ = 0.0f;
}

bool Transicao::avancar(float dt) {
    if (modo_ == Modo::Nenhuma) {
        return false;
    }

    const float anterior = t_;
    t_ = std::min(1.0f, t_ + dt / (modo_ == Modo::Saindo ? kDuracaoSaida : kDuracaoEntrada));

    if (modo_ == Modo::Entrando) {
        if (t_ >= 1.0f) {
            modo_ = Modo::Nenhuma;
        }
        return false;
    }
    // A saida nao se desliga sozinha: a tela fica coberta ate a cena de destino
    // aparecer -- e, na volta, e desta cobertura que a entrada continua.
    return anterior < 1.0f && t_ >= 1.0f;
}

float Transicao::cobertura() const {
    switch (modo_) {
        case Modo::Saindo:
            return suavizar(t_);
        case Modo::Entrando:
            return 1.0f - suavizar(t_);
        case Modo::Nenhuma:
            break;
    }
    return 0.0f;
}

void Transicao::desenhar(SDL_Renderer* renderer) const {
    const float alfa = cobertura();
    if (alfa <= 0.0f) {
        return;
    }
    draw::retanguloTela(renderer,
                        SDL_FRect{0.0f, 0.0f, static_cast<float>(App::kLarguraLogica),
                                  static_cast<float>(App::kAlturaLogica)},
                        SDL_Color{kCor.r, kCor.g, kCor.b,
                                  static_cast<Uint8>(std::clamp(alfa, 0.0f, 1.0f) * 255.0f)});
}

}  // namespace jogo

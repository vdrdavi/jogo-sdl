#pragma once

#include <SDL3/SDL.h>

namespace jogo {

/// xorshift32: gerador barato e deterministico. Nao serve para criptografia --
/// serve para espalhar estrelas e rochas sempre do mesmo jeito a partir de uma
/// semente.
class Aleatorio {
public:
    explicit Aleatorio(Uint32 semente) : estado_(semente != 0 ? semente : 0x9E3779B9u) {}

    Uint32 proximo() {
        estado_ ^= estado_ << 13;
        estado_ ^= estado_ >> 17;
        estado_ ^= estado_ << 5;
        return estado_;
    }

    /// Float uniforme em [0, 1).
    float unitario() { return static_cast<float>(proximo() >> 8) / 16777216.0f; }

    float entre(float minimo, float maximo) { return minimo + unitario() * (maximo - minimo); }

private:
    Uint32 estado_;
};

}  // namespace jogo

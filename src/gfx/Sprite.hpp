#pragma once

#include <SDL3/SDL.h>

namespace jogo {

/// Descricao de um desenho: textura (ou recorte de atlas), tamanho em unidades
/// de mundo, ancora, rotacao, espelhamento e tingimento.
struct Sprite {
    SDL_Texture* textura{nullptr};
    /// Recorte no atlas. Com largura ou altura zero, usa a textura inteira.
    SDL_FRect recorte{0.0f, 0.0f, 0.0f, 0.0f};
    /// Tamanho em unidades de mundo. Com zero, usa o tamanho do recorte.
    SDL_FPoint tamanho{0.0f, 0.0f};
    /// Ancora normalizada: {0,0} canto superior esquerdo, {0.5,0.5} centro.
    SDL_FPoint ancora{0.5f, 0.5f};
    double rotacao{0.0};
    SDL_FlipMode espelho{SDL_FLIP_NONE};
    SDL_Color tinta{255, 255, 255, 255};
};

}  // namespace jogo

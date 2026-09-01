#pragma once

#include <SDL3/SDL.h>

#include "gfx/Camera.hpp"
#include "gfx/Sprite.hpp"

namespace jogo::draw {

/// Desenha um sprite posicionado em coordenadas de mundo, atraves da camera.
void sprite(SDL_Renderer* renderer, const Camera& camera, const Sprite& sprite,
            SDL_FPoint posicaoMundo);

/// Desenha um sprite direto em coordenadas de tela (HUD, menus).
void spriteTela(SDL_Renderer* renderer, const Sprite& sprite, SDL_FPoint posicaoTela,
                float escala = 1.0f);

void retanguloMundo(SDL_Renderer* renderer, const Camera& camera, const SDL_FRect& retangulo,
                    SDL_Color cor, bool preenchido = true);

void retanguloTela(SDL_Renderer* renderer, const SDL_FRect& retangulo, SDL_Color cor,
                   bool preenchido = true);

void linhaMundo(SDL_Renderer* renderer, const Camera& camera, SDL_FPoint a, SDL_FPoint b,
                SDL_Color cor);

}  // namespace jogo::draw

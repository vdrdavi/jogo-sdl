#pragma once

#include <SDL3/SDL.h>

/// Loga uma falha de chamada SDL junto do motivo devolvido por SDL_GetError().
#define JOGO_ERRO_SDL(contexto) \
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", (contexto), SDL_GetError())

#define JOGO_ERRO(...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define JOGO_INFO(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

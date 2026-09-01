#pragma once

#include <SDL3/SDL.h>

namespace jogo {

class App;
class Assets;
class Audio;
class BitmapFont;
class Input;
class SceneStack;

/// Servicos compartilhados entregues as cenas a cada chamada. Guarda apenas
/// referencias: quem e dono de tudo e o App.
struct Context {
    App& app;
    SDL_Renderer* renderer;
    Input& input;
    Assets& assets;
    Audio& audio;
    BitmapFont& fonte;
    SceneStack& cenas;
};

}  // namespace jogo

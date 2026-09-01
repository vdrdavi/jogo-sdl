#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <memory>

#include "core/App.hpp"
#include "scenes/MenuScene.hpp"

int main(int /*argc*/, char* /*argv*/[]) {
    jogo::App app;
    if (!app.iniciar("Jogo SDL3")) {
        return 1;
    }
    app.rodar(std::make_unique<jogo::MenuScene>());
    return 0;
}

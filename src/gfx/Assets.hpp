#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <string_view>
#include <unordered_map>

#include "core/SdlPtr.hpp"

namespace jogo {

/// Cache de texturas por caminho relativo a assets/. As texturas vivem enquanto
/// o Assets viver, entao as cenas podem guardar os ponteiros crus devolvidos.
class Assets {
public:
    void iniciar(SDL_Renderer* renderer) { renderer_ = renderer; }

    /// Carrega (ou devolve do cache) uma textura. Ex.: textura("textures/player.png").
    /// Devolve nullptr e loga em caso de falha.
    SDL_Texture* textura(std::string_view relativo);

    void limpar() { texturas_.clear(); }

private:
    SDL_Renderer* renderer_{nullptr};
    std::unordered_map<std::string, TexturePtr> texturas_;
};

}  // namespace jogo

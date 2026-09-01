#pragma once

#include <SDL3/SDL.h>

#include <memory>

namespace jogo {

/// Deleter generico para as funcoes de destruicao do SDL, para que todo recurso
/// viva em um unique_ptr e nao vaze em nenhum caminho de erro.
template <auto Fn>
struct SdlDeleter {
    template <typename T>
    void operator()(T* ponteiro) const noexcept {
        if (ponteiro != nullptr) {
            Fn(ponteiro);
        }
    }
};

using WindowPtr      = std::unique_ptr<SDL_Window, SdlDeleter<SDL_DestroyWindow>>;
using RendererPtr    = std::unique_ptr<SDL_Renderer, SdlDeleter<SDL_DestroyRenderer>>;
using TexturePtr     = std::unique_ptr<SDL_Texture, SdlDeleter<SDL_DestroyTexture>>;
using SurfacePtr     = std::unique_ptr<SDL_Surface, SdlDeleter<SDL_DestroySurface>>;
using GamepadPtr     = std::unique_ptr<SDL_Gamepad, SdlDeleter<SDL_CloseGamepad>>;
using AudioStreamPtr = std::unique_ptr<SDL_AudioStream, SdlDeleter<SDL_DestroyAudioStream>>;

}  // namespace jogo

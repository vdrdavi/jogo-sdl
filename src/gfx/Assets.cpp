#include "gfx/Assets.hpp"

#include "core/Log.hpp"
#include "core/Paths.hpp"

namespace jogo {
namespace {

bool terminaCom(std::string_view texto, std::string_view sufixo) {
    return texto.size() >= sufixo.size() &&
           texto.compare(texto.size() - sufixo.size(), sufixo.size(), sufixo) == 0;
}

/// SDL 3.4 carrega PNG e BMP no core, sem depender de SDL3_image.
SurfacePtr carregarSuperficie(const std::string& caminho) {
    if (terminaCom(caminho, ".png")) {
        return SurfacePtr{SDL_LoadPNG(caminho.c_str())};
    }
    if (terminaCom(caminho, ".bmp")) {
        return SurfacePtr{SDL_LoadBMP(caminho.c_str())};
    }
    JOGO_ERRO("Formato de imagem nao suportado: %s", caminho.c_str());
    return nullptr;
}

}  // namespace

SDL_Texture* Assets::textura(std::string_view relativo) {
    const std::string chave{relativo};
    if (const auto it = texturas_.find(chave); it != texturas_.end()) {
        return it->second.get();
    }

    if (renderer_ == nullptr) {
        JOGO_ERRO("Assets usado antes de iniciar()");
        return nullptr;
    }

    const std::string caminho = paths::asset(relativo);
    SurfacePtr superficie = carregarSuperficie(caminho);
    if (!superficie) {
        JOGO_ERRO_SDL(caminho.c_str());
        return nullptr;
    }

    TexturePtr textura{SDL_CreateTextureFromSurface(renderer_, superficie.get())};
    if (!textura) {
        JOGO_ERRO_SDL("SDL_CreateTextureFromSurface");
        return nullptr;
    }
    // Arte em pixel: nada de filtragem linear ao escalar.
    SDL_SetTextureScaleMode(textura.get(), SDL_SCALEMODE_NEAREST);

    SDL_Texture* cru = textura.get();
    texturas_.emplace(chave, std::move(textura));
    return cru;
}

}  // namespace jogo

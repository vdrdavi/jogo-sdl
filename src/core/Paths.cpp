#include "core/Paths.hpp"

#include <SDL3/SDL.h>

#include <filesystem>

#include "core/Log.hpp"

namespace jogo::paths {
namespace {

std::string resolver() {
    namespace fs = std::filesystem;

    if (const char* base = SDL_GetBasePath(); base != nullptr) {
        const fs::path candidato = fs::path(base) / "assets";
        std::error_code ec;
        if (fs::is_directory(candidato, ec)) {
            return candidato.string() + "/";
        }
    }

#ifdef JOGO_ASSETS_DIR
    {
        const fs::path candidato{JOGO_ASSETS_DIR};
        std::error_code ec;
        if (fs::is_directory(candidato, ec)) {
            return candidato.string() + "/";
        }
    }
#endif

    JOGO_ERRO("Diretorio de assets nao encontrado; usando \"assets/\" relativo ao cwd");
    return "assets/";
}

}  // namespace

const std::string& assetsRoot() {
    static const std::string raiz = resolver();
    return raiz;
}

std::string asset(std::string_view relativo) {
    std::string caminho = assetsRoot();
    caminho.append(relativo);
    return caminho;
}

}  // namespace jogo::paths

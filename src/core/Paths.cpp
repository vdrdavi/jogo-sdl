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

const std::string& prefRoot() {
    static const std::string raiz = [] {
        // O SDL sabe a convencao de cada sistema (~/.local/share no Linux,
        // %APPDATA% no Windows, ~/Library/Application Support no macOS) e ja
        // cria o diretorio. A string devolvida e nossa e precisa ser liberada.
        char* caminho = SDL_GetPrefPath("jogo-sdl", "jogo");
        if (caminho == nullptr) {
            JOGO_ERRO_SDL("SDL_GetPrefPath");
            return std::string{};
        }
        std::string resultado{caminho};
        SDL_free(caminho);
        return resultado;
    }();
    return raiz;
}

std::string pref(std::string_view relativo) {
    const std::string& raiz = prefRoot();
    if (raiz.empty()) {
        return std::string{};
    }
    std::string caminho = raiz;
    caminho.append(relativo);
    return caminho;
}

}  // namespace jogo::paths

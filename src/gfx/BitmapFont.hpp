#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <string_view>
#include <unordered_map>

namespace jogo {

class Assets;

/// Fonte bitmap monoespacada lida de um atlas PNG + metadados .fnt gerados por
/// tools/gen_assets.py. Evita a dependencia de SDL3_ttf e aceita UTF-8 (os
/// acentos usados em portugues estao no atlas).
class BitmapFont {
public:
    /// Ex.: carregar(assets, "fonts/mono.fnt", "fonts/mono.png").
    bool carregar(Assets& assets, std::string_view caminhoFnt, std::string_view caminhoPng);

    /// Desenha em coordenadas de tela; (x, y) e o canto superior esquerdo.
    /// Reconhece '\n'.
    void desenhar(SDL_Renderer* renderer, std::string_view texto, float x, float y,
                  SDL_Color cor = SDL_Color{255, 255, 255, 255}, float escala = 1.0f) const;

    /// Igual a desenhar(), mas centralizado horizontalmente em `centroX`.
    void desenharCentralizado(SDL_Renderer* renderer, std::string_view texto, float centroX,
                              float y, SDL_Color cor = SDL_Color{255, 255, 255, 255},
                              float escala = 1.0f) const;

    SDL_FPoint medir(std::string_view texto, float escala = 1.0f) const;
    float alturaLinha(float escala = 1.0f) const {
        return static_cast<float>(alturaCelula_) * escala;
    }
    float larguraGlifo(float escala = 1.0f) const {
        return static_cast<float>(larguraCelula_) * escala;
    }
    bool valido() const { return atlas_ != nullptr; }

private:
    SDL_Texture* atlas_{nullptr};
    int larguraCelula_{0};
    int alturaCelula_{0};
    int colunas_{16};
    std::unordered_map<Uint32, int> indicePorCodepoint_;
};

}  // namespace jogo

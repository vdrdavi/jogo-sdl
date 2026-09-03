#include "gfx/Draw.hpp"

#include <cmath>
#include <vector>

namespace jogo::draw {
namespace {

/// Recorte efetivo: se o sprite nao definir um, usa a textura inteira.
SDL_FRect recorteDe(const Sprite& s) {
    if (s.recorte.w > 0.0f && s.recorte.h > 0.0f) {
        return s.recorte;
    }
    float largura = 0.0f;
    float altura = 0.0f;
    if (s.textura != nullptr) {
        SDL_GetTextureSize(s.textura, &largura, &altura);
    }
    return SDL_FRect{0.0f, 0.0f, largura, altura};
}

SDL_FPoint tamanhoDe(const Sprite& s, const SDL_FRect& recorte) {
    if (s.tamanho.x > 0.0f && s.tamanho.y > 0.0f) {
        return s.tamanho;
    }
    return SDL_FPoint{recorte.w, recorte.h};
}

void aplicarTinta(SDL_Texture* textura, SDL_Color cor) {
    SDL_SetTextureColorMod(textura, cor.r, cor.g, cor.b);
    SDL_SetTextureAlphaMod(textura, cor.a);
    SDL_SetTextureBlendMode(textura, SDL_BLENDMODE_BLEND);
}

void desenhar(SDL_Renderer* renderer, const Sprite& s, SDL_FPoint canto, SDL_FPoint tamanho,
              SDL_FPoint ancoraEmPixels) {
    const SDL_FRect recorte = recorteDe(s);
    const SDL_FRect destino{canto.x, canto.y, tamanho.x, tamanho.y};

    aplicarTinta(s.textura, s.tinta);
    if (s.rotacao == 0.0 && s.espelho == SDL_FLIP_NONE) {
        SDL_RenderTexture(renderer, s.textura, &recorte, &destino);
    } else {
        SDL_RenderTextureRotated(renderer, s.textura, &recorte, &destino, s.rotacao,
                                 &ancoraEmPixels, s.espelho);
    }
    aplicarTinta(s.textura, SDL_Color{255, 255, 255, 255});
}

}  // namespace

void sprite(SDL_Renderer* renderer, const Camera& camera, const Sprite& s,
            SDL_FPoint posicaoMundo) {
    if (s.textura == nullptr) {
        return;
    }
    const SDL_FRect recorte = recorteDe(s);
    const SDL_FPoint tamanhoMundo = tamanhoDe(s, recorte);
    const SDL_FPoint tamanhoTela{tamanhoMundo.x * camera.zoom(), tamanhoMundo.y * camera.zoom()};

    const SDL_FPoint centro = camera.mundoParaTela(posicaoMundo);
    const SDL_FPoint canto{centro.x - tamanhoTela.x * s.ancora.x,
                           centro.y - tamanhoTela.y * s.ancora.y};
    const SDL_FPoint ancoraPixels{tamanhoTela.x * s.ancora.x, tamanhoTela.y * s.ancora.y};

    desenhar(renderer, s, canto, tamanhoTela, ancoraPixels);
}

void spriteTela(SDL_Renderer* renderer, const Sprite& s, SDL_FPoint posicaoTela, float escala) {
    if (s.textura == nullptr) {
        return;
    }
    const SDL_FRect recorte = recorteDe(s);
    const SDL_FPoint base = tamanhoDe(s, recorte);
    const SDL_FPoint tamanho{base.x * escala, base.y * escala};
    const SDL_FPoint canto{posicaoTela.x - tamanho.x * s.ancora.x,
                           posicaoTela.y - tamanho.y * s.ancora.y};
    const SDL_FPoint ancoraPixels{tamanho.x * s.ancora.x, tamanho.y * s.ancora.y};

    desenhar(renderer, s, canto, tamanho, ancoraPixels);
}

void retanguloMundo(SDL_Renderer* renderer, const Camera& camera, const SDL_FRect& retangulo,
                    SDL_Color cor, bool preenchido) {
    const SDL_FPoint canto = camera.mundoParaTela(SDL_FPoint{retangulo.x, retangulo.y});
    const SDL_FRect emTela{canto.x, canto.y, retangulo.w * camera.zoom(),
                           retangulo.h * camera.zoom()};
    retanguloTela(renderer, emTela, cor, preenchido);
}

void retanguloTela(SDL_Renderer* renderer, const SDL_FRect& retangulo, SDL_Color cor,
                   bool preenchido) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, cor.r, cor.g, cor.b, cor.a);
    if (preenchido) {
        SDL_RenderFillRect(renderer, &retangulo);
    } else {
        SDL_RenderRect(renderer, &retangulo);
    }
}

void linhaMundo(SDL_Renderer* renderer, const Camera& camera, SDL_FPoint a, SDL_FPoint b,
                SDL_Color cor) {
    const SDL_FPoint ta = camera.mundoParaTela(a);
    const SDL_FPoint tb = camera.mundoParaTela(b);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, cor.r, cor.g, cor.b, cor.a);
    SDL_RenderLine(renderer, ta.x, ta.y, tb.x, tb.y);
}

void brilhoAditivo(SDL_Renderer* renderer, SDL_FPoint centro, float raio, SDL_FColor cor) {
    constexpr int kSegmentos = 12;
    if (raio <= 0.5f) {
        return;
    }

    std::vector<SDL_Vertex> leque;
    leque.reserve(kSegmentos * 3);

    const SDL_FColor transparente{cor.r, cor.g, cor.b, 0.0f};
    for (int i = 0; i < kSegmentos; ++i) {
        const float a0 = 6.2831853f * static_cast<float>(i) / kSegmentos;
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / kSegmentos;

        const SDL_Vertex meio{centro, cor, {0.0f, 0.0f}};
        const SDL_Vertex borda0{
            SDL_FPoint{centro.x + std::cos(a0) * raio, centro.y + std::sin(a0) * raio},
            transparente,
            {0.0f, 0.0f}};
        const SDL_Vertex borda1{
            SDL_FPoint{centro.x + std::cos(a1) * raio, centro.y + std::sin(a1) * raio},
            transparente,
            {0.0f, 0.0f}};
        leque.insert(leque.end(), {meio, borda0, borda1});
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    SDL_RenderGeometry(renderer, nullptr, leque.data(), static_cast<int>(leque.size()), nullptr, 0);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

}  // namespace jogo::draw

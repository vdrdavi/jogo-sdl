#include "gfx/Camera.hpp"

#include <algorithm>
#include <cmath>

namespace jogo {

void Camera::definirZoom(float z) {
    zoom_ = std::clamp(z, 0.1f, 16.0f);
}

SDL_FPoint Camera::mundoParaTela(SDL_FPoint mundo) const {
    return SDL_FPoint{
        (mundo.x - posicao_.x) * zoom_ + viewport_.x * 0.5f,
        (mundo.y - posicao_.y) * zoom_ + viewport_.y * 0.5f,
    };
}

SDL_FPoint Camera::telaParaMundo(SDL_FPoint tela) const {
    return SDL_FPoint{
        (tela.x - viewport_.x * 0.5f) / zoom_ + posicao_.x,
        (tela.y - viewport_.y * 0.5f) / zoom_ + posicao_.y,
    };
}

SDL_FRect Camera::areaVisivel() const {
    const float largura = viewport_.x / zoom_;
    const float altura = viewport_.y / zoom_;
    return SDL_FRect{posicao_.x - largura * 0.5f, posicao_.y - altura * 0.5f, largura, altura};
}

void Camera::seguir(SDL_FPoint alvo, float dt, float suavidade) {
    // 1 - exp(-k*dt) da uma resposta independente do dt, ao contrario de um lerp cru.
    const float fator = 1.0f - std::exp(-suavidade * dt);
    posicao_.x += (alvo.x - posicao_.x) * fator;
    posicao_.y += (alvo.y - posicao_.y) * fator;
}

void Camera::limitarA(const SDL_FRect& limitesDoMundo) {
    const SDL_FRect visivel = areaVisivel();

    if (visivel.w >= limitesDoMundo.w) {
        posicao_.x = limitesDoMundo.x + limitesDoMundo.w * 0.5f;
    } else {
        const float min = limitesDoMundo.x + visivel.w * 0.5f;
        const float max = limitesDoMundo.x + limitesDoMundo.w - visivel.w * 0.5f;
        posicao_.x = std::clamp(posicao_.x, min, max);
    }

    if (visivel.h >= limitesDoMundo.h) {
        posicao_.y = limitesDoMundo.y + limitesDoMundo.h * 0.5f;
    } else {
        const float min = limitesDoMundo.y + visivel.h * 0.5f;
        const float max = limitesDoMundo.y + limitesDoMundo.h - visivel.h * 0.5f;
        posicao_.y = std::clamp(posicao_.y, min, max);
    }
}

}  // namespace jogo

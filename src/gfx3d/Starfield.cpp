#include "gfx3d/Starfield.hpp"

#include <algorithm>
#include <cmath>

#include "core/Aleatorio.hpp"

namespace jogo {
namespace {

/// Rastro em segundos: o quanto do deslocamento vira risco na tela.
constexpr float kTempoRastro = 0.055f;
constexpr float kTamanhoBase = 0.26f;

/// Cores por "temperatura": a maioria branca/azulada, poucas alaranjadas.
SDL_FColor corEstelar(Aleatorio& rng) {
    const float sorteio = rng.unitario();
    if (sorteio < 0.55f) {
        return SDL_FColor{0.88f, 0.92f, 1.00f, 1.0f};  // branco azulado
    }
    if (sorteio < 0.78f) {
        return SDL_FColor{0.65f, 0.78f, 1.00f, 1.0f};  // azul
    }
    if (sorteio < 0.93f) {
        return SDL_FColor{1.00f, 0.94f, 0.75f, 1.0f};  // amarelo
    }
    return SDL_FColor{1.00f, 0.66f, 0.50f, 1.0f};  // laranja
}

/// Mantem uma coordenada relativa dentro de [-raio, raio).
float envolver(float distancia, float raio) {
    const float largura = raio * 2.0f;
    float valor = std::fmod(distancia + raio, largura);
    if (valor < 0.0f) {
        valor += largura;
    }
    return valor - raio;
}

}  // namespace

void Starfield::gerar(Uint32 semente, int quantidade, float raio) {
    raio_ = raio;
    Aleatorio rng(semente);

    estrelas_.clear();
    estrelas_.reserve(static_cast<std::size_t>(quantidade));
    for (int i = 0; i < quantidade; ++i) {
        Estrela estrela;
        estrela.posicao = Vec3{rng.entre(-raio, raio), rng.entre(-raio, raio),
                               rng.entre(-raio, raio)};
        estrela.cor = corEstelar(rng);
        estrela.brilho = rng.entre(0.35f, 1.0f);
        estrela.fase = rng.entre(0.0f, 6.2831853f);
        estrelas_.push_back(estrela);
    }
}

void Starfield::centralizar(Vec3 posicao) {
    for (Estrela& estrela : estrelas_) {
        estrela.posicao = posicao + Vec3{envolver(estrela.posicao.x - posicao.x, raio_),
                                         envolver(estrela.posicao.y - posicao.y, raio_),
                                         envolver(estrela.posicao.z - posicao.z, raio_)};
    }
}

void Starfield::desenhar(SDL_Renderer* renderer, const Renderer3D& cena, Vec3 velocidade,
                         float tempo) {
    buffer_.clear();
    const Vec3 deslocamento = velocidade * kTempoRastro;

    for (const Estrela& estrela : estrelas_) {
        SDL_FPoint cabeca;
        float profundidade = 0.0f;
        if (!cena.projetar(estrela.posicao, cabeca, &profundidade)) {
            continue;
        }

        // Some suavemente na borda do cubo, para nenhuma estrela "aparecer" ao
        // ser reposicionada pelo wrap.
        // Queda suave: as estrelas proximas ficam nitidas por mais tempo e so
        // somem perto da borda do cubo, onde o wrap as reposiciona.
        const float distanciaNormalizada = std::clamp(profundidade / raio_, 0.0f, 1.0f);
        float intensidade =
            estrela.brilho * (1.0f - distanciaNormalizada * distanciaNormalizada * distanciaNormalizada);
        if (intensidade <= 0.02f) {
            continue;
        }
        intensidade *= 0.82f + 0.18f * std::sin(tempo * 2.5f + estrela.fase);
        intensidade = std::clamp(intensidade, 0.0f, 1.0f);

        const float tamanho =
            std::clamp(cena.escalaEmTela(profundidade) * kTamanhoBase, 1.0f, 3.4f);

        // Cauda: onde a estrela aparecia um instante atras. Como a camera
        // avancou v*dt, isso equivale a projetar a estrela deslocada de +v*dt.
        SDL_FPoint cauda = cabeca;
        const bool temRastro = cena.projetar(estrela.posicao + deslocamento, cauda, nullptr);

        float dx = temRastro ? cauda.x - cabeca.x : 0.0f;
        float dy = temRastro ? cauda.y - cabeca.y : 0.0f;
        const float distancia = std::sqrt(dx * dx + dy * dy);
        if (distancia < 0.5f) {
            // Parada (ou quase): desenha um quadrado no lugar do risco.
            dx = 0.0f;
            dy = 0.0f;
            cauda = cabeca;
        }

        // Perpendicular normalizada, com largura minima de um pixel.
        float px = -dy;
        float py = dx;
        const float tamanhoPerp = std::sqrt(px * px + py * py);
        if (tamanhoPerp > 1e-4f) {
            px = px / tamanhoPerp * tamanho * 0.5f;
            py = py / tamanhoPerp * tamanho * 0.5f;
        } else {
            px = tamanho * 0.5f;
            py = 0.0f;
        }

        const SDL_FColor corCabeca{estrela.cor.r * intensidade, estrela.cor.g * intensidade,
                                   estrela.cor.b * intensidade, 1.0f};
        const SDL_FColor corCauda{corCabeca.r, corCabeca.g, corCabeca.b, 0.0f};

        const SDL_Vertex a{SDL_FPoint{cabeca.x + px, cabeca.y + py}, corCabeca, {0.0f, 0.0f}};
        const SDL_Vertex b{SDL_FPoint{cabeca.x - px, cabeca.y - py}, corCabeca, {0.0f, 0.0f}};
        const SDL_Vertex c{SDL_FPoint{cauda.x - px, cauda.y - py}, corCauda, {0.0f, 0.0f}};
        const SDL_Vertex d{SDL_FPoint{cauda.x + px, cauda.y + py}, corCauda, {0.0f, 0.0f}};

        buffer_.insert(buffer_.end(), {a, b, c, a, c, d});
    }

    if (buffer_.empty()) {
        return;
    }

    // Aditivo: estrelas somam luz ao vazio, sem escurecer o que esta atras.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
    SDL_RenderGeometry(renderer, nullptr, buffer_.data(), static_cast<int>(buffer_.size()),
                       nullptr, 0);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

}  // namespace jogo

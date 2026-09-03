#include "gfx3d/Destrocos.hpp"

#include <algorithm>
#include <cmath>

#include "gfx/Draw.hpp"

namespace jogo {
namespace {

// Quanto os cacos se abrem. A explosao e lida pela separacao entre eles, nao
// pela velocidade absoluta: rapido demais e a nave some antes de o jogador ver
// que era a nave.
constexpr float kArremessoMinimo = 3.5f;  // u/s
constexpr float kArremessoMaximo = 9.0f;
constexpr float kGiroMaximo = 4.5f;  // rad/s

// Faiscas: rapidas e curtas, so o clarao inicial do rompimento.
constexpr int kFaiscas = 44;
constexpr float kFaiscaMinima = 9.0f;  // u/s
constexpr float kFaiscaMaxima = 26.0f;
constexpr float kVidaMinima = 0.35f;  // s
constexpr float kVidaMaxima = 1.30f;
constexpr float kRaioFaisca = 0.16f;  // tamanho aparente, em unidades de mundo

/// O lado de dentro do casco nunca tinha sido visto: e a mesma cor da face,
/// escurecida, para o caco ter frente e avesso em vez de parecer papel.
SDL_FColor avesso(SDL_FColor cor) {
    return SDL_FColor{cor.r * 0.38f, cor.g * 0.38f, cor.b * 0.40f, cor.a};
}

}  // namespace

void Destrocos::gerar(const Mesh& malha, Vec3 posicao, const Mat3& rotacao, float escala,
                      Vec3 velocidade, Uint32 semente) {
    cacos_.clear();
    faiscas_.clear();
    rng_ = Aleatorio{semente};

    if (malha.vertices.empty() || malha.faces.empty()) {
        return;
    }

    Vec3 centro{};
    for (const Vec3& v : malha.vertices) {
        centro += v;
    }
    centro = centro * (1.0f / static_cast<float>(malha.vertices.size()));

    cacos_.reserve(malha.faces.size());
    for (const Mesh::Face& face : malha.faces) {
        const Vec3 a = malha.vertices[static_cast<std::size_t>(face.a)];
        const Vec3 b = malha.vertices[static_cast<std::size_t>(face.b)];
        const Vec3 c = malha.vertices[static_cast<std::size_t>(face.c)];

        // O caco gira em torno de si mesmo, entao ele nasce centrado no proprio
        // centroide e o que sobra vira a posicao dele no mundo.
        const Vec3 centroide = (a + b + c + centro) * 0.25f;

        Caco caco;
        caco.malha.vertices = {(a - centroide) * escala, (b - centroide) * escala,
                               (c - centroide) * escala, (centro - centroide) * escala};
        caco.malha.faces = {
            {0, 1, 2, face.cor},  // a pele da nave
            {0, 1, 3, avesso(face.cor)},
            {1, 2, 3, avesso(face.cor)},
            {0, 2, 3, avesso(face.cor)},
        };
        orientarFacesParaFora(caco.malha);

        caco.base = rotacao;
        caco.posicao = posicao + rotacao * (centroide * escala);
        // Para fora do centro da nave: e a direcao em que a peca ja estava, e
        // por isso a nave parece se abrir em vez de espirrar em outra direcao.
        const Vec3 paraFora = rotacao * normalizar(centroide - centro);
        caco.velocidade = velocidade + paraFora * rng_.entre(kArremessoMinimo, kArremessoMaximo);
        caco.giroYaw = rng_.entre(-kGiroMaximo, kGiroMaximo);
        caco.giroPitch = rng_.entre(-kGiroMaximo, kGiroMaximo);
        cacos_.push_back(std::move(caco));
    }

    faiscas_.reserve(kFaiscas);
    for (int i = 0; i < kFaiscas; ++i) {
        // Direcao isotropica pelo metodo do cubo rejeitado: sortear angulos
        // acumularia faisca nos polos.
        Vec3 direcao{};
        float tamanho = 0.0f;
        do {
            direcao = Vec3{rng_.entre(-1.0f, 1.0f), rng_.entre(-1.0f, 1.0f),
                           rng_.entre(-1.0f, 1.0f)};
            tamanho = comprimento(direcao);
        } while (tamanho < 0.2f || tamanho > 1.0f);
        direcao = direcao * (1.0f / tamanho);

        Faisca faisca;
        faisca.posicao = posicao + direcao * rng_.entre(0.0f, 1.2f);
        faisca.velocidade = velocidade + direcao * rng_.entre(kFaiscaMinima, kFaiscaMaxima);
        faisca.duracao = rng_.entre(kVidaMinima, kVidaMaxima);
        faisca.vida = faisca.duracao;
        // Do branco ao laranja: a mesma familia de cor do escapamento.
        const float quente = rng_.unitario();
        faisca.cor = SDL_FColor{1.0f, 0.45f + 0.45f * quente, 0.15f + 0.55f * quente * quente,
                                1.0f};
        faiscas_.push_back(faisca);
    }
}

void Destrocos::atualizar(float dt) {
    for (Caco& caco : cacos_) {
        caco.posicao += caco.velocidade * dt;
        caco.yaw += caco.giroYaw * dt;
        caco.pitch += caco.giroPitch * dt;
    }

    for (Faisca& faisca : faiscas_) {
        faisca.posicao += faisca.velocidade * dt;
        faisca.vida -= dt;
    }
}

void Destrocos::submeter(Renderer3D& cena) const {
    for (const Caco& caco : cacos_) {
        // O tombo entra depois da orientacao de origem, e e recomposto do
        // angulo a cada quadro em vez de multiplicado passo a passo: assim nao
        // acumula erro (o mesmo motivo do campo de asteroides).
        cena.submeter(caco.malha, caco.posicao, caco.base * Mat3::deEuler(caco.yaw, caco.pitch, 0.0f));
    }
}

void Destrocos::desenharFaiscas(SDL_Renderer* renderer, const Renderer3D& cena) const {
    for (const Faisca& faisca : faiscas_) {
        if (faisca.vida <= 0.0f) {
            continue;
        }
        SDL_FPoint tela;
        float profundidade = 0.0f;
        if (!cena.projetar(faisca.posicao, tela, &profundidade)) {
            continue;
        }
        // Apaga pelo quadrado do que resta de vida: o fim do brilho e o que se
        // percebe, e o linear parecia desligar de uma vez.
        const float restante = faisca.vida / faisca.duracao;
        const float brilho = restante * restante;
        draw::brilhoAditivo(renderer, tela, cena.escalaEmTela(profundidade) * kRaioFaisca,
                            SDL_FColor{faisca.cor.r * brilho, faisca.cor.g * brilho,
                                       faisca.cor.b * brilho, 1.0f});
    }
}

}  // namespace jogo

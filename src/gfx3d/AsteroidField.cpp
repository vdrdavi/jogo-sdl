#include "gfx3d/AsteroidField.hpp"

#include <cmath>

namespace jogo {
namespace {

/// Variedades de rocha sorteadas na geracao: poucas malhas, muitas pedras.
constexpr int kVariedades = 5;

/// Distancia minima entre a nave e uma rocha recem-colocada: sem isso o campo
/// poderia nascer (ou ressurgir) com uma pedra dentro da cabine.
constexpr float kDistanciaSegura = 55.0f;

constexpr float kRaioMinimo = 2.2f;
constexpr float kRaioMaximo = 7.5f;

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

Vec3 AsteroidField::sortear(Vec3 centro, float minimo) {
    // O cubo e grande perto do limite seguro, entao sortear de novo converge
    // rapido; o teto de tentativas so existe para nao depender de sorte.
    for (int tentativa = 0; tentativa < 16; ++tentativa) {
        const Vec3 deslocamento{rng_.entre(-raio_, raio_), rng_.entre(-raio_, raio_),
                                rng_.entre(-raio_, raio_)};
        if (comprimento(deslocamento) >= minimo) {
            return centro + deslocamento;
        }
    }
    return centro + Vec3{0.0f, 0.0f, -raio_};
}

void AsteroidField::gerar(Uint32 semente, int quantidade, float raio) {
    raio_ = raio;
    rng_ = Aleatorio(semente);

    malhas_.clear();
    malhas_.reserve(kVariedades);
    for (int i = 0; i < kVariedades; ++i) {
        malhas_.push_back(criarAsteroideLowPoly(rng_.proximo()));
    }

    asteroides_.clear();
    asteroides_.reserve(static_cast<std::size_t>(quantidade));
    for (int i = 0; i < quantidade; ++i) {
        Asteroide rocha;
        rocha.posicao = sortear(Vec3{}, kDistanciaSegura);
        rocha.raio = rng_.entre(kRaioMinimo, kRaioMaximo);
        rocha.yaw = rng_.entre(0.0f, 6.2831853f);
        rocha.pitch = rng_.entre(0.0f, 6.2831853f);
        rocha.giroYaw = rng_.entre(-0.5f, 0.5f);
        rocha.giroPitch = rng_.entre(-0.5f, 0.5f);
        rocha.malha = static_cast<std::size_t>(rng_.proximo() % kVariedades);
        asteroides_.push_back(rocha);
    }
}

void AsteroidField::atualizar(float dt) {
    for (Asteroide& rocha : asteroides_) {
        rocha.yaw += rocha.giroYaw * dt;
        rocha.pitch += rocha.giroPitch * dt;
    }
}

void AsteroidField::centralizar(Vec3 posicao) {
    for (Asteroide& rocha : asteroides_) {
        const Vec3 relativo = rocha.posicao - posicao;
        Vec3 envolvido{envolver(relativo.x, raio_), envolver(relativo.y, raio_),
                       envolver(relativo.z, raio_)};

        // Wrap puro deixaria o campo periodico: voando reto, as mesmas rochas
        // voltariam na mesma formacao a cada travessia do cubo (com o Starfield
        // isso passa batido, mas rocha tem forma e a repeticao aparece). Quem
        // atravessa a borda volta sorteada nos eixos que nao viraram -- e
        // sempre a uma aresta inteira de distancia, dentro da nevoa, entao a
        // troca acontece longe dos olhos.
        //
        // O teste e por magnitude: quem virou andou uma aresta inteira, e o
        // resto e ruido de arredondamento (envolver() soma e subtrai `raio`,
        // entao nem sempre devolve o mesmo float que entrou). Por igualdade,
        // quase toda rocha seria sorteada de novo a cada quadro.
        const bool virouX = std::fabs(envolvido.x - relativo.x) > raio_;
        const bool virouY = std::fabs(envolvido.y - relativo.y) > raio_;
        const bool virouZ = std::fabs(envolvido.z - relativo.z) > raio_;
        if (virouX || virouY || virouZ) {
            if (!virouX) {
                envolvido.x = rng_.entre(-raio_, raio_);
            }
            if (!virouY) {
                envolvido.y = rng_.entre(-raio_, raio_);
            }
            if (!virouZ) {
                envolvido.z = rng_.entre(-raio_, raio_);
            }
            rocha.raio = rng_.entre(kRaioMinimo, kRaioMaximo);
            rocha.giroYaw = rng_.entre(-0.5f, 0.5f);
            rocha.giroPitch = rng_.entre(-0.5f, 0.5f);
            rocha.malha = static_cast<std::size_t>(rng_.proximo() % kVariedades);
        }

        rocha.posicao = posicao + envolvido;
    }
}

void AsteroidField::submeter(Renderer3D& cena) const {
    const Vec3 olho = cena.camera().posicao;
    for (const Asteroide& rocha : asteroides_) {
        // O cubo tem canto: uma rocha no vertice esta a raio*sqrt(3) e so
        // gastaria faces que a nevoa ja apagou.
        if (comprimento(rocha.posicao - olho) - rocha.raio > raio_) {
            continue;
        }
        cena.submeter(malhas_[rocha.malha], rocha.posicao,
                      Mat3::deEuler(rocha.yaw, rocha.pitch, 0.0f), rocha.raio);
    }
}

int AsteroidField::colisao(Vec3 posicao, float raio) const {
    for (std::size_t i = 0; i < asteroides_.size(); ++i) {
        const Asteroide& rocha = asteroides_[i];
        const float alcance = rocha.raio + raio;
        const Vec3 delta = rocha.posicao - posicao;
        if (dot(delta, delta) < alcance * alcance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void AsteroidField::reposicionar(int indice, Vec3 referencia) {
    if (indice < 0 || indice >= quantidade()) {
        return;
    }
    Asteroide& rocha = asteroides_[static_cast<std::size_t>(indice)];
    rocha.posicao = sortear(referencia, kDistanciaSegura);
    rocha.raio = rng_.entre(kRaioMinimo, kRaioMaximo);
    rocha.malha = static_cast<std::size_t>(rng_.proximo() % kVariedades);
}

}  // namespace jogo

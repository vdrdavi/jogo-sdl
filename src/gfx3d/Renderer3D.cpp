#include "gfx3d/Renderer3D.hpp"

#include <algorithm>
#include <array>

namespace jogo {
namespace {

/// Luz direcional fixa do "sistema estelar", em espaco de mundo.
constexpr Vec3 kDirecaoLuz{-0.45f, -0.75f, -0.5f};
constexpr float kAmbiente = 0.30f;

SDL_FColor aplicarLuz(SDL_FColor cor, float intensidade) {
    return SDL_FColor{cor.r * intensidade, cor.g * intensidade, cor.b * intensidade, cor.a};
}

SDL_FColor misturar(SDL_FColor cor, SDL_FColor outra, float t) {
    return SDL_FColor{cor.r + (outra.r - cor.r) * t, cor.g + (outra.g - cor.g) * t,
                      cor.b + (outra.b - cor.b) * t, cor.a};
}

/// Recorta um triangulo (ja em espaco de vista) contra o plano proximo.
/// Devolve o numero de vertices do poligono resultante (0, 3 ou 4).
int recortarNoPlanoProximo(const std::array<Vec3, 3>& entrada, std::array<Vec3, 4>& saida) {
    constexpr float kLimite = -Renderer3D::kPlanoProximo;  // dentro: z <= kLimite
    int total = 0;

    for (int i = 0; i < 3; ++i) {
        const Vec3 atual = entrada[static_cast<std::size_t>(i)];
        const Vec3 proximo = entrada[static_cast<std::size_t>((i + 1) % 3)];
        const bool atualDentro = atual.z <= kLimite;
        const bool proximoDentro = proximo.z <= kLimite;

        if (atualDentro) {
            saida[static_cast<std::size_t>(total++)] = atual;
        }
        if (atualDentro != proximoDentro) {
            const float t = (kLimite - atual.z) / (proximo.z - atual.z);
            saida[static_cast<std::size_t>(total++)] = lerp(atual, proximo, t);
        }
    }
    return total;
}

}  // namespace

void Renderer3D::redimensionar(float largura, float altura) {
    largura_ = largura;
    altura_ = altura;
    atualizarDistanciaFocal();
}

void Renderer3D::definirNevoa(SDL_FColor cor, float inicio, float fim) {
    nevoaCor_ = cor;
    nevoaInicio_ = inicio;
    nevoaFim_ = fim;
}

void Renderer3D::definirCamera(const Camera3D& camera) {
    camera_ = camera;
    // O fov pode mudar a cada quadro (aceleracao), e a projecao das estrelas
    // acontece antes de qualquer submeter().
    atualizarDistanciaFocal();
}

void Renderer3D::atualizarDistanciaFocal() {
    const float fovRad = camera_.fovGraus * 3.14159265f / 180.0f;
    distanciaFocal_ = (altura_ * 0.5f) / std::tan(fovRad * 0.5f);
}

bool Renderer3D::projetarVista(Vec3 vista, SDL_FPoint& tela) const {
    if (vista.z > -kPlanoProximo) {
        return false;
    }
    const float invZ = distanciaFocal_ / -vista.z;
    tela.x = largura_ * 0.5f + vista.x * invZ;
    tela.y = altura_ * 0.5f - vista.y * invZ;
    return true;
}

bool Renderer3D::projetar(Vec3 mundo, SDL_FPoint& tela, float* profundidade) const {
    const Vec3 vista = paraVista(mundo);
    if (profundidade != nullptr) {
        *profundidade = -vista.z;
    }
    return projetarVista(vista, tela);
}

void Renderer3D::submeter(const Mesh& malha, Vec3 posicao, const Mat3& rotacao, float escala) {
    const Vec3 luz = normalizar(kDirecaoLuz);

    for (const Mesh::Face& face : malha.faces) {
        const Vec3 a = posicao + rotacao * (malha.vertices[static_cast<std::size_t>(face.a)] * escala);
        const Vec3 b = posicao + rotacao * (malha.vertices[static_cast<std::size_t>(face.b)] * escala);
        const Vec3 c = posicao + rotacao * (malha.vertices[static_cast<std::size_t>(face.c)] * escala);

        const Vec3 normal = normalizar(cross(b - a, c - a));

        // Face traseira: a normal aponta no mesmo sentido do olhar.
        if (dot(normal, a - camera_.posicao) >= 0.0f) {
            continue;
        }

        const float difusa = std::max(0.0f, dot(normal, -luz));
        SDL_FColor cor = aplicarLuz(face.cor, kAmbiente + (1.0f - kAmbiente) * difusa);

        const std::array<Vec3, 3> vista{paraVista(a), paraVista(b), paraVista(c)};

        if (nevoaFim_ > nevoaInicio_) {
            const float profundidade = -(vista[0].z + vista[1].z + vista[2].z) / 3.0f;
            const float t = std::clamp((profundidade - nevoaInicio_) / (nevoaFim_ - nevoaInicio_),
                                       0.0f, 1.0f);
            if (t >= 1.0f) {
                // Ja e a cor do fundo: desenhar nao mudaria um pixel.
                continue;
            }
            cor = misturar(cor, nevoaCor_, t);
        }

        std::array<Vec3, 4> recortado{};
        const int total = recortarNoPlanoProximo(vista, recortado);
        if (total < 3) {
            continue;
        }

        // Poligono convexo -> leque de triangulos a partir do primeiro vertice.
        for (int i = 1; i + 1 < total; ++i) {
            const std::array<Vec3, 3> tri{recortado[0], recortado[static_cast<std::size_t>(i)],
                                          recortado[static_cast<std::size_t>(i + 1)]};

            FaceProjetada projetada;
            bool visivel = true;
            for (int k = 0; k < 3; ++k) {
                SDL_FPoint ponto;
                if (!projetarVista(tri[static_cast<std::size_t>(k)], ponto)) {
                    visivel = false;
                    break;
                }
                projetada.v[k].position = ponto;
                projetada.v[k].color = cor;
                projetada.v[k].tex_coord = SDL_FPoint{0.0f, 0.0f};
            }
            if (!visivel) {
                continue;
            }
            projetada.profundidade = -(tri[0].z + tri[1].z + tri[2].z) / 3.0f;
            faces_.push_back(projetada);
        }
    }
}

void Renderer3D::desenhar(SDL_Renderer* renderer) {
    if (faces_.empty()) {
        return;
    }

    // Algoritmo do pintor: sem z-buffer, o que esta longe vai primeiro.
    std::sort(faces_.begin(), faces_.end(),
              [](const FaceProjetada& a, const FaceProjetada& b) {
                  return a.profundidade > b.profundidade;
              });

    saida_.clear();
    saida_.reserve(faces_.size() * 3);
    for (const FaceProjetada& face : faces_) {
        saida_.push_back(face.v[0]);
        saida_.push_back(face.v[1]);
        saida_.push_back(face.v[2]);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer, nullptr, saida_.data(), static_cast<int>(saida_.size()), nullptr,
                       0);
    faces_.clear();
}

}  // namespace jogo

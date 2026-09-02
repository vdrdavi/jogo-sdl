#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "gfx3d/Math3D.hpp"
#include "gfx3d/Mesh.hpp"

namespace jogo {

struct Camera3D {
    Vec3 posicao{0.0f, 0.0f, 0.0f};
    /// Colunas: direita, cima e "para tras". A frente e -colunas[2].
    Mat3 orientacao{};
    float fovGraus{65.0f};
};

/// Rasterizador 3D minimo sobre SDL_RenderGeometry: projecao em perspectiva,
/// recorte no plano proximo, descarte de faces traseiras, sombreamento flat e
/// ordenacao por profundidade (algoritmo do pintor). Sem shaders e sem
/// dependencias alem do proprio SDL.
class Renderer3D {
public:
    Renderer3D(float largura, float altura) { redimensionar(largura, altura); }

    void redimensionar(float largura, float altura);
    void definirCamera(const Camera3D& camera);
    const Camera3D& camera() const { return camera_; }

    /// Faz as faces se dissolverem na cor do fundo entre `inicio` e `fim`
    /// (distancias em Z). Sem isso um objeto de um campo com wrap aparece
    /// inteiro na borda do campo; com isso ele emerge do vazio. `fim <= inicio`
    /// desliga a nevoa.
    void definirNevoa(SDL_FColor cor, float inicio, float fim);

    /// Comeca um novo quadro (descarta o lote anterior).
    void iniciarQuadro() { faces_.clear(); }

    void submeter(const Mesh& malha, Vec3 posicao, const Mat3& rotacao, float escala = 1.0f);

    /// Ordena o lote de tras para frente e emite tudo em uma chamada.
    void desenhar(SDL_Renderer* renderer);

    /// Coordenadas no espaco da camera (-Z para a frente).
    Vec3 paraVista(Vec3 mundo) const { return camera_.orientacao.aplicarTransposta(mundo - camera_.posicao); }

    /// Projeta um ponto ja em espaco de vista. Devolve false se estiver atras
    /// do plano proximo.
    bool projetarVista(Vec3 vista, SDL_FPoint& tela) const;

    /// Projeta um ponto do mundo; `profundidade` recebe a distancia em Z.
    bool projetar(Vec3 mundo, SDL_FPoint& tela, float* profundidade = nullptr) const;

    /// Escala em pixels de um objeto de tamanho 1 a uma dada profundidade.
    float escalaEmTela(float profundidade) const { return distanciaFocal_ / profundidade; }

    float largura() const { return largura_; }
    float altura() const { return altura_; }

    static constexpr float kPlanoProximo = 0.15f;

private:
    void atualizarDistanciaFocal();

    struct FaceProjetada {
        SDL_Vertex v[3];
        float profundidade{0.0f};
    };

    Camera3D camera_;
    float largura_{640.0f};
    float altura_{360.0f};
    float distanciaFocal_{300.0f};

    SDL_FColor nevoaCor_{0.0f, 0.0f, 0.0f, 1.0f};
    float nevoaInicio_{0.0f};
    float nevoaFim_{0.0f};

    std::vector<FaceProjetada> faces_;
    std::vector<SDL_Vertex> saida_;
};

}  // namespace jogo

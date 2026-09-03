#pragma once

#include <SDL3/SDL.h>

namespace jogo {

/// Um clipe de animacao: qual linha do atlas, quantos quadros ela tem e a que
/// ritmo eles passam. E so descricao -- o estado (que quadro esta no ar) fica na
/// Animacao, para que o mesmo clipe possa ser tocado por varios personagens.
struct Clipe {
    /// Linha do atlas. Tambem e a identidade do clipe: trocar de linha reinicia.
    int linha{0};
    /// Quantas colunas dessa linha o clipe usa, a partir da primeira.
    int quadros{1};
    /// Quadros por segundo. Zero (ou um unico quadro) deixa a animacao parada.
    float fps{8.0f};
};

/// Temporizador de quadros sobre um atlas em grade: a linha diz o clipe, a
/// coluna diz o quadro. Devolve o recorte que o Sprite ja sabia desenhar, entao
/// o Sprite continua sem saber o que e uma animacao -- quem anima e quem
/// desenha nao se conhecem.
class Animacao {
public:
    Animacao() = default;
    Animacao(float larguraCelula, float alturaCelula) : celula_{larguraCelula, alturaCelula} {}

    /// Tamanho da celula do atlas, em pixels da textura.
    void definirCelula(float largura, float altura) { celula_ = SDL_FPoint{largura, altura}; }

    /// Escolhe o clipe. So reinicia se a linha for outra: as cenas chamam isto a
    /// cada passo com o clipe do estado atual, e reiniciar sempre travaria o
    /// personagem no quadro 0 enquanto ele anda.
    void tocar(const Clipe& clipe);

    /// Anda o relogio. Chame do `atualizar` da cena, nunca do `desenhar`: com o
    /// passo fixo do App a animacao fica igual em qualquer taxa de quadros.
    void atualizar(float dt);

    void reiniciar();

    /// Recorte do quadro atual, para atribuir a Sprite::recorte.
    SDL_FRect recorte() const;

    int quadro() const { return quadro_; }

private:
    Clipe clipe_{};
    SDL_FPoint celula_{0.0f, 0.0f};
    float tempo_{0.0f};
    int quadro_{0};
};

}  // namespace jogo

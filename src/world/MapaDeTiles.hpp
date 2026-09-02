#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace jogo {

/// Grade de tiles de um ambiente, lida de um arquivo de texto em assets/maps/.
///
/// O cenario deixou de ser gerado em codigo justamente para poder ser editado
/// sem recompilar (e para caber mais de um ambiente no jogo): o arquivo traz a
/// legenda de caracteres, marcadores nomeados e a grade. Quais nomes de tile
/// existem, o indice de cada um no atlas e quais sao solidos continuam no
/// codigo, porque isso e propriedade da textura e da colisao, nao do cenario.
class MapaDeTiles {
public:
    /// Lado do tile em unidades de mundo. E o mesmo do atlas textures/interior.png.
    static constexpr int kTile = 16;

    /// Le o mapa a partir de um caminho relativo a assets/, ex.:
    /// carregar("maps/conves.mapa"). Em qualquer falha loga o motivo, monta uma
    /// sala de emergencia (piso cercado de parede) e devolve false: um arquivo
    /// faltando ou torto degrada o cenario, nao derruba o jogo.
    bool carregar(std::string_view relativo);

    int largura() const { return largura_; }
    int altura() const { return altura_; }

    /// Indice do tile no atlas. Fora da grade a busca e grampeada, entao a
    /// borda se repete em vez de abrir um vazio.
    Uint8 tile(int tx, int ty) const;

    /// Bloqueia o movimento? Fora da grade e sempre solido.
    bool solido(int tx, int ty) const;

    /// Retangulo do mapa em unidades de mundo.
    SDL_FRect limites() const;

    /// Marcador nomeado, em unidades de mundo. Devolve false se o arquivo nao
    /// trouxer esse nome.
    bool marcador(std::string_view nome, SDL_FPoint& saida) const;

private:
    void salaDeEmergencia();

    int largura_{0};
    int altura_{0};
    std::vector<Uint8> tiles_;
    std::unordered_map<std::string, SDL_FPoint> marcadores_;  // em tiles
};

}  // namespace jogo

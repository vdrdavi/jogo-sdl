#include "world/MapaDeTiles.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "core/Log.hpp"
#include "core/Paths.hpp"

namespace jogo {
namespace {

/// Os tiles que existem, na ordem do atlas textures/interior.png. O arquivo de
/// mapa fala por nome; o indice e a solidez ficam aqui porque pertencem a
/// textura e a colisao, e nao ao desenho do cenario.
struct DefinicaoDeTile {
    std::string_view nome;
    Uint8 indice;
    bool solido;
};

constexpr DefinicaoDeTile kDefinicoes[] = {
    {"piso", 0, false},  {"grade", 1, false}, {"piso-escuro", 2, false},
    {"parede", 3, true}, {"faixa", 4, true},  {"janela", 5, true},
};

const DefinicaoDeTile* definicaoPorNome(std::string_view nome) {
    for (const DefinicaoDeTile& definicao : kDefinicoes) {
        if (definicao.nome == nome) {
            return &definicao;
        }
    }
    return nullptr;
}

bool solidoPorIndice(Uint8 indice) {
    for (const DefinicaoDeTile& definicao : kDefinicoes) {
        if (definicao.indice == indice) {
            return definicao.solido;
        }
    }
    return true;  // indice desconhecido: melhor barrar do que deixar vazar.
}

/// Tira os brancos das pontas e devolve vazio se a linha for comentario. O
/// comentario e a linha inteira, nunca um rabicho no fim: '#' tambem e um
/// simbolo legitimo da legenda ("legenda # parede") e do tile parede na grade.
std::string limpar(const std::string& linha) {
    const auto branco = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    std::string texto = linha;
    while (!texto.empty() && branco(texto.back())) {
        texto.pop_back();
    }
    std::size_t inicio = 0;
    while (inicio < texto.size() && branco(texto[inicio])) {
        ++inicio;
    }
    texto = texto.substr(inicio);
    return texto.starts_with('#') ? std::string{} : texto;
}

/// Na grade so cai o fim de linha do Windows.
std::string semRetorno(const std::string& linha) {
    std::string texto = linha;
    while (!texto.empty() && (texto.back() == '\r' || texto.back() == '\n')) {
        texto.pop_back();
    }
    return texto;
}

}  // namespace

void MapaDeTiles::salaDeEmergencia() {
    const Uint8 piso = definicaoPorNome("piso")->indice;
    const Uint8 parede = definicaoPorNome("parede")->indice;

    largura_ = 20;
    altura_ = 13;
    tiles_.assign(static_cast<std::size_t>(largura_ * altura_), piso);
    for (int y = 0; y < altura_; ++y) {
        for (int x = 0; x < largura_; ++x) {
            if (x == 0 || y == 0 || x == largura_ - 1 || y == altura_ - 1) {
                tiles_[static_cast<std::size_t>(y * largura_ + x)] = parede;
            }
        }
    }
    marcadores_.clear();
}

bool MapaDeTiles::carregar(std::string_view relativo) {
    largura_ = 0;
    altura_ = 0;
    tiles_.clear();
    marcadores_.clear();

    const std::string arquivo = paths::asset(relativo);
    std::ifstream entrada(arquivo);
    if (!entrada) {
        JOGO_ERRO("Nao foi possivel abrir %s", arquivo.c_str());
        salaDeEmergencia();
        return false;
    }

    std::unordered_map<char, Uint8> legenda;
    std::vector<std::string> grade;
    bool naGrade = false;
    std::string linha;

    while (std::getline(entrada, linha)) {
        if (naGrade) {
            const std::string fileira = semRetorno(linha);
            if (!fileira.empty()) {
                grade.push_back(fileira);
            }
            continue;
        }

        const std::string limpa = limpar(linha);
        if (limpa.empty()) {
            continue;
        }

        std::istringstream fluxo(limpa);
        std::string chave;
        fluxo >> chave;

        if (chave == "mapa") {
            naGrade = true;
        } else if (chave == "legenda") {
            std::string simbolo;
            std::string nome;
            fluxo >> simbolo >> nome;
            const DefinicaoDeTile* definicao = definicaoPorNome(nome);
            if (simbolo.size() != 1 || definicao == nullptr) {
                JOGO_ERRO("%s: legenda invalida \"%s\"", arquivo.c_str(), limpa.c_str());
                salaDeEmergencia();
                return false;
            }
            legenda[simbolo[0]] = definicao->indice;
        } else if (chave == "marcador") {
            std::string nome;
            float x = 0.0f;
            float y = 0.0f;
            if (!(fluxo >> nome >> x >> y)) {
                JOGO_ERRO("%s: marcador invalido \"%s\"", arquivo.c_str(), limpa.c_str());
                salaDeEmergencia();
                return false;
            }
            marcadores_[nome] = SDL_FPoint{x, y};
        } else {
            JOGO_ERRO("%s: diretiva desconhecida \"%s\"", arquivo.c_str(), chave.c_str());
            salaDeEmergencia();
            return false;
        }
    }

    if (grade.empty()) {
        JOGO_ERRO("%s: nenhuma grade depois de \"mapa\"", arquivo.c_str());
        salaDeEmergencia();
        return false;
    }

    largura_ = static_cast<int>(grade.front().size());
    altura_ = static_cast<int>(grade.size());
    tiles_.reserve(static_cast<std::size_t>(largura_ * altura_));

    for (int y = 0; y < altura_; ++y) {
        const std::string& fileira = grade[static_cast<std::size_t>(y)];
        if (static_cast<int>(fileira.size()) != largura_) {
            JOGO_ERRO("%s: linha %d da grade tem %d colunas, esperava %d", arquivo.c_str(), y + 1,
                      static_cast<int>(fileira.size()), largura_);
            salaDeEmergencia();
            return false;
        }
        for (char simbolo : fileira) {
            const auto achado = legenda.find(simbolo);
            if (achado == legenda.end()) {
                JOGO_ERRO("%s: caractere '%c' fora da legenda", arquivo.c_str(), simbolo);
                salaDeEmergencia();
                return false;
            }
            tiles_.push_back(achado->second);
        }
    }
    return true;
}

Uint8 MapaDeTiles::tile(int tx, int ty) const {
    if (tiles_.empty()) {
        return definicaoPorNome("piso")->indice;
    }
    const int x = std::clamp(tx, 0, largura_ - 1);
    const int y = std::clamp(ty, 0, altura_ - 1);
    return tiles_[static_cast<std::size_t>(y * largura_ + x)];
}

bool MapaDeTiles::solido(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= largura_ || ty >= altura_) {
        return true;
    }
    return solidoPorIndice(tiles_[static_cast<std::size_t>(ty * largura_ + tx)]);
}

SDL_FRect MapaDeTiles::limites() const {
    return SDL_FRect{0.0f, 0.0f, static_cast<float>(largura_ * kTile),
                     static_cast<float>(altura_ * kTile)};
}

bool MapaDeTiles::marcador(std::string_view nome, SDL_FPoint& saida) const {
    const auto achado = marcadores_.find(std::string(nome));
    if (achado == marcadores_.end()) {
        return false;
    }
    saida = SDL_FPoint{achado->second.x * static_cast<float>(kTile),
                      achado->second.y * static_cast<float>(kTile)};
    return true;
}

}  // namespace jogo

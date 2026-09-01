#include "gfx/BitmapFont.hpp"

#include <fstream>
#include <sstream>

#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "gfx/Assets.hpp"

namespace jogo {
namespace {

/// Decodifica o proximo codepoint UTF-8 avancando `pos`. Bytes invalidos viram
/// '?', o que mantem o desenho estavel diante de texto malformado.
Uint32 proximoCodepoint(std::string_view texto, std::size_t& pos) {
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(texto[i]); };
    const unsigned char primeiro = byte(pos);

    std::size_t extras = 0;
    Uint32 codepoint = 0;
    if (primeiro < 0x80U) {
        ++pos;
        return primeiro;
    } else if ((primeiro & 0xE0U) == 0xC0U) {
        extras = 1;
        codepoint = primeiro & 0x1FU;
    } else if ((primeiro & 0xF0U) == 0xE0U) {
        extras = 2;
        codepoint = primeiro & 0x0FU;
    } else if ((primeiro & 0xF8U) == 0xF0U) {
        extras = 3;
        codepoint = primeiro & 0x07U;
    } else {
        ++pos;
        return '?';
    }

    // Sequencia truncada no fim da string.
    if (pos + extras >= texto.size()) {
        pos = texto.size();
        return '?';
    }

    for (std::size_t i = 1; i <= extras; ++i) {
        const unsigned char continuacao = byte(pos + i);
        if ((continuacao & 0xC0U) != 0x80U) {
            ++pos;
            return '?';
        }
        codepoint = (codepoint << 6) | (continuacao & 0x3FU);
    }
    pos += extras + 1;
    return codepoint;
}

}  // namespace

bool BitmapFont::carregar(Assets& assets, std::string_view caminhoFnt,
                          std::string_view caminhoPng) {
    const std::string arquivoFnt = paths::asset(caminhoFnt);
    std::ifstream entrada(arquivoFnt);
    if (!entrada) {
        JOGO_ERRO("Nao foi possivel abrir %s", arquivoFnt.c_str());
        return false;
    }

    std::string charset;
    std::string linha;
    while (std::getline(entrada, linha)) {
        if (linha.empty() || linha[0] == '#') {
            continue;
        }
        std::istringstream fluxo(linha);
        std::string chave;
        fluxo >> chave;
        if (chave == "cell") {
            fluxo >> larguraCelula_ >> alturaCelula_;
        } else if (chave == "cols") {
            fluxo >> colunas_;
        } else if (chave == "chars") {
            // O restante da linha e a lista de glifos, na ordem do atlas.
            charset = linha.substr(chave.size() + 1);
        }
    }

    if (larguraCelula_ <= 0 || alturaCelula_ <= 0 || colunas_ <= 0 || charset.empty()) {
        JOGO_ERRO("Metadados de fonte invalidos em %s", arquivoFnt.c_str());
        return false;
    }

    indicePorCodepoint_.clear();
    std::size_t pos = 0;
    int indice = 0;
    while (pos < charset.size()) {
        const Uint32 codepoint = proximoCodepoint(charset, pos);
        indicePorCodepoint_.emplace(codepoint, indice++);
    }

    atlas_ = assets.textura(caminhoPng);
    if (atlas_ == nullptr) {
        return false;
    }
    SDL_SetTextureBlendMode(atlas_, SDL_BLENDMODE_BLEND);
    return true;
}

void BitmapFont::desenhar(SDL_Renderer* renderer, std::string_view texto, float x, float y,
                          SDL_Color cor, float escala) const {
    if (atlas_ == nullptr) {
        return;
    }

    SDL_SetTextureColorMod(atlas_, cor.r, cor.g, cor.b);
    SDL_SetTextureAlphaMod(atlas_, cor.a);

    const float largura = static_cast<float>(larguraCelula_) * escala;
    const float altura = static_cast<float>(alturaCelula_) * escala;

    float cursorX = x;
    float cursorY = y;
    std::size_t pos = 0;
    while (pos < texto.size()) {
        const Uint32 codepoint = proximoCodepoint(texto, pos);
        if (codepoint == '\n') {
            cursorX = x;
            cursorY += altura;
            continue;
        }
        if (codepoint != ' ') {
            const auto it = indicePorCodepoint_.find(codepoint);
            if (it != indicePorCodepoint_.end()) {
                const int indice = it->second;
                const SDL_FRect recorte{
                    static_cast<float>((indice % colunas_) * larguraCelula_),
                    static_cast<float>((indice / colunas_) * alturaCelula_),
                    static_cast<float>(larguraCelula_),
                    static_cast<float>(alturaCelula_),
                };
                const SDL_FRect destino{cursorX, cursorY, largura, altura};
                SDL_RenderTexture(renderer, atlas_, &recorte, &destino);
            }
        }
        cursorX += largura;
    }

    SDL_SetTextureColorMod(atlas_, 255, 255, 255);
    SDL_SetTextureAlphaMod(atlas_, 255);
}

void BitmapFont::desenharCentralizado(SDL_Renderer* renderer, std::string_view texto,
                                      float centroX, float y, SDL_Color cor,
                                      float escala) const {
    const SDL_FPoint tamanho = medir(texto, escala);
    desenhar(renderer, texto, centroX - tamanho.x * 0.5f, y, cor, escala);
}

SDL_FPoint BitmapFont::medir(std::string_view texto, float escala) const {
    int maiorLinha = 0;
    int atual = 0;
    int linhas = 1;

    std::size_t pos = 0;
    while (pos < texto.size()) {
        const Uint32 codepoint = proximoCodepoint(texto, pos);
        if (codepoint == '\n') {
            maiorLinha = atual > maiorLinha ? atual : maiorLinha;
            atual = 0;
            ++linhas;
            continue;
        }
        ++atual;
    }
    maiorLinha = atual > maiorLinha ? atual : maiorLinha;

    return SDL_FPoint{
        static_cast<float>(maiorLinha * larguraCelula_) * escala,
        static_cast<float>(linhas * alturaCelula_) * escala,
    };
}

}  // namespace jogo

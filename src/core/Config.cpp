#include "core/Config.hpp"

#include <array>
#include <cstdlib>
#include <fstream>
#include <string_view>

#include "audio/Audio.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "input/Input.hpp"

namespace jogo::config {
namespace {

constexpr std::string_view kArquivo = "config.ini";

/// Tira os brancos das pontas. Nao mexe no meio: nome de tecla do SDL tem
/// espaco ("Left Shift", "Keypad Enter").
std::string_view aparar(std::string_view texto) {
    const auto branco = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!texto.empty() && branco(texto.front())) {
        texto.remove_prefix(1);
    }
    while (!texto.empty() && branco(texto.back())) {
        texto.remove_suffix(1);
    }
    return texto;
}

bool lerNumero(std::string_view valor, float& destino) {
    const std::string texto{valor};
    char* fim = nullptr;
    const float lido = std::strtof(texto.c_str(), &fim);
    if (fim == texto.c_str() || *fim != '\0') {
        return false;
    }
    destino = lido;
    return true;
}

bool lerBooleano(std::string_view valor, bool& destino) {
    if (valor == "1" || valor == "sim") {
        destino = true;
        return true;
    }
    if (valor == "0" || valor == "nao") {
        destino = false;
        return true;
    }
    return false;
}

/// Divide "tecla.interagir.1" em prefixo, acao e numero do vinculo. Devolve
/// false para qualquer chave que nao tenha essa forma.
bool lerChaveDeVinculo(std::string_view chave, std::string_view& prefixo, Acao& acao,
                       int& indice) {
    const std::size_t ponto1 = chave.find('.');
    if (ponto1 == std::string_view::npos) {
        return false;
    }
    const std::size_t ponto2 = chave.find('.', ponto1 + 1);
    if (ponto2 == std::string_view::npos) {
        return false;
    }

    prefixo = chave.substr(0, ponto1);
    acao = acaoPorNome(chave.substr(ponto1 + 1, ponto2 - ponto1 - 1));
    if (acao == Acao::Contagem) {
        return false;
    }

    const std::string numero{chave.substr(ponto2 + 1)};
    char* fim = nullptr;
    const long lido = std::strtol(numero.c_str(), &fim, 10);
    if (fim == numero.c_str() || *fim != '\0') {
        return false;
    }
    indice = static_cast<int>(lido);
    return true;
}

/// Estado da leitura dos vinculos: o mapa que esta sendo montado e a memoria de
/// quais acoes o arquivo mencionou. Uma acao citada no arquivo passa a valer
/// pelo que o arquivo diz -- os vinculos de fabrica dela saem inteiros, senao
/// apagar uma tecla a mao no arquivo nao teria efeito nenhum.
struct Vinculos {
    std::array<Input::Mapeamento, Input::kNumAcoes> mapa{};
    std::array<bool, Input::kNumAcoes> teclasCitadas{};
    std::array<bool, Input::kNumAcoes> botoesCitados{};
};

void aplicarVinculo(Vinculos& v, std::string_view prefixo, Acao acao, int indice,
                    std::string_view valor) {
    const std::size_t i = static_cast<std::size_t>(acao);
    const std::string nome{valor};

    if (prefixo == "tecla") {
        if (indice < 1 || indice > Input::kMaxTeclasPorAcao) {
            JOGO_ERRO("config: tecla %d fora de 1..%d em \"%s\"", indice,
                      Input::kMaxTeclasPorAcao, std::string{nomeDaAcao(acao)}.c_str());
            return;
        }
        const SDL_Scancode tecla = SDL_GetScancodeFromName(nome.c_str());
        if (tecla == SDL_SCANCODE_UNKNOWN) {
            JOGO_ERRO("config: tecla desconhecida \"%s\"", nome.c_str());
            return;
        }
        // A limpeza so acontece depois que a linha se provou boa: um nome com
        // erro de digitacao vira uma linha ignorada, e nao uma acao sem tecla
        // nenhuma -- errar o nome de "cima" nao pode tirar o Up e o W junto.
        if (!v.teclasCitadas[i]) {
            v.mapa[i].teclas.fill(SDL_SCANCODE_UNKNOWN);
            v.teclasCitadas[i] = true;
        }
        v.mapa[i].teclas[static_cast<std::size_t>(indice - 1)] = tecla;
        return;
    }

    if (indice < 1 || indice > Input::kMaxBotoesPorAcao) {
        JOGO_ERRO("config: botao %d fora de 1..%d em \"%s\"", indice, Input::kMaxBotoesPorAcao,
                  std::string{nomeDaAcao(acao)}.c_str());
        return;
    }
    const SDL_GamepadButton botao = SDL_GetGamepadButtonFromString(nome.c_str());
    if (botao == SDL_GAMEPAD_BUTTON_INVALID) {
        JOGO_ERRO("config: botao desconhecido \"%s\"", nome.c_str());
        return;
    }
    if (!v.botoesCitados[i]) {
        v.mapa[i].botoes.fill(SDL_GAMEPAD_BUTTON_INVALID);
        v.botoesCitados[i] = true;
    }
    v.mapa[i].botoes[static_cast<std::size_t>(indice - 1)] = botao;
}

}  // namespace

const std::string& caminho() {
    static const std::string arquivo = paths::pref(kArquivo);
    return arquivo;
}

bool carregar(SDL_Window* janela, Audio& audio, Input& input) {
    if (caminho().empty()) {
        return false;
    }
    std::ifstream entrada(caminho());
    if (!entrada) {
        return false;  // primeira execucao: nao e erro, so nao ha o que ler
    }

    Vinculos vinculos;
    for (std::size_t i = 0; i < Input::kNumAcoes; ++i) {
        vinculos.mapa[i] = input.mapeamento(static_cast<Acao>(i));
    }

    std::string linha;
    while (std::getline(entrada, linha)) {
        const std::string_view limpa = aparar(linha);
        if (limpa.empty() || limpa.front() == '#') {
            continue;
        }
        const std::size_t igual = limpa.find('=');
        if (igual == std::string_view::npos) {
            JOGO_ERRO("config: linha sem '=' ignorada: %s", std::string{limpa}.c_str());
            continue;
        }
        // O valor e o resto da linha inteiro, sem separador interno: nome de
        // tecla pode conter ',' e ';' (as proprias teclas), entao uma lista em
        // uma linha so seria ambigua -- por isso cada vinculo tem sua linha.
        const std::string_view chave = aparar(limpa.substr(0, igual));
        const std::string_view valor = aparar(limpa.substr(igual + 1));

        if (chave == "volume") {
            float volume = 0.0f;
            if (lerNumero(valor, volume)) {
                audio.definirVolume(volume);  // o Audio grampeia em 0..1
            } else {
                JOGO_ERRO("config: volume invalido \"%s\"", std::string{valor}.c_str());
            }
            continue;
        }
        if (chave == "tela-cheia") {
            bool cheia = false;
            if (lerBooleano(valor, cheia)) {
                if (janela != nullptr && !SDL_SetWindowFullscreen(janela, cheia)) {
                    JOGO_ERRO_SDL("SDL_SetWindowFullscreen");
                }
            } else {
                JOGO_ERRO("config: tela-cheia invalida \"%s\"", std::string{valor}.c_str());
            }
            continue;
        }

        std::string_view prefixo;
        Acao acao = Acao::Contagem;
        int indice = 0;
        if (lerChaveDeVinculo(chave, prefixo, acao, indice) &&
            (prefixo == "tecla" || prefixo == "botao")) {
            aplicarVinculo(vinculos, prefixo, acao, indice, valor);
            continue;
        }
        JOGO_ERRO("config: chave desconhecida \"%s\"", std::string{chave}.c_str());
    }

    for (std::size_t i = 0; i < Input::kNumAcoes; ++i) {
        input.definirMapeamento(static_cast<Acao>(i), vinculos.mapa[i]);
    }
    JOGO_INFO("Configuracao carregada de %s", caminho().c_str());
    return true;
}

bool salvar(SDL_Window* janela, const Audio& audio, const Input& input) {
    if (caminho().empty()) {
        return false;
    }
    std::ofstream saida(caminho(), std::ios::trunc);
    if (!saida) {
        JOGO_ERRO("Nao consegui escrever %s", caminho().c_str());
        return false;
    }

    const bool cheia =
        janela != nullptr && (SDL_GetWindowFlags(janela) & SDL_WINDOW_FULLSCREEN) != 0;

    saida << "# Preferencias do jogo, reescritas pelo proprio jogo.\n"
          << "# Apagar este arquivo (ou uma linha dele) restaura o padrao.\n\n";
    saida << "volume=" << audio.volume() << "\n";
    saida << "tela-cheia=" << (cheia ? 1 : 0) << "\n";

    saida << "\n# Vinculos: uma linha por tecla/botao, numeradas a partir de 1.\n"
          << "# Os nomes sao os do SDL (SDL_GetScancodeName,\n"
          << "# SDL_GetGamepadStringForButton). Citar uma acao aqui substitui\n"
          << "# todos os vinculos de fabrica dela.\n";
    for (std::size_t i = 0; i < Input::kNumAcoes; ++i) {
        const Acao acao = static_cast<Acao>(i);
        const std::string nome{nomeDaAcao(acao)};
        const Input::Mapeamento& m = input.mapeamento(acao);

        saida << "\n";
        int slot = 0;
        for (SDL_Scancode tecla : m.teclas) {
            ++slot;
            const char* nomeTecla = SDL_GetScancodeName(tecla);
            if (tecla == SDL_SCANCODE_UNKNOWN || nomeTecla == nullptr || *nomeTecla == '\0') {
                continue;
            }
            saida << "tecla." << nome << "." << slot << "=" << nomeTecla << "\n";
        }
        slot = 0;
        for (SDL_GamepadButton botao : m.botoes) {
            ++slot;
            const char* nomeBotao = SDL_GetGamepadStringForButton(botao);
            if (botao == SDL_GAMEPAD_BUTTON_INVALID || nomeBotao == nullptr) {
                continue;
            }
            saida << "botao." << nome << "." << slot << "=" << nomeBotao << "\n";
        }
    }

    return saida.good();
}

}  // namespace jogo::config

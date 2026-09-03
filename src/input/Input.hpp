#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

#include "core/SdlPtr.hpp"

namespace jogo {

/// Acoes logicas do jogo. As cenas consultam acoes, nunca teclas cruas, para
/// que o remapeamento fique concentrado na tabela do Input -- que comeca nos
/// vinculos de fabrica de Input.cpp e pode ser reescrita pela configuracao.
enum class Acao {
    Esquerda,
    Direita,
    Cima,
    Baixo,
    Confirmar,
    Voltar,
    Pausar,
    Interagir,
    Contagem,
};

/// Nome estavel da acao, na mesma ordem do enum. E o que aparece como chave no
/// arquivo de configuracao, entao renomear aqui invalida os vinculos salvos.
std::string_view nomeDaAcao(Acao acao);

/// Acao de um nome; Acao::Contagem quando o nome nao existe.
Acao acaoPorNome(std::string_view nome);

/// Snapshot de entrada por quadro: alem do estado atual guarda o do quadro
/// anterior, o que da as consultas de borda (pressionada/soltada).
class Input {
public:
    static constexpr std::size_t kNumAcoes = static_cast<std::size_t>(Acao::Contagem);
    static constexpr int kMaxTeclasPorAcao = 3;
    static constexpr int kMaxBotoesPorAcao = 2;

    /// Vinculos de uma acao. As posicoes nao usadas ficam em
    /// SDL_SCANCODE_UNKNOWN / SDL_GAMEPAD_BUTTON_INVALID, que as consultas
    /// ignoram -- e por isso um vinculo pode ser removido sem buraco na tabela.
    struct Mapeamento {
        std::array<SDL_Scancode, kMaxTeclasPorAcao> teclas{
            SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN};
        std::array<SDL_GamepadButton, kMaxBotoesPorAcao> botoes{
            SDL_GAMEPAD_BUTTON_INVALID, SDL_GAMEPAD_BUTTON_INVALID};
    };

    Input();

    void iniciar();

    /// Inicio do quadro: arquiva o estado anterior e le teclado, mouse e gamepads.
    void novoQuadro(SDL_Renderer* renderer);

    /// Avisa que a simulacao ja leu este estado. Enquanto isso nao acontece, as
    /// bordas (pressionada/soltada) sao preservadas entre quadros: com a tela a
    /// 240 Hz e a simulacao a 60 Hz, 3 de cada 4 quadros nao rodam update algum
    /// e uma tecla pressionada neles seria simplesmente perdida.
    void marcarConsumido() { consumido_ = true; }

    /// Eventos que nao aparecem em polling (hotplug de gamepad, roda do mouse).
    void onEvent(const SDL_Event& evento);

    bool acaoAtiva(Acao acao) const;
    bool acaoPressionada(Acao acao) const;
    bool acaoSolta(Acao acao) const;

    bool teclaAtiva(SDL_Scancode tecla) const;
    bool teclaPressionada(SDL_Scancode tecla) const;

    /// Posicao do cursor ja convertida para as coordenadas logicas do renderer.
    SDL_FPoint mouse() const { return mouse_; }
    bool botaoMouseAtivo(int botao) const;
    bool botaoMousePressionado(int botao) const;
    float rodaMouse() const { return roda_; }

    /// Direcao de movimento em [-1, 1] combinando teclado, direcional e analogico.
    SDL_FPoint eixoMovimento() const;

    bool temGamepad() const { return !gamepads_.empty(); }

    /// Vinculos em vigor de uma acao, e como troca-los (o remapeamento). O mapa
    /// e estado do Input, e nao uma tabela constante, justamente para que a
    /// configuracao possa reescreve-lo ao carregar.
    const Mapeamento& mapeamento(Acao acao) const;
    void definirMapeamento(Acao acao, const Mapeamento& mapeamento);

    /// Os vinculos de fabrica, para comparar ou voltar atras.
    static const Mapeamento& mapeamentoPadrao(Acao acao);
    void restaurarPadroes();

private:
    void abrirGamepad(SDL_JoystickID id);
    void fecharGamepad(SDL_JoystickID id);
    bool botaoGamepadAtivo(SDL_GamepadButton botao) const;
    bool botaoGamepadPressionado(SDL_GamepadButton botao) const;

    struct GamepadAberto {
        SDL_JoystickID id{0};
        GamepadPtr pad;
    };

    std::array<Mapeamento, kNumAcoes> mapa_{};

    std::array<bool, SDL_SCANCODE_COUNT> teclas_{};
    std::array<bool, SDL_SCANCODE_COUNT> teclasAntes_{};

    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> botoes_{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> botoesAntes_{};
    std::array<float, SDL_GAMEPAD_AXIS_COUNT> eixos_{};

    SDL_FPoint mouse_{0.0f, 0.0f};
    SDL_MouseButtonFlags botoesMouse_{0};
    SDL_MouseButtonFlags botoesMouseAntes_{0};
    float roda_{0.0f};
    float rodaAcumulada_{0.0f};

    bool consumido_{true};

    std::vector<GamepadAberto> gamepads_;
};

}  // namespace jogo

#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <vector>

#include "core/SdlPtr.hpp"

namespace jogo {

/// Acoes logicas do jogo. As cenas consultam acoes, nunca teclas cruas, para
/// que o remapeamento fique concentrado na tabela de Input.cpp.
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

/// Snapshot de entrada por quadro: alem do estado atual guarda o do quadro
/// anterior, o que da as consultas de borda (pressionada/soltada).
class Input {
public:
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

private:
    void abrirGamepad(SDL_JoystickID id);
    void fecharGamepad(SDL_JoystickID id);
    bool botaoGamepadAtivo(SDL_GamepadButton botao) const;
    bool botaoGamepadPressionado(SDL_GamepadButton botao) const;

    struct GamepadAberto {
        SDL_JoystickID id{0};
        GamepadPtr pad;
    };

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

#pragma once

#include <array>
#include <string_view>

#include "audio/Audio.hpp"
#include "scene/Scene.hpp"

namespace jogo {

/// Menu inicial: navegacao por teclado/gamepad, som nas transicoes e entrada
/// para a partida.
class MenuScene : public Scene {
public:
    void aoEntrar(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

private:
    enum class Opcao { Jogar, TelaCheia, Sair, Contagem };

    static constexpr std::array<std::string_view, 3> kRotulos{
        "Jogar",
        "Tela cheia (F11)",
        "Sair",
    };

    int selecao_{0};
    float tempo_{0.0f};
    Audio::SomId somMover_{0};
    Audio::SomId somConfirmar_{0};
};

}  // namespace jogo

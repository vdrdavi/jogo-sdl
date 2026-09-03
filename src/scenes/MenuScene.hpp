#pragma once

#include <string>

#include "audio/Audio.hpp"
#include "scene/Scene.hpp"

namespace jogo {

/// Menu inicial: navegacao por teclado/gamepad, som nas transicoes e entrada
/// para a partida. Tambem e onde ficam as preferencias -- volume e tela cheia
/// --, que o App grava em disco e reencontra na proxima execucao.
class MenuScene : public Scene {
public:
    void aoEntrar(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

private:
    enum class Opcao { Jogar, Volume, TelaCheia, Sair, Contagem };

    /// Passo do volume por toque; 20 toques atravessam a faixa inteira.
    static constexpr float kPassoVolume = 0.05f;

    /// O rotulo mostra o valor da preferencia, entao nao pode ser constante.
    static std::string rotulo(const Context& ctx, Opcao opcao);
    /// Esquerda/direita sobre a opcao selecionada. Devolve true se mudou algo.
    static bool ajustar(Context& ctx, Opcao opcao, int passo);

    int selecao_{0};
    float tempo_{0.0f};
    Audio::SomId somMover_{0};
    Audio::SomId somConfirmar_{0};
};

}  // namespace jogo

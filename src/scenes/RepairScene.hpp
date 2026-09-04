#pragma once

#include <SDL3/SDL.h>

#include "audio/Audio.hpp"
#include "core/Aleatorio.hpp"
#include "scene/Scene.hpp"
#include "sim/Flight.hpp"

namespace jogo {

/// A bancada de reparo do conves: solda o casco de volta, um ponto por vez.
///
/// O jogo e um compasso. Um ponteiro varre a barra de ponta a ponta e o jogador
/// aperta Confirmar dentro da zona marcada; cada acerto devolve um pedaco de
/// casco, encolhe a zona e acelera o ponteiro, entao a serie fica mais dificil
/// justamente porque esta indo bem. Errar nao custa casco -- custa tempo, que e
/// a unica moeda que importa aqui.
///
/// O que faz disto uma decisao, e nao um passatempo, e a arquitetura em volta: a
/// nave nao para de voar porque o piloto foi soldar. Como a FlightScene e a
/// StatusScene, esta cena nao e dona do voo (guarda uma referencia para o Flight
/// da InteriorScene, que sempre sobrevive a ela) e, bloqueando o update de quem
/// esta embaixo, passa a ser quem chama Flight::atualizar -- um passo por passo
/// fixo. Enquanto se solda, ninguem esta vendo o campo de rochas; a rocha que
/// chegar tira mais casco do que a serie inteira devolveu, e ainda quebra a
/// solda em andamento.
class RepairScene : public Scene {
public:
    explicit RepairScene(Flight& voo);

    void aoEntrar(Context& ctx) override;
    void atualizar(Context& ctx, float dt) override;
    /// O passo desta cena sem nenhuma decisao: o voo em piloto automatico, o
    /// relogio da tela e o tranco da rocha, que desmancha a solda esteja quem
    /// estiver olhando. O ponteiro **nao** anda aqui -- ele nao persegue a
    /// simulacao, e a mao do jogador, e um painel aberto por cima nao pode
    /// gastar a serie de quem ficou sem poder apertar nada.
    void acompanhar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

    /// O conves fica visivel atras da bancada, como no diagnostico -- inclusive
    /// a luz vermelha do casco critico, que e metade da graca de estar aqui.
    bool bloqueiaRender() const override { return false; }

private:
    static constexpr float kAlturaBarra = 20.0f;

    /// Casco devolvido por ponto de solda. Sao mais de tres acertos para pagar
    /// uma unica batida (0,125 de casco): a bancada compensa o estrago devagar,
    /// senao voar bem deixaria de importar.
    static constexpr float kGanhoPorPonto = 0.035f;

    /// A zona de acerto, em fracao da barra: a largura com que a serie comeca,
    /// o quanto ela encolhe a cada acerto e o quanto nunca passa disso.
    static constexpr float kZonaInicial = 0.17f;
    static constexpr float kZonaMinima = 0.055f;
    static constexpr float kEncolhimento = 0.84f;

    /// A varredura do ponteiro, em barras por segundo, e como ela acelera.
    static constexpr float kVelocidadeInicial = 0.62f;
    static constexpr float kAceleracao = 1.11f;
    static constexpr float kVelocidadeMaxima = 1.9f;

    /// A zona nova nunca nasce a menos que isto do ponteiro: seria um acerto de
    /// graca, e a serie deixaria de medir alguma coisa.
    static constexpr float kFolgaDaZona = 0.25f;

    /// O macarico frio depois de uma solda errada ou de uma batida: o ponteiro
    /// para, e sao estes segundos que a viagem cobra pelo erro.
    static constexpr float kTravaDoErro = 0.55f;

    /// Com que rapidez o realce do ultimo acerto (ou erro) se apaga.
    static constexpr float kDecaimentoRealce = 3.2f;

    /// Sorteia onde fica a proxima zona de acerto.
    void sortearZona();
    /// Volta a serie ao comeco e esfria o macarico: e o preco do erro e o do
    /// tranco da rocha, que sao o mesmo preco de proposito.
    void perderSerie();
    void avancarPonteiro(float dt);
    /// Julga o aperto do jogador contra a posicao do ponteiro.
    void soldar(Context& ctx);

    Flight& voo_;
    Aleatorio rng_;

    /// Onde o ponteiro esta na barra (0..1) e onde estava no passo anterior: a
    /// varredura anda no passo fixo e e o desenho que interpola, senao a 240 Hz
    /// o ponteiro andaria aos saltos de 60 Hz.
    float ponteiro_{0.0f};
    float ponteiroAnterior_{0.0f};
    /// +1 indo para a direita, -1 voltando. O ponteiro quica nas pontas em vez
    /// de reaparecer do outro lado: ver o retorno chegando e o que deixa
    /// antecipar a zona em vez de so reagir a ela.
    float sentido_{1.0f};

    float velocidade_{kVelocidadeInicial};
    float zonaCentro_{0.5f};
    float zonaMeia_{kZonaInicial * 0.5f};
    int pontos_{0};
    float trava_{0.0f};

    float realceAcerto_{0.0f};
    float realceErro_{0.0f};
    float tempo_{0.0f};

    /// A batida do passo anterior, para reconhecer a rocha nova: Flight::batida
    /// so sobe no instante do impacto e decai no resto do tempo.
    float batidaAnterior_{0.0f};

    /// A vista externa ja foi pedida para mostrar o fim da nave -- mesma trava
    /// da InteriorScene, pelo mesmo motivo: dois passos fixos podem cair no
    /// mesmo quadro.
    bool entregouADestruicao_{false};

    Audio::SomId somSolda_{0};
    Audio::SomId somErro_{0};
    Audio::SomId somVoltar_{0};
};

}  // namespace jogo

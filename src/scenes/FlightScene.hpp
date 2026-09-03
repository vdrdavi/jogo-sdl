#pragma once

#include <SDL3/SDL.h>

#include "audio/Audio.hpp"
#include "gfx3d/Destrocos.hpp"
#include "gfx3d/Mesh.hpp"
#include "gfx3d/Renderer3D.hpp"
#include "gfx3d/Starfield.hpp"
#include "scene/Scene.hpp"
#include "scenes/Transicao.hpp"
#include "sim/Flight.hpp"

namespace jogo {

/// Visao 3D: a cabine. Empilhada sobre a InteriorScene quando o jogador usa o
/// painel; Esc volta. Nao e dona do voo -- ele e da InteriorScene e continua
/// acontecendo depois que esta cena sai --, so o comanda e o desenha.
///
/// E tambem onde a nave acaba. Quando o casco zera, esta cena vira a **vista
/// externa da destruicao**: larga os controles, troca a nave pelos destrocos e,
/// passado o tempo de ver a coisa acontecer, apaga a tela e da lugar a
/// GameOverScene. Se o casco ceder com o jogador no conves ou no painel do
/// casco, e esta cena que e empilhada la para mostrar o mesmo -- a nave se
/// despedaca em cena, nunca fora dela.
class FlightScene : public Scene {
public:
    explicit FlightScene(Flight& voo);

    void aoEntrar(Context& ctx) override;
    void aoSair(Context&) override;
    void atualizar(Context& ctx, float dt) override;
    void desenhar(Context& ctx, float alpha) override;

private:
    static constexpr float kFovBase = 62.0f;
    static constexpr float kFovTurbo = 80.0f;
    /// Quanto o campo de visao nasce fechado: a cabine abre do painel para o
    /// espaco, entao a vista alarga em vez de aparecer pronta.
    static constexpr float kAberturaFov = 12.0f;
    /// A nevoa comeca antes da borda do campo, para as rochas emergirem do vazio.
    static constexpr float kNevoaInicio = 45.0f;
    static constexpr float kAmplitudeTremor = 0.55f;  // unidades de mundo
    /// Com que taxa a camera persegue a nave; o atraso e o que da peso as manobras.
    static constexpr float kPerseguicaoCamera = 9.0f;  // 1/s
    /// Quanto tempo os destrocos correm soltos antes de a tela comecar a
    /// apagar, e quanto dura o apagar. A soma e o que separa a batida final da
    /// tela de fim.
    static constexpr float kTempoDestrocos = 1.9f;   // s
    static constexpr float kApagarDaMorte = 1.4f;    // s
    /// A camera afrouxa a perseguicao quando nao ha mais nave: o que ela segue
    /// agora e a deriva dos destrocos, e o atraso maior abre distancia deles.
    static constexpr float kPerseguicaoMorte = 2.2f;  // 1/s

    /// Comeca a sequencia de destruicao: estilhaca a nave e larga os controles.
    void comecarDestruicao();

    /// Camera de terceira pessoa com atraso, guardada nos dois ultimos passos.
    Vec3 camera_{};
    Vec3 cameraAnterior_{};
    Vec3 cameraCima_{0.0f, 1.0f, 0.0f};

    /// A nave em que o jogador entrou; vive na cena de baixo, que sempre
    /// sobrevive a esta (a pilha so desempilha do topo).
    Flight& voo_;

    Renderer3D cena_;
    Starfield estrelas_;
    Mesh nave_;

    Transicao transicao_;

    /// O que restou da nave, e ha quanto tempo. `morrendo_` e a chave de tudo
    /// nesta cena: sem ela, a mesma tecla que voltava para o conves voltaria
    /// para uma nave que nao existe mais.
    Destrocos destrocos_;
    bool morrendo_{false};
    float tempoDeMorte_{0.0f};

    float fov_{kFovBase};
    float tempo_{0.0f};

    Audio::SomId somSaida_{0};
};

}  // namespace jogo

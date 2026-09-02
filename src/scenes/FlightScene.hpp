#pragma once

#include <SDL3/SDL.h>

#include "audio/Audio.hpp"
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

    float fov_{kFovBase};
    float tempo_{0.0f};

    Audio::SomId somSaida_{0};
};

}  // namespace jogo

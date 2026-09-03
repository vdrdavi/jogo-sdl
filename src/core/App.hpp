#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "audio/Audio.hpp"
#include "core/Config.hpp"
#include "core/Context.hpp"
#include "core/SdlPtr.hpp"
#include "core/Time.hpp"
#include "gfx/Assets.hpp"
#include "gfx/BitmapFont.hpp"
#include "input/Input.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneStack.hpp"

namespace jogo {

/// Dono da janela, do renderer, dos subsistemas e do laco principal.
class App {
public:
    /// Resolucao logica fixa: tudo e desenhado nessas coordenadas e o SDL cuida
    /// da escala/letterbox para o tamanho real da janela.
    static constexpr int kLarguraLogica = 640;
    static constexpr int kAlturaLogica = 360;

    App() = default;
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool iniciar(const std::string& titulo, int larguraJanela = 1280, int alturaJanela = 720);

    /// Empilha a cena inicial e roda ate o jogo pedir para sair.
    void rodar(ScenePtr cenaInicial);

    void sair() { rodando_ = false; }
    void alternarTelaCheia();

    /// Volume geral, em 0..1. Passa pelo App (e nao direto pelo Audio) porque e
    /// aqui que a mudanca vira arquivo de preferencias.
    float volume() const;
    void definirVolume(float v);

    SDL_Window* janela() { return janela_.get(); }
    SDL_Renderer* renderer() { return renderer_.get(); }
    float fps() const { return fps_; }

private:
    void encerrar();
    Context contexto();
    void processarEventos(Context& ctx);
    void atualizarTitulo(float dtReal);
    /// Marca que uma preferencia mudou; quem salva e o laco, depois da espera.
    void marcarConfigSuja() { esperaConfig_ = kEsperaConfig; }
    /// Salva o arquivo quando a espera acabar. Segurar a seta no volume muda a
    /// preferencia a cada passo, e reescrever o arquivo 60 vezes por segundo
    /// para gravar so o ultimo valor seria trabalho jogado fora.
    void salvarConfigSeSuja(float dtReal);

    WindowPtr janela_;
    RendererPtr renderer_;

    Input input_;
    Assets assets_;
    Audio audio_;
    BitmapFont fonte_;
    SceneStack cenas_;
    StepTimer relogio_;

    std::string titulo_;
    bool rodando_{false};
    bool sdlIniciado_{false};

    /// Quanto tempo sem novas mudancas antes de gravar as preferencias.
    static constexpr float kEsperaConfig = 0.6f;
    float esperaConfig_{0.0f};

    float fps_{0.0f};
    float acumuladorFps_{0.0f};
    int quadrosNoIntervalo_{0};
};

}  // namespace jogo

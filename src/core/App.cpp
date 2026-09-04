#include "core/App.hpp"

#include <cstdio>

#include "core/Log.hpp"

// A tela de depuracao so e compilada na build de depuracao (veja o
// CMakeLists.txt): fora dela o arquivo nem entra na lista de fontes, e o F3
// abaixo nao existe.
#ifdef JOGO_DEBUG
#include "scenes/DebugScene.hpp"
#endif

namespace jogo {

App::~App() {
    encerrar();
}

bool App::iniciar(const std::string& titulo, int larguraJanela, int alturaJanela) {
    titulo_ = titulo;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        JOGO_ERRO_SDL("SDL_Init");
        return false;
    }
    sdlIniciado_ = true;

    SDL_Window* janelaCrua = nullptr;
    SDL_Renderer* rendererCru = nullptr;
    if (!SDL_CreateWindowAndRenderer(titulo.c_str(), larguraJanela, alturaJanela,
                                     SDL_WINDOW_RESIZABLE, &janelaCrua, &rendererCru)) {
        JOGO_ERRO_SDL("SDL_CreateWindowAndRenderer");
        return false;
    }
    janela_.reset(janelaCrua);
    renderer_.reset(rendererCru);

    if (!SDL_SetRenderVSync(renderer_.get(), 1)) {
        // Sem vsync o jogo ainda roda: o passo fixo mantem a simulacao correta.
        JOGO_ERRO_SDL("SDL_SetRenderVSync (seguindo sem vsync)");
    }
    if (!SDL_SetRenderLogicalPresentation(renderer_.get(), kLarguraLogica, kAlturaLogica,
                                          SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        JOGO_ERRO_SDL("SDL_SetRenderLogicalPresentation");
        return false;
    }

    assets_.iniciar(renderer_.get());
    input_.iniciar();
    audio_.iniciar();  // ausencia de audio nao e fatal

    if (!fonte_.carregar(assets_, "fonts/mono.fnt", "fonts/mono.png")) {
        JOGO_ERRO("Fonte nao carregada; rode: python tools/gen_assets.py");
        return false;
    }

    // As preferencias entram depois de todos os subsistemas de pe: elas mexem
    // no volume do Audio, no modo da janela e no mapa de teclas do Input.
    // O arquivo nasce ja na primeira execucao, mesmo com tudo no padrao: hoje
    // ele e o unico jeito de remapear um controle, e um arquivo que so aparece
    // depois de mexer no volume seria um arquivo que ninguem encontra.
    if (!config::carregar(janela_.get(), audio_, input_)) {
        config::salvar(janela_.get(), audio_, input_);
    }

    JOGO_INFO("SDL %d.%d.%d | renderer: %s", SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
              SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()),
              SDL_GetRendererName(renderer_.get()));
    return true;
}

Context App::contexto() {
    return Context{*this, renderer_.get(), input_, assets_, audio_, fonte_, cenas_};
}

void App::alternarTelaCheia() {
    const bool cheia = (SDL_GetWindowFlags(janela_.get()) & SDL_WINDOW_FULLSCREEN) != 0;
    if (!SDL_SetWindowFullscreen(janela_.get(), !cheia)) {
        JOGO_ERRO_SDL("SDL_SetWindowFullscreen");
    }
    // Quem marca a preferencia como suja e o evento de entrada/saida da tela
    // cheia, nao esta funcao: o pedido pode demorar (ou nem ser atendido) e o
    // gerenciador de janelas tambem pode trocar o modo por conta propria.
}

float App::volume() const {
    return audio_.volume();
}

void App::definirVolume(float v) {
    audio_.definirVolume(v);
    marcarConfigSuja();
}

void App::salvarConfigSeSuja(float dtReal) {
    if (esperaConfig_ <= 0.0f) {
        return;
    }
    esperaConfig_ -= dtReal;
    if (esperaConfig_ <= 0.0f) {
        esperaConfig_ = 0.0f;
        config::salvar(janela_.get(), audio_, input_);
    }
}

void App::processarEventos(Context& ctx) {
    SDL_Event evento;
    while (SDL_PollEvent(&evento)) {
        // Deixa as coordenadas de mouse do evento no espaco logico do renderer.
        SDL_ConvertEventToRenderCoordinates(renderer_.get(), &evento);

        switch (evento.type) {
            case SDL_EVENT_QUIT:
                rodando_ = false;
                break;
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                marcarConfigSuja();
                break;
            case SDL_EVENT_KEY_DOWN:
                if (evento.key.scancode == SDL_SCANCODE_F11 && !evento.key.repeat) {
                    alternarTelaCheia();
                }
#ifdef JOGO_DEBUG
                // O F3 vive aqui, e nao em uma cena, porque a tela de depuracao
                // abre de qualquer lugar do jogo -- inclusive de cenas que
                // ignoram a entrada, como o menu durante uma cortina. E ele nao
                // e uma Acao do Input: nao e um controle do jogo para o jogador
                // remapear, e sim uma tecla da ferramenta.
                if (evento.key.scancode == SDL_SCANCODE_F3 && !evento.key.repeat) {
                    DebugScene::alternar(cenas_);
                }
#endif
                break;
            default:
                break;
        }

        input_.onEvent(evento);
        cenas_.aoEvento(ctx, evento);
    }
}

void App::atualizarTitulo(float dtReal) {
    acumuladorFps_ += dtReal;
    ++quadrosNoIntervalo_;
    if (acumuladorFps_ < 0.5f) {
        return;
    }

    fps_ = static_cast<float>(quadrosNoIntervalo_) / acumuladorFps_;
    acumuladorFps_ = 0.0f;
    quadrosNoIntervalo_ = 0;

    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s - %.0f FPS", titulo_.c_str(),
                  static_cast<double>(fps_));
    SDL_SetWindowTitle(janela_.get(), buffer);
}

void App::rodar(ScenePtr cenaInicial) {
    Context ctx = contexto();

    cenas_.empilhar(std::move(cenaInicial));
    cenas_.aplicarPendentes(ctx);

    rodando_ = true;
    relogio_.iniciar();

    while (rodando_) {
        const float dtReal = relogio_.novoQuadro();

        input_.novoQuadro(renderer_.get());
        processarEventos(ctx);

        // Simulacao em fatias fixas, independente da taxa de quadros.
        bool simulou = false;
        while (relogio_.consumirPasso()) {
            cenas_.atualizar(ctx, StepTimer::kPassoFixo);
            simulou = true;
        }
        if (simulou) {
            input_.marcarConsumido();
        }
        audio_.atualizar();

        SDL_SetRenderDrawColor(renderer_.get(), 18, 20, 28, 255);
        SDL_RenderClear(renderer_.get());
        cenas_.desenhar(ctx, relogio_.alpha());
        SDL_RenderPresent(renderer_.get());

        // Transicoes de cena so aqui: nunca no meio de um update ou desenho.
        cenas_.aplicarPendentes(ctx);
        if (cenas_.vazia()) {
            rodando_ = false;
        }

        salvarConfigSeSuja(dtReal);
        atualizarTitulo(dtReal);
    }

    cenas_.limpar();
    cenas_.aplicarPendentes(ctx);
}

void App::encerrar() {
    // Uma mudanca dos ultimos instantes ainda pode estar esperando para ser
    // gravada; fechar o jogo nao pode ser o jeito de perde-la.
    if (esperaConfig_ > 0.0f && janela_) {
        esperaConfig_ = 0.0f;
        config::salvar(janela_.get(), audio_, input_);
    }
    audio_.encerrar();
    assets_.limpar();
    renderer_.reset();
    janela_.reset();
    if (sdlIniciado_) {
        SDL_Quit();
        sdlIniciado_ = false;
    }
}

}  // namespace jogo

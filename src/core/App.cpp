#include "core/App.hpp"

#include <cstdio>

#include "core/Log.hpp"

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
            case SDL_EVENT_KEY_DOWN:
                if (evento.key.scancode == SDL_SCANCODE_F11 && !evento.key.repeat) {
                    alternarTelaCheia();
                }
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

        atualizarTitulo(dtReal);
    }

    cenas_.limpar();
    cenas_.aplicarPendentes(ctx);
}

void App::encerrar() {
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

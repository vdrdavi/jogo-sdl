#include "scenes/DebugScene.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

#include "audio/Audio.hpp"
#include "core/App.hpp"
#include "core/Paths.hpp"
#include "core/Time.hpp"
#include "gfx/BitmapFont.hpp"
#include "gfx/Draw.hpp"
#include "input/Input.hpp"
#include "scene/SceneStack.hpp"
#include "scenes/InteriorScene.hpp"
#include "sim/Flight.hpp"

namespace jogo {
namespace {

/// O veu quase esconde a cena de baixo, mas nao de todo: dizer de que tela o
/// painel foi aberto e parte da informacao, e um numero lido por cima de um HUD
/// legivel e um numero mal lido.
constexpr SDL_Color kCorVeu{5, 8, 14, 246};
constexpr SDL_Color kCorLinha{40, 72, 100, 255};
constexpr SDL_Color kCorTitulo{150, 230, 255, 255};
constexpr SDL_Color kCorSecao{110, 195, 230, 255};
constexpr SDL_Color kCorRotulo{104, 124, 146, 255};
constexpr SDL_Color kCorValor{206, 226, 242, 255};
constexpr SDL_Color kCorAlerta{235, 110, 105, 255};

constexpr float kRadParaGrau = 57.29578f;

/// snprintf devolvendo string, para um campo caber em uma expressao. O painel e
/// remontado a cada quadro, mas sao duas dezenas de linhas curtas: o custo nao
/// aparece ao lado de um quadro do jogo, e um buffer por campo so deixaria o
/// codigo mais longo.
template <typename... Args>
std::string texto(const char* formato, Args... args) {
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), formato, args...);
    return std::string(buffer);
}

/// Um texto do SDL que pode vir nulo (rodando sem driver de video, por exemplo).
const char* ou(const char* valor, const char* alternativa) {
    return (valor != nullptr && valor[0] != '\0') ? valor : alternativa;
}

/// Corta um caminho comprido pela **esquerda**: o fim ("build/debug/assets/") e
/// o que diz de onde o jogo esta lendo; o comeco e quase sempre o mesmo.
std::string encurtar(std::string_view caminho, std::size_t limite) {
    if (caminho.size() <= limite || limite < 4) {
        return std::string(caminho);
    }
    return "..." + std::string(caminho.substr(caminho.size() - (limite - 3)));
}

/// Cursor de escrita de uma coluna: guarda o canto e desce uma linha a cada
/// campo. As medidas saem da metrica da fonte (alturaLinha/larguraGlifo) e nao
/// de constantes de pixel -- trocar a fonte muda o tamanho da celula.
class Coluna {
public:
    Coluna(const Context& ctx, float x, float y) : ctx_(ctx), x_(x), y_(y) {}

    void secao(std::string_view titulo) {
        ctx_.fonte.desenhar(ctx_.renderer, titulo, x_, y_, kCorSecao, 1.0f);
        y_ += ctx_.fonte.alturaLinha();
    }

    void campo(std::string_view rotulo, std::string_view valor, SDL_Color cor = kCorValor) {
        ctx_.fonte.desenhar(ctx_.renderer, rotulo, x_, y_, kCorRotulo, 1.0f);
        ctx_.fonte.desenhar(ctx_.renderer, valor, x_ + kRecuoDoValor * ctx_.fonte.larguraGlifo(),
                            y_, cor, 1.0f);
        y_ += ctx_.fonte.alturaLinha();
    }

    /// Respiro entre duas secoes: meia linha, para o bloco seguinte nao colar.
    void respiro() { y_ += ctx_.fonte.alturaLinha() * 0.5f; }

private:
    /// Onde comeca o valor, contado em glifos a partir do rotulo. Todos os
    /// rotulos cabem nesse recuo, e e isso que alinha a coluna dos valores.
    static constexpr float kRecuoDoValor = 9.0f;

    const Context& ctx_;
    float x_;
    float y_;
};

/// O voo em curso, procurado do topo da pilha para a base -- so para ler, que e
/// tudo que este painel faz com ele. Quem guarda o Flight e a InteriorScene (a
/// nave em que se anda e a mesma que voa); a cabine e o painel do casco so tem
/// referencias para o mesmo objeto, entao achar a InteriorScene mais alta ja e
/// achar a viagem. nullptr no menu.
Flight* vooNaPilha(SceneStack& cenas) {
    for (std::size_t i = cenas.tamanho(); i-- > 0;) {
        if (auto* interior = dynamic_cast<InteriorScene*>(cenas.em(i))) {
            return &interior->voo();
        }
    }
    return nullptr;
}

void desenharDesempenho(Coluna& coluna, const Context& ctx, float alpha) {
    coluna.secao("DESEMPENHO");

    // O contador e o mesmo que o App poe no titulo da janela: uma media dos
    // ultimos 0,5 s, e nao a duracao do ultimo quadro, que oscila demais para
    // ser lida.
    const float fps = ctx.app.fps();
    const float ms = fps > 0.0f ? 1000.0f / fps : 0.0f;
    coluna.campo("quadros",
                 texto("%.0f/s  %.1f ms", static_cast<double>(fps), static_cast<double>(ms)));
    coluna.campo("passo", texto("%.0f Hz  alpha %.2f",
                                static_cast<double>(1.0f / StepTimer::kPassoFixo),
                                static_cast<double>(alpha)));

    int vsync = 0;
    const char* modoVsync = "desconhecido";
    if (SDL_GetRenderVSync(ctx.renderer, &vsync)) {
        modoVsync = vsync == 0 ? "desligado" : (vsync < 0 ? "adaptativo" : "ligado");
    }
    coluna.campo("vsync", modoVsync);
    coluna.campo("cenas", texto("%zu na pilha", ctx.cenas.tamanho()));
}

void desenharOnde(Coluna& coluna, const Context& ctx) {
    coluna.secao("ONDE ESTA RODANDO");

    coluna.campo("sistema",
                 texto("%s  %d nucleos", SDL_GetPlatform(), SDL_GetNumLogicalCPUCores()));
    coluna.campo("memoria", texto("%.1f GB", static_cast<double>(SDL_GetSystemRAM()) / 1024.0));

    const int versao = SDL_GetVersion();
    coluna.campo("sdl", texto("%d.%d.%d", SDL_VERSIONNUM_MAJOR(versao),
                              SDL_VERSIONNUM_MINOR(versao), SDL_VERSIONNUM_MICRO(versao)));
    coluna.campo("video", ou(SDL_GetCurrentVideoDriver(), "nenhum"));
    coluna.campo("render", ou(SDL_GetRendererName(ctx.renderer), "nenhum"));

    int larguraJanela = 0;
    int alturaJanela = 0;
    SDL_GetWindowSizeInPixels(ctx.app.janela(), &larguraJanela, &alturaJanela);
    const bool cheia = (SDL_GetWindowFlags(ctx.app.janela()) & SDL_WINDOW_FULLSCREEN) != 0;
    coluna.campo("janela", texto("%dx%d %s", larguraJanela, alturaJanela,
                                 cheia ? "tela cheia" : "em janela"));
    coluna.campo("logica", texto("%dx%d letterbox", App::kLarguraLogica, App::kAlturaLogica));

    coluna.campo("audio", ctx.audio.ativo()
                              ? texto("%s  vol %.0f%%", ou(SDL_GetCurrentAudioDriver(), "?"),
                                      static_cast<double>(ctx.app.volume() * 100.0f))
                              : std::string("sem dispositivo"));
    coluna.campo("entrada", ctx.input.temGamepad() ? "teclado + gamepad" : "teclado");
}

void desenharNave(Coluna& coluna, Flight* voo) {
    coluna.secao("NAVE");

    if (voo == nullptr) {
        coluna.campo("estado", "sem viagem em curso", kCorRotulo);
        return;
    }

    // A integridade sai crua ao lado da porcentagem: o painel do casco
    // (StatusScene) e que traduz o numero em faixa e cor para o jogador; aqui
    // o que importa e o valor que o resto do codigo compara.
    const float casco = voo->casco();
    const SDL_Color corCasco = voo->destruida() ? kCorAlerta : kCorValor;
    coluna.campo("casco", texto("%.1f%%  (%.3f)", static_cast<double>(casco * 100.0f),
                                static_cast<double>(casco)),
                 corCasco);
    coluna.campo("estado", voo->destruida() ? "destruida" : (voo->turbo() ? "turbo" : "cruzeiro"),
                 corCasco);
    coluna.campo("veloc.", texto("%.1f u/s  (t %.2f)", static_cast<double>(voo->velocidade()),
                                 static_cast<double>(voo->fatorTurbo())));
    coluna.campo("batida", texto("%.2f", static_cast<double>(voo->batida())));

    const Flight::Pose& pose = voo->pose();
    coluna.campo("posicao", texto("%.0f %.0f %.0f", static_cast<double>(pose.posicao.x),
                                  static_cast<double>(pose.posicao.y),
                                  static_cast<double>(pose.posicao.z)));
    coluna.campo("rumo", texto("y%.0f p%.0f r%.0f", static_cast<double>(pose.yaw * kRadParaGrau),
                               static_cast<double>(pose.pitch * kRadParaGrau),
                               static_cast<double>(pose.roll * kRadParaGrau)));
    coluna.campo("rochas", texto("%d em raio %.0f", voo->rochas().quantidade(),
                                 static_cast<double>(voo->rochas().raio())));
}

}  // namespace

void DebugScene::alternar(SceneStack& cenas) {
    if (dynamic_cast<DebugScene*>(cenas.topo()) != nullptr) {
        cenas.desempilhar();
        return;
    }
    cenas.empilhar(std::make_unique<DebugScene>());
}

void DebugScene::aoEntrar(Context& ctx) {
    voo_ = vooNaPilha(ctx.cenas);
}

void DebugScene::atualizar(Context& ctx, float dt) {
    // O passo de quem esta congelado embaixo. Vem antes de qualquer saida:
    // fechar o painel nao pode custar um passo a viagem, nem dar um passo a
    // mais. Este painel nao simula nada por conta propria -- quem sabe se o
    // mundo anda e a cena de baixo: a cabine e o conves seguem voando por
    // aqui, a pausa continua sendo pausa, e o menu, menu.
    ctx.cenas.acompanharAbaixoDoTopo(ctx, dt);

    // O casco pode ceder com o painel aberto, e aqui nao ha nada a fazer: quem
    // entrega a vista externa e a cena de baixo, olhando Flight::destruida() --
    // que e estado, e nao um aviso que se perde --, no primeiro passo depois
    // que esta tela sair da frente.

    // Esc tambem fecha, alem do F3, que o App trata para funcionar de qualquer
    // lugar. Nenhuma outra tecla e lida: um painel de leitura nao comanda nada.
    if (ctx.input.acaoPressionada(Acao::Voltar)) {
        ctx.cenas.desempilhar();
    }
}

void DebugScene::desenhar(Context& ctx, float alpha) {
    const float larguraTela = static_cast<float>(App::kLarguraLogica);
    const float alturaTela = static_cast<float>(App::kAlturaLogica);
    const float linha = ctx.fonte.alturaLinha(1.0f);
    const float margem = 16.0f;

    draw::retanguloTela(ctx.renderer, SDL_FRect{0.0f, 0.0f, larguraTela, alturaTela}, kCorVeu);

    // Cabecalho: o que e a tela, e como sair dela.
    ctx.fonte.desenhar(ctx.renderer, "DEPURACAO", margem, margem * 0.5f, kCorTitulo, 1.0f);
    const std::string_view dica = "F3 ou Esc: fechar";
    ctx.fonte.desenhar(ctx.renderer, dica, larguraTela - margem - ctx.fonte.medir(dica, 1.0f).x,
                       margem * 0.5f, kCorRotulo, 1.0f);

    const float yRegua = margem * 0.5f + linha + 3.0f;
    draw::retanguloTela(ctx.renderer, SDL_FRect{margem, yRegua, larguraTela - margem * 2.0f, 1.0f},
                        kCorLinha);

    // Duas colunas: o que o programa esta fazendo a esquerda, a nave a direita.
    const float yTopo = yRegua + 8.0f;
    Coluna esquerda(ctx, margem, yTopo);
    desenharDesempenho(esquerda, ctx, alpha);
    esquerda.respiro();
    desenharOnde(esquerda, ctx);

    Coluna direita(ctx, larguraTela * 0.5f + 8.0f, yTopo);
    desenharNave(direita, voo_);

    // Os caminhos ficam embaixo, em largura inteira: nao cabem em uma coluna.
    const float yArquivos = alturaTela - margem - linha * 2.0f;
    draw::retanguloTela(
        ctx.renderer, SDL_FRect{margem, yArquivos - 8.0f, larguraTela - margem * 2.0f, 1.0f},
        kCorLinha);
    const std::size_t limite =
        static_cast<std::size_t>((larguraTela - margem * 2.0f) / ctx.fonte.larguraGlifo(1.0f)) - 9;
    Coluna arquivos(ctx, margem, yArquivos);
    arquivos.campo("assets", encurtar(paths::assetsRoot(), limite));
    const std::string& prefs = paths::prefRoot();
    arquivos.campo("config",
                   prefs.empty() ? "(o SDL nao soube responder)" : encurtar(prefs, limite));
}

}  // namespace jogo

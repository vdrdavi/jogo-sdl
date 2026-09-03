#include "scenes/InteriorScene.hpp"

#include <cmath>

#include "core/App.hpp"
#include "gfx/Assets.hpp"
#include "gfx/BitmapFont.hpp"
#include "gfx/Draw.hpp"
#include "input/Input.hpp"
#include "scenes/FlightScene.hpp"
#include "scenes/PauseScene.hpp"
#include "scenes/StatusScene.hpp"

namespace jogo {
namespace {

constexpr SDL_Color kCorHud{226, 232, 240, 255};
constexpr SDL_Color kCorPainel{12, 14, 22, 170};
constexpr SDL_FPoint kTamanhoJogador{20.0f, 12.0f};  // caixa de colisao (pes)
constexpr float kCelulaJogador = 32.0f;  // celula da folha textures/player.png

SDL_FPoint interpolar(SDL_FPoint anterior, SDL_FPoint atual, float alpha) {
    return SDL_FPoint{anterior.x + (atual.x - anterior.x) * alpha,
                      anterior.y + (atual.y - anterior.y) * alpha};
}

bool sobrepoe(const SDL_FRect& a, const SDL_FRect& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

}  // namespace

InteriorScene::InteriorScene()
    : camera_(static_cast<float>(App::kLarguraLogica), static_cast<float>(App::kAlturaLogica)) {}

SDL_FRect InteriorScene::limitesDoMundo() const {
    return mapa_.limites();
}

SDL_FRect InteriorScene::caixaDoJogador(SDL_FPoint centro) const {
    return SDL_FRect{centro.x - kTamanhoJogador.x * 0.5f, centro.y - kTamanhoJogador.y * 0.5f,
                     kTamanhoJogador.x, kTamanhoJogador.y};
}

void InteriorScene::moverComColisao(SDL_FPoint deslocamento) {
    // Um eixo de cada vez: assim o jogador desliza pela parede em vez de travar.
    const auto tentar = [this](float dx, float dy) {
        const SDL_FPoint candidato{posicao_.x + dx, posicao_.y + dy};
        const SDL_FRect caixa = caixaDoJogador(candidato);

        if (sobrepoe(caixa, console_)) {
            return;
        }

        const int x0 = static_cast<int>(std::floor(caixa.x / kTile));
        const int x1 = static_cast<int>(std::floor((caixa.x + caixa.w - 0.01f) / kTile));
        const int y0 = static_cast<int>(std::floor(caixa.y / kTile));
        const int y1 = static_cast<int>(std::floor((caixa.y + caixa.h - 0.01f) / kTile));

        for (int ty = y0; ty <= y1; ++ty) {
            for (int tx = x0; tx <= x1; ++tx) {
                if (mapa_.solido(tx, ty)) {
                    return;
                }
            }
        }
        posicao_ = candidato;
    };

    tentar(deslocamento.x, 0.0f);
    tentar(0.0f, deslocamento.y);
}

bool InteriorScene::pertoDoConsole() const {
    return sobrepoe(caixaDoJogador(posicao_), zonaDoConsole_);
}

void InteriorScene::aoEntrar(Context& ctx) {
    // O conves vem de assets/maps/conves.mapa; se o arquivo faltar ou estiver
    // torto, o MapaDeTiles loga e entrega uma sala fechada no lugar.
    mapa_.carregar("maps/conves.mapa");

    tiles_ = ctx.assets.textura("textures/interior.png");
    jogador_.textura = ctx.assets.textura("textures/player.png");
    jogador_.tamanho = SDL_FPoint{kCelulaJogador, kCelulaJogador};
    jogador_.ancora = SDL_FPoint{0.5f, 0.72f};  // ancora nos pes
    animacaoJogador_.definirCelula(kCelulaJogador, kCelulaJogador);
    animacaoJogador_.tocar(kParado);

    consoleSprite_.textura = ctx.assets.textura("textures/console.png");
    consoleSprite_.tamanho = SDL_FPoint{48.0f, 32.0f};
    consoleSprite_.ancora = SDL_FPoint{0.0f, 0.0f};

    somConfirmar_ = ctx.audio.carregar("audio/confirm.wav");

    const SDL_FRect mundo = limitesDoMundo();
    // Onde o painel fica e decisao do mapa: o marcador "console" da o canto
    // superior esquerdo. Sem ele, sobra o centro do conves como palpite.
    SDL_FPoint cantoDoConsole{mundo.w * 0.5f - consoleSprite_.tamanho.x * 0.5f,
                              static_cast<float>(2 * kTile)};
    mapa_.marcador("console", cantoDoConsole);
    console_ = SDL_FRect{cantoDoConsole.x, cantoDoConsole.y, consoleSprite_.tamanho.x,
                         consoleSprite_.tamanho.y};
    // Zona de interacao: logo a frente do painel.
    zonaDoConsole_ = SDL_FRect{console_.x - 10.0f, console_.y + console_.h, console_.w + 20.0f,
                               26.0f};

    // Nasce de frente para o painel: o convite [E] aparece de cara.
    posicao_ = SDL_FPoint{console_.x + console_.w * 0.5f, console_.y + console_.h + 14.0f};
    posicaoAnterior_ = posicao_;

    camera_.definirZoom(kZoom);
    camera_.definirPosicao(posicao_);
    camera_.limitarA(mundo);

    // A viagem comeca aqui e dura enquanto esta cena existir: o campo de rochas
    // e o ambiente do lado de fora nao pertencem a cabine.
    voo_.iniciar(ctx, 0xA57E01Du);
}

void InteriorScene::aoSair(Context& ctx) {
    voo_.encerrar(ctx);
}

void InteriorScene::aoRetomar(Context&) {
    // A cabine fechou. A cortina esta abaixada desde que ela abriu, entao o
    // conves reaparece de dentro dela -- e a camera desfaz o zoom pelo mesmo
    // caminho, de tras para frente. Voltar da pausa nao mexe em nada: ali a
    // transicao esta parada.
    if (transicao_.saindo()) {
        transicao_.iniciarEntrada();
    }
}

void InteriorScene::aproximarDoConsole(float dt) {
    // Zoom e alvo andam pela cobertura da cortina, nao pelo tempo: assim a
    // volta e o mesmo movimento invertido, sem uma segunda curva para manter.
    const float k = transicao_.cobertura();
    camera_.definirZoom(kZoom + (kZoomConsole - kZoom) * k);
    const SDL_FPoint painel{console_.x + console_.w * 0.5f, console_.y + console_.h * 0.5f};
    camera_.seguir(interpolar(posicao_, painel, k), dt, 9.0f);
    camera_.limitarA(limitesDoMundo());
}

void InteriorScene::animarJogador(SDL_FPoint direcao, float dt) {
    const bool andando = direcao.x != 0.0f || direcao.y != 0.0f;
    animacaoJogador_.tocar(andando ? kAndando : kParado);
    animacaoJogador_.atualizar(dt);
}

void InteriorScene::atualizar(Context& ctx, float dt) {
    tempo_ += dt;
    posicaoAnterior_ = posicao_;

    // Piloto automatico: sem comando, a nave segue reto -- e ainda pode bater.
    // Fica antes das saidas antecipadas para o voo nao perder passos; quando a
    // FlightScene esta no topo, ela e quem chama isto (e esta cena nem roda).
    voo_.atualizar(ctx, dt, Flight::Comando{});

    // O casco cedeu enquanto o jogador andava aqui dentro. Nao ha o que fazer
    // no conves de uma nave que acabou de se abrir: a vista vai para fora, que
    // e de onde se ve o que aconteceu. Sem cortina, e antes de qualquer outra
    // coisa -- inclusive de uma que ja estivesse fechando.
    if (voo_.destruida() && !entregouADestruicao_) {
        entregouADestruicao_ = true;
        ctx.cenas.empilhar(std::make_unique<FlightScene>(voo_));
        return;
    }
    if (entregouADestruicao_) {
        return;
    }

    // Com a cortina em cena o jogador ja nao comanda nada: nenhuma tecla e lida
    // (um segundo E empilharia uma segunda cabine) e a camera termina o
    // movimento sozinha. O voo, esse, continua recebendo o passo la em cima.
    if (transicao_.ativa()) {
        if (transicao_.avancar(dt)) {
            ctx.cenas.empilhar(std::make_unique<FlightScene>(voo_));
        }
        aproximarDoConsole(dt);
        // Sem tecla lida, o jogador esta parado de fato: o clipe segue o estado
        // e nao a tecla, senao ele congelaria no meio de um passo.
        animarJogador(SDL_FPoint{0.0f, 0.0f}, dt);
        return;
    }

    if (ctx.input.acaoPressionada(Acao::Pausar)) {
        ctx.cenas.empilhar(std::make_unique<PauseScene>());
        return;
    }

    // A outra opcao do painel: ler o casco sem assumir os controles. Aqui nao
    // ha cortina -- o painel e um overlay sobre o proprio conves, que continua
    // visivel atras --, entao a StatusScene e empilhada na hora.
    if (pertoDoConsole() && ctx.input.acaoPressionada(Acao::Diagnostico)) {
        ctx.audio.tocar(somConfirmar_);
        ctx.cenas.empilhar(std::make_unique<StatusScene>(voo_));
        return;
    }

    if (pertoDoConsole() && ctx.input.acaoPressionada(Acao::Interagir)) {
        ctx.audio.tocar(somConfirmar_);
        // A cabine so e empilhada quando a cortina cobrir a tela; ate la, o
        // conves continua rodando. O casco para de abafar ja agora, para o
        // ambiente do lado de fora abrir junto com a imagem.
        transicao_.iniciarSaida();
        voo_.definirAbafado(false);
        return;
    }

    const SDL_FPoint direcao = ctx.input.eixoMovimento();
    moverComColisao(SDL_FPoint{direcao.x * kVelocidade * dt, direcao.y * kVelocidade * dt});
    animarJogador(direcao, dt);

    if (direcao.x < 0.0f) {
        espelho_ = SDL_FLIP_HORIZONTAL;
    } else if (direcao.x > 0.0f) {
        espelho_ = SDL_FLIP_NONE;
    }

    camera_.seguir(posicao_, dt, 7.0f);
    camera_.limitarA(limitesDoMundo());
}

void InteriorScene::desenhar(Context& ctx, float alpha) {
    // Tremor da batida: a nave levou o tranco e o convés inteiro sacode. A
    // sacudida entra so aqui, numa copia da camera, para nao realimentar o
    // seguidor -- somada em camera_ ela viraria deriva no passo seguinte.
    Camera camera = camera_;
    if (voo_.batida() > 0.0f) {
        const float amplitude = kAmplitudeTremor * voo_.batida() * voo_.batida();
        camera.definirPosicao(
            SDL_FPoint{camera.posicao().x + std::sin(tempo_ * 61.0f) * amplitude,
                       camera.posicao().y + std::sin(tempo_ * 43.0f) * amplitude});
    }

    // Conves
    if (tiles_ != nullptr) {
        const SDL_FRect visivel = camera.areaVisivel();
        // O intervalo nao e grampeado ao mapa: fora dele o tile da borda se
        // repete (quem grampeia e MapaDeTiles::tile), entao o casco continua em
        // vez de abrir um vazio preto quando o tremor empurra a camera para fora.
        const int x0 = static_cast<int>(std::floor(visivel.x / kTile));
        const int y0 = static_cast<int>(std::floor(visivel.y / kTile));
        const int x1 = static_cast<int>(std::ceil((visivel.x + visivel.w) / kTile));
        const int y1 = static_cast<int>(std::ceil((visivel.y + visivel.h) / kTile));

        Sprite tile;
        tile.textura = tiles_;
        tile.tamanho = SDL_FPoint{static_cast<float>(kTile), static_cast<float>(kTile)};
        tile.ancora = SDL_FPoint{0.0f, 0.0f};

        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const Uint8 indice = mapa_.tile(x, y);
                tile.recorte = SDL_FRect{static_cast<float>(indice * kTile), 0.0f,
                                         static_cast<float>(kTile), static_cast<float>(kTile)};
                draw::sprite(ctx.renderer, camera, tile,
                             SDL_FPoint{static_cast<float>(x * kTile),
                                        static_cast<float>(y * kTile)});
            }
        }
    }

    // Console + brilho pulsante das telas
    draw::sprite(ctx.renderer, camera, consoleSprite_, SDL_FPoint{console_.x, console_.y});
    const float pulso = 0.5f + 0.5f * std::sin(tempo_ * 2.2f);
    draw::retanguloMundo(ctx.renderer, camera,
                         SDL_FRect{console_.x + 2.0f, console_.y + 8.0f, console_.w - 4.0f, 14.0f},
                         SDL_Color{90, 190, 240, static_cast<Uint8>(24.0f + pulso * 34.0f)});

    // Jogador
    const SDL_FPoint desenhada = interpolar(posicaoAnterior_, posicao_, alpha);
    draw::retanguloMundo(ctx.renderer, camera,
                         SDL_FRect{desenhada.x - 9.0f, desenhada.y - 2.0f, 18.0f, 5.0f},
                         SDL_Color{0, 0, 0, 80});
    // O balanco de quem esta parado vinha daqui, de um seno somado a posicao de
    // desenho; agora ele e o clipe "parado" da folha -- em vez de a imagem
    // inteira flutuar meio pixel, o tronco desce um pixel e as pernas ficam.
    Sprite jogador = jogador_;
    jogador.espelho = espelho_;
    jogador.recorte = animacaoJogador_.recorte();
    draw::sprite(ctx.renderer, camera, jogador, desenhada);

    // Convite de interacao, ancorado no console e em coordenadas de tela.
    if (pertoDoConsole()) {
        // Duas linhas de mesma largura: a tarja sai retangular e cada opcao
        // fica centralizada sobre o painel sem calculo a parte.
        const char* convite = "[E] Assumir os controles\n[Q] Diagnostico do casco";
        const SDL_FPoint acima =
            camera.mundoParaTela(SDL_FPoint{console_.x + console_.w * 0.5f, console_.y - 6.0f});
        const SDL_FPoint tamanho = ctx.fonte.medir(convite, 1.0f);
        const float subida = std::sin(tempo_ * 4.0f) * 1.5f;

        draw::retanguloTela(ctx.renderer,
                            SDL_FRect{acima.x - tamanho.x * 0.5f - 6.0f,
                                      acima.y - tamanho.y - 4.0f + subida, tamanho.x + 12.0f,
                                      tamanho.y + 6.0f},
                            SDL_Color{10, 14, 24, 200});
        ctx.fonte.desenharCentralizado(ctx.renderer, convite, acima.x,
                                       acima.y - tamanho.y - 1.0f + subida,
                                       SDL_Color{150, 230, 255, 255}, 1.0f);
    }

    // HUD
    const char* dica = ctx.input.temGamepad()
                           ? "analogico: andar   X: painel   Y: casco   Start: pausar"
                           : "WASD: andar   E: painel   Q: casco   Esc: pausar";
    const SDL_FPoint tamanhoDica = ctx.fonte.medir(dica, 1.0f);
    const float meio = static_cast<float>(App::kLarguraLogica) * 0.5f;
    const float yDica = static_cast<float>(App::kAlturaLogica) - 24.0f;
    draw::retanguloTela(ctx.renderer,
                        SDL_FRect{meio - tamanhoDica.x * 0.5f - 8.0f, yDica - 4.0f,
                                  tamanhoDica.x + 16.0f, tamanhoDica.y + 8.0f},
                        kCorPainel);
    ctx.fonte.desenharCentralizado(ctx.renderer, dica, meio, yDica, kCorHud, 1.0f);

    transicao_.desenhar(ctx.renderer);
}

}  // namespace jogo

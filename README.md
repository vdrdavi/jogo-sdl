# Jogo em SDL3 — nave 2D + voo 3D

O jogador começa **dentro de uma nave**, em visão 2D de cima. No painel de
pilotagem da parede superior, apertar **E** assume os controles e a tela muda
para uma **visão 3D**: um caça low poly triangular atravessando um campo de
estrelas gerado proceduralmente. `Esc` volta ao interior.

Por baixo há uma base completa: janela + renderer com resolução lógica, laço de
passo fixo, entrada unificada (teclado/mouse/gamepad), câmera 2D, sprites de
atlas, colisão contra tiles, texto por fonte bitmap, áudio, pilha de cenas e um
rasterizador 3D próprio.

## Dependências

Apenas **SDL3 ≥ 3.4**, CMake ≥ 3.28 e um compilador C++20.

```sh
sudo pacman -S sdl3 cmake ninja      # Arch
```

Não são necessárias `SDL3_image`, `SDL3_ttf` nem `SDL3_mixer`: o SDL 3.4 já
carrega PNG (`SDL_LoadPNG`) e WAV (`SDL_LoadWAV`) no core, o texto usa uma fonte
bitmap gerada em `assets/fonts/` e o áudio usa `SDL_AudioStream` direto.

Os assets placeholder já vêm gerados em `assets/`. Para regerá-los (opcional)
é preciso Pillow:

```sh
python tools/gen_assets.py
```

## Build e execução

```sh
cmake --preset debug
cmake --build build/debug
./build/debug/jogo
```

Há também o preset `release` (RelWithDebInfo). Os assets são copiados para o
lado do executável a cada build, então o jogo roda de qualquer diretório.

## Controles

| Ação | Teclado | Gamepad |
|---|---|---|
| Andar / pilotar / navegar | WASD ou setas | analógico esquerdo / direcional |
| **Usar o painel** (interior) | **E** | X (botão oeste) |
| Turbo (voo 3D) | Espaço | A (botão sul) |
| Confirmar (menu) | Enter ou Espaço | A (botão sul) |
| Voltar ao console / sair | Esc | B / Back |
| Pausar | Esc ou P | Start |
| Tela cheia | F11 | — |
| Menu (na pausa) | M | — |

## Mapa do código

```
src/
├─ main.cpp              cria o App e empilha a MenuScene
├─ core/
│  ├─ App.*              janela, renderer, laço principal, F11, FPS no título
│  ├─ Time.hpp           StepTimer: acumulador de passo fixo + alpha
│  ├─ Paths.*            resolução do diretório de assets
│  ├─ Context.hpp        referências dos subsistemas entregues às cenas
│  ├─ SdlPtr.hpp         unique_ptr para os tipos do SDL
│  └─ Log.hpp            macros de log com SDL_GetError()
├─ input/Input.*         ações lógicas, estados de borda, gamepad com hotplug
├─ gfx/
│  ├─ Assets.*           cache de texturas (PNG/BMP)
│  ├─ Sprite.hpp         recorte de atlas, âncora, rotação, espelho, tinta
│  ├─ Camera.*           mundo↔tela, zoom, seguir alvo, limites do mundo
│  ├─ Draw.*             desenho de sprites/retângulos/linhas em mundo ou tela
│  └─ BitmapFont.*       texto UTF-8 a partir de um atlas + .fnt
├─ gfx3d/
│  ├─ Math3D.hpp         Vec3 e Mat3 (base ortonormal, Euler, olhar-para)
│  ├─ Mesh.*             malha low poly com cor por face + a nave
│  ├─ Renderer3D.*       projeção, recorte, culling, pintor, SDL_RenderGeometry
│  └─ Starfield.*        campo de estrelas procedural com rastro
├─ audio/Audio.*         WAVs em memória, vozes mixadas pelo dispositivo
├─ scene/                Scene (interface) e SceneStack (transições adiadas)
└─ scenes/               MenuScene, InteriorScene, FlightScene, PauseScene
```

### O 3D sem shaders

`Renderer3D` é um rasterizador por software que termina em uma única chamada de
`SDL_RenderGeometry` por quadro — nada de SDL_gpu, OpenGL ou SPIR-V:

1. cada face vira três vértices em espaço de mundo (`posição + rotação * vértice`);
2. descarte de faces traseiras pela normal contra a direção do olhar;
3. sombreamento *flat*: uma luz direcional fixa modula a cor da face;
4. recorte no plano próximo (Sutherland–Hodgman em um plano só), que pode
   transformar o triângulo em um quadrilátero — daí o leque de triângulos;
5. projeção em perspectiva com distância focal derivada do fov;
6. **algoritmo do pintor**: sem z-buffer, as faces são ordenadas de trás para
   frente antes de emitir a geometria.

O campo de estrelas (`Starfield`) sorteia posições com um xorshift semeado
dentro de um cubo e, a cada quadro, reposiciona cada estrela por *wrap* em torno
da câmera: um campo infinito com memória constante. O rastro sai de projetar a
estrela deslocada de `velocidade * Δt` e desenhar um quadrilátero que desvanece
na cauda, em blending aditivo.

### Como o laço funciona

`App::rodar()` mede o tempo real do quadro, acumula e simula em fatias fixas de
1/60 s (`StepTimer::kPassoFixo`), com teto de 0,25 s por quadro para evitar
avalanche de updates depois de um travamento. O que sobra no acumulador vira o
`alpha` passado ao desenho, usado para interpolar a posição (veja
`PlayScene::desenhar`). Assim a física é determinística e o desenho é suave em
qualquer taxa de quadros.

Tudo é desenhado em coordenadas lógicas de **640×360**
(`SDL_SetRenderLogicalPresentation` com letterbox); o SDL escala para o tamanho
real da janela e converte as coordenadas do mouse.

### Como adicionar uma cena

```cpp
class MinhaScene : public jogo::Scene {
public:
    void atualizar(jogo::Context& ctx, float dt) override { /* ... */ }
    void desenhar(jogo::Context& ctx, float alpha) override { /* ... */ }
};

// de dentro de outra cena:
ctx.cenas.empilhar(std::make_unique<MinhaScene>());
```

`bloqueiaUpdate()`/`bloqueiaRender()` controlam se as cenas abaixo continuam
simulando e aparecendo (é assim que `PauseScene` congela a partida sem escondê-la).
Transições são aplicadas no fim do quadro, então uma cena pode trocar a si mesma
com segurança durante o próprio `atualizar()`.

### Como adicionar assets

Solte os arquivos em `assets/` e carregue pelo caminho relativo:

```cpp
SDL_Texture* tex = ctx.assets.textura("textures/inimigo.png");
auto som        = ctx.audio.carregar("audio/pulo.wav");
```

Imagens: PNG ou BMP. Áudio: WAV. Para outros formatos (OGG, JPG, fontes
TrueType escaláveis), aí sim vale acrescentar `SDL3_image`/`SDL3_mixer`/`SDL3_ttf`
ao `CMakeLists.txt`.

## Próximos passos sugeridos

- Carregar o convés de um arquivo em vez de gerá-lo em `InteriorScene::gerarConves()`.
- Obstáculos no voo 3D (asteroides como malhas low poly) e colisão em 3D.
- Transição animada entre o interior e o voo (fade ou zoom na tela do console).
- Animação por quadros no `Sprite` (linha do atlas + temporizador).
- Salvar/carregar configurações com `SDL_GetPrefPath()`.

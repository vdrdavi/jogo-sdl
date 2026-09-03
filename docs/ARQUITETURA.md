# Arquitetura

Documento para quem vai mexer no código. Para o que o jogo é, como compilar e
como o laço e o 3D funcionam, veja o [README](../README.md); para *como cada
coisa foi feita e por quê*, com os termos explicados, veja o
[diário de desenvolvimento](DESENVOLVIMENTO.md).

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
│  ├─ Aleatorio.hpp      xorshift32 semeado (estrelas, rochas)
│  └─ Log.hpp            macros de log com SDL_GetError()
├─ input/Input.*         ações lógicas, estados de borda, gamepad com hotplug
├─ gfx/
│  ├─ Assets.*           cache de texturas (PNG/BMP)
│  ├─ Sprite.hpp         recorte de atlas, âncora, rotação, espelho, tinta
│  ├─ Animacao.*         temporizador de quadros sobre um atlas em grade
│  ├─ Camera.*           mundo↔tela, zoom, seguir alvo, limites do mundo
│  ├─ Draw.*             desenho de sprites/retângulos/linhas em mundo ou tela
│  └─ BitmapFont.*       texto UTF-8 a partir de um atlas + .fnt
├─ gfx3d/
│  ├─ Math3D.hpp         Vec3 e Mat3 (base ortonormal, Euler, olhar-para)
│  ├─ Mesh.*             malha low poly com cor por face + a nave e as rochas
│  ├─ Renderer3D.*       projeção, recorte, culling, névoa, pintor, RenderGeometry
│  ├─ Starfield.*        campo de estrelas procedural com rastro
│  └─ AsteroidField.*    campo de asteroides com wrap e colisão esfera-esfera
├─ sim/Flight.*          o voo da nave: pose, rochas, colisão e o ambiente
├─ world/MapaDeTiles.*   grade de tiles lida de um arquivo em assets/maps/
├─ audio/Audio.*         WAVs em memória, vozes (e loops) mixados pelo dispositivo
├─ scene/                Scene (interface) e SceneStack (transições adiadas)
└─ scenes/               MenuScene, InteriorScene, FlightScene, PauseScene
```

Quem é dono de tudo é o `App`: ele cria janela, renderer e subsistemas, e passa
a cada cena um `Context` com referências para eles (`input`, `assets`, `audio`,
`fonte`, `cenas`). Nenhum subsistema é global.

## Como adicionar uma cena

```cpp
class MinhaScene : public jogo::Scene {
public:
    void atualizar(jogo::Context& ctx, float dt) override { /* ... */ }
    void desenhar(jogo::Context& ctx, float alpha) override { /* ... */ }
};

// de dentro de outra cena:
ctx.cenas.empilhar(std::make_unique<MinhaScene>());
```

O voo da nave não pertence a nenhuma tela: `sim/Flight.*` é o estado da viagem
e vive na `InteriorScene`, que o atualiza em piloto automático enquanto o jogador
anda pelo convés. A `FlightScene` recebe uma referência para o mesmo `Flight` e
apenas o comanda e o desenha — sair da cabine não interrompe o voo, e uma batida
com o jogador lá dentro chega como sacudida da câmera e um baque abafado.

`bloqueiaUpdate()` e `bloqueiaRender()` controlam se as cenas abaixo continuam
simulando e aparecendo — é assim que a `PauseScene` congela a partida sem
escondê-la, e como a `FlightScene` cobre o interior da nave por inteiro.

As transições (`empilhar`, `desempilhar`, `substituir`) são **adiadas para o fim
do quadro**, então uma cena pode trocar a si mesma com segurança durante o
próprio `atualizar()` — é exatamente o que a `InteriorScene` faz ao apertar E.
Depois de um `desempilhar`, a cena que voltou a ser o topo recebe `aoRetomar`.

A passagem entre o convés e a cabine não é um corte: cada uma das duas cenas
guarda uma [`Transicao`](../src/scenes/Transicao.hpp) e a desenha por cima de si
mesma. A cortina fecha sobre o painel (com a câmera do convés dando zoom nele),
a cena só troca quando a tela está coberta, e do outro lado a mesma cor abre.
Ela é **estado de cena, não uma cena de overlay**: uma cena teria que sobreviver
à troca da cena de baixo — coisa que uma pilha não faz — e, ocupando o topo,
tiraria de quem está embaixo o passo de simulação do voo.

Cadastre o `.cpp` novo na lista de fontes do `CMakeLists.txt`.

## Como editar o cenário

O convés da `InteriorScene` é lido de `assets/maps/conves.mapa` por
[`world/MapaDeTiles`](../src/world/MapaDeTiles.hpp). O arquivo traz três
diretivas — `legenda <caractere> <nome-do-tile>`, `marcador <nome> <x> <y>` (em
tiles, aceita fração) e `mapa`, que abre a grade — e o próprio arquivo explica o
formato no cabeçalho. Um novo ambiente é um arquivo novo mais um
`carregar("maps/<nome>.mapa")`.

O que **não** está no arquivo: os nomes de tile válidos, o índice de cada um no
atlas e quais são sólidos. Isso vive na tabela `kDefinicoes` no topo de
[`MapaDeTiles.cpp`](../src/world/MapaDeTiles.cpp), porque descreve a textura e a
colisão, não o cenário. Acrescentar um tile é acrescentar um recorte ao atlas e
uma linha nessa tabela.

Qualquer falha de leitura (arquivo ausente, linha da grade com comprimento
diferente, caractere fora da legenda) é logada e substituída por uma sala
fechada de emergência: um cenário quebrado degrada o jogo, não o derruba.

## Como adicionar assets

Solte os arquivos em `assets/` e carregue pelo caminho relativo:

```cpp
SDL_Texture* tex = ctx.assets.textura("textures/inimigo.png");
auto som         = ctx.audio.carregar("audio/pulo.wav");
```

Imagens: PNG ou BMP. Áudio: WAV. Ambos são carregados pelo core do SDL, e as
texturas ficam em cache pelo caminho — pedir a mesma duas vezes não recarrega.

Um som contínuo usa `ctx.audio.tocarEmLoop(...)`, que devolve o handle da voz
para `ajustarGanho` e `parar` (com *fade*, se quiser). O WAV precisa emendar o
fim no começo — veja `gerar_ambiente` em `tools/gen_assets.py`.

Os placeholders versionados saem de `tools/gen_assets.py` (Pillow), que gera as
texturas, os efeitos sonoros e o atlas da fonte bitmap. Para formatos além de
PNG/WAV (OGG, JPG, fontes TrueType escaláveis), aí sim vale acrescentar
`SDL3_image`, `SDL3_mixer` ou `SDL3_ttf` ao `CMakeLists.txt`.

## Entrada

As cenas consultam **ações** (`Acao::Interagir`, `Acao::Confirmar`, ...), nunca
teclas cruas: o mapeamento para teclado e gamepad vive em uma tabela única no
começo de [`src/input/Input.cpp`](../src/input/Input.cpp). Adicionar um comando
é acrescentar um valor ao enum e uma linha na tabela.

As bordas (`acaoPressionada`) sobrevivem a quadros que não rodaram nenhum passo
de simulação. Isso importa: com a tela a 240 Hz e a simulação a 60 Hz, três de
cada quatro quadros não chamam `atualizar()`, e uma tecla apertada neles seria
perdida se o estado anterior fosse arquivado a cada quadro.

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Jogo 2D/3D em C++20 sobre SDL3: o jogador anda pelo interior de uma nave em 2D e,
no painel de pilotagem, passa para uma visão 3D de voo por estrelas procedurais.

## Comandos

```sh
cmake --preset debug && cmake --build build/debug     # preset release tambem existe
./build/debug/jogo
python tools/gen_assets.py                            # regera assets (precisa de Pillow)
cmake --install build/release --prefix <dir>
```

**Não há suíte de testes nem linter configurado.** O build é o controle de
qualidade: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, hoje sem nenhum
warning — mantenha assim. Como fumaça, rode o jogo sem sessão gráfica:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/release/jogo   # mate depois de alguns segundos
```

Arquivos `.cpp` novos precisam entrar na lista de fontes do `CMakeLists.txt` — há
glob só para assets, nunca para código.

## Como olhar o jogo de fato

Para conferir uma mudança visual, a forma determinística é um patch temporário no
laço de `App::rodar()` que grava o viewport com `SDL_RenderReadPixels` +
`SDL_SavePNG` e encerra. Rodando com `SDL_VIDEODRIVER=dummy`, a janela sai exatos
1280×720 (2× a resolução lógica), sem nada do desktop em volta — foi assim que
`docs/*.png` foram gerados. Injetar teclas no compositor (ydotool) é frágil: a
janela do jogo perde o foco e as teclas vão parar em outra aplicação do usuário.
Para exercitar uma cena específica, aponte `main.cpp` para ela num build
temporário e reverta depois.

## Arquitetura

O `App` é dono de janela, renderer e de todos os subsistemas; cada cena recebe um
`Context` com referências para eles (`input`, `assets`, `audio`, `fonte`,
`cenas`). Não há estado global.

**Laço de passo fixo.** `App::rodar()` acumula o tempo real e simula em fatias de
1/60 s, com teto de 0,25 s por quadro; o resto do acumulador vira o `alpha` que as
cenas usam para interpolar o desenho. Um acoplamento não óbvio: `App` só chama
`Input::marcarConsumido()` quando ao menos um passo rodou, e é isso que preserva
as bordas (`acaoPressionada`) em quadros sem update — com a tela a 240 Hz e a
simulação a 60 Hz, três de cada quatro quadros não chamam `atualizar()`. Se mexer
no laço, preserve essa relação.

**Pilha de cenas.** `empilhar`/`desempilhar`/`substituir` são adiados para o fim
do quadro, então uma cena pode trocar a si mesma durante o próprio `atualizar()`.
`bloqueiaUpdate()` e `bloqueiaRender()` decidem se as cenas abaixo continuam
simulando e aparecendo (`PauseScene` congela sem esconder; `FlightScene` cobre o
interior por inteiro).

**Coordenadas.** Tudo é desenhado em 640×360 lógicos com letterbox; o `App` já
converte as coordenadas de mouse dos eventos. Não escreva em pixels de janela.

**Assets.** `paths::assetsRoot()` prefere o diretório do binário
(`SDL_GetBasePath()`) e só cai no `JOGO_ASSETS_DIR` das fontes como último
recurso. Por isso a cópia de `assets/` no `CMakeLists.txt` depende dos *arquivos*
de assets (carimbo + lista gravada por `file(CONFIGURE)`), e não do relink do
alvo: amarrada a um `POST_BUILD`, mudar só um asset deixava a cópia velha e o
jogo carregava o arquivo antigo. Não volte para `POST_BUILD`.

**3D sem shaders.** `Renderer3D` é um rasterizador por software que termina em uma
chamada de `SDL_RenderGeometry` por lote: culling por normal, sombreamento flat,
recorte no plano próximo e **algoritmo do pintor** — sem z-buffer, geometria que se
interpenetra ou translúcida ordena errado. Convenção de orientação: a frente é
**-Z** (`Mat3::frente()` devolve `-colunas[2]`), então malhas novas devem ter o
nariz em -Z; a ordem dos vértices não importa, porque `orientarFacesParaFora()`
corrige o winding pela normal contra o centro da malha.

`Starfield` mantém um cubo de estrelas com wrap em torno da câmera (campo infinito
com memória constante); o rastro sai de projetar a estrela deslocada de `+v·Δt`,
porque a câmera é que andou.

**Texto.** `BitmapFont` lê um atlas PNG mais metadados `.fnt` gerados por
`tools/gen_assets.py`; a ordem do charset no `.fnt` é o índice no atlas. Posicione
texto com `medir()` e `alturaLinha()` em vez de constantes — trocar a fonte muda a
métrica da célula (já aconteceu: 11×18 → 8×16).

**Entrada.** As cenas consultam ações (`Acao::Interagir`, ...), nunca scancodes; o
mapeamento para teclado e gamepad vive numa tabela única no topo de
`src/input/Input.cpp`, na mesma ordem do enum.

**Áudio.** Sem SDL3_mixer: cada reprodução é um `SDL_AudioStream` ligado ao
dispositivo, que faz a mixagem, com carência de 250 ms antes de recolher a voz
para não cortar o fim do som.

## Dependências

**Só SDL3 ≥ 3.4.** O SDL 3.4 traz `SDL_LoadPNG` e `SDL_LoadWAV` no core, o texto
usa fonte bitmap e o áudio usa `SDL_AudioStream` — não acrescente `SDL3_image`,
`SDL3_ttf` ou `SDL3_mixer` sem uma necessidade real (OGG, JPG, TrueType
escalável). A fonte `assets/fonts/unscii-16.ttf` está em domínio público e
acompanha o repositório justamente para que gerar assets não dependa do sistema.

## Convenções

Identificadores e comentários em português; arquivos de `src/` sem acentuação,
`README.md` e `docs/` com acentuação normal. Mensagens de commit em português,
com o corpo explicando **por que** a mudança foi feita.

Mais detalhes em [README.md](README.md) e [docs/ARQUITETURA.md](docs/ARQUITETURA.md).

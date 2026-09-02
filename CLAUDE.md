# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Jogo 2D/3D em C++20 sobre SDL3: o jogador anda pelo interior de uma nave em 2D e,
no painel de pilotagem, passa para uma visão 3D de voo por estrelas procedurais.

## Diário de desenvolvimento

**Toda implementação nova entra em
[docs/DESENVOLVIMENTO.md](docs/DESENVOLVIMENTO.md), no mesmo commit da
implementação.** Não é um changelog: é o documento que explica o *processo* —
as decisões e o porquê delas, as alternativas descartadas, o que quebrou no
caminho e como a mudança foi conferida — e que explica **cada termo técnico**
que usa, porque o leitor não é necessariamente da área.

Uma entrada nova vai no fim da Parte 3 e traz:

- o pedido, em uma linha;
- as decisões e o **porquê**, incluindo o que foi descartado e por quê;
- os termos técnicos novos, explicados no texto e acrescentados ao glossário;
- os tropeços: o que quebrou, por que quebrou e como foi corrigido;
- **como a mudança foi verificada**, com os números que foram medidos.

As entradas se identificam por data e título; o hash do commit é opcional e só
aparece quando já existe (ao documentar algo commitado antes). Se a mudança
introduzir um jeito novo de conferir alguma coisa, a Parte 2 também cresce.

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

Para o áudio existe o equivalente: o driver `disk` do SDL grava o PCM cru em vez
de tocar, e aí o som vira número.

```sh
SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=saida.raw ./build/debug/jogo
```

O arquivo sai no formato do dispositivo (aqui S16LE estéreo a 44100 Hz); foi
assim que o loop do ambiente, os fades e o abafamento do casco foram conferidos.

Marque todo patch temporário com `// TEMP` e confira com `grep -rn "TEMP" src/`
antes de commitar.

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

**O voo não é uma cena.** `Flight` (`src/sim/Flight.*`) guarda pose, velocidade,
campo de rochas, colisão e o ambiente sonoro, e vive na `InteriorScene` — a nave
continua voando enquanto o piloto anda lá dentro. Quem chama `Flight::atualizar`
é a cena ativa: a `FlightScene` com o comando do jogador, a `InteriorScene` com
`Comando{}` (piloto automático, sem código extra: sem entrada tudo tende a
seguir reto). Como a `FlightScene` bloqueia o update da de baixo, o voo avança
**exatamente um passo por passo fixo** nos dois casos — se mexer nisso, confira
que continua assim. A `FlightScene` guarda uma referência para o `Flight` da
cena de baixo; isso é seguro porque a pilha só desempilha do topo, então o
interior sempre sobrevive à cabine.

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

`AsteroidField` usa o mesmo cubo com wrap, com duas diferenças que não são
enfeite: a rocha que atravessa a borda é **sorteada de novo** nos eixos que não
viraram (wrap puro deixa o campo periódico — voando reto, as mesmas pedras
voltam na mesma formação a cada travessia) e as malhas são normalizadas para
raio 1, o que faz a escala de desenho ser também o raio da esfera de colisão. A
névoa do `Renderer3D` (`definirNevoa`) existe para a rocha emergir do vazio em
vez de aparecer inteira na borda do campo, e de quebra descarta as faces que já
viraram a cor do fundo.

**Texto.** `BitmapFont` lê um atlas PNG mais metadados `.fnt` gerados por
`tools/gen_assets.py`; a ordem do charset no `.fnt` é o índice no atlas. Posicione
texto com `medir()` e `alturaLinha()` em vez de constantes — trocar a fonte muda a
métrica da célula (já aconteceu: 11×18 → 8×16).

**Entrada.** As cenas consultam ações (`Acao::Interagir`, ...), nunca scancodes; o
mapeamento para teclado e gamepad vive numa tabela única no topo de
`src/input/Input.cpp`, na mesma ordem do enum.

**Áudio.** O ambiente do lado de fora toca a viagem inteira, abafado por um
ganho menor enquanto o jogador está no convés (`Flight::definirAbafado`); a
rampa que faz a passagem entre convés e cabine ser um swell fica no `Flight`,
não na cena, senão a troca de cena viraria um degrau. Sem SDL3_mixer: cada reprodução é um `SDL_AudioStream` ligado ao
dispositivo, que faz a mixagem, com carência de 250 ms antes de recolher a voz
para não cortar o fim do som. Um loop (`tocarEmLoop`) é a mesma voz reabastecida
em `atualizar()` enquanto a fila do fluxo estiver com menos de meio segundo, e é
o único caso em que **não** se chama `SDL_FlushAudioStream`: o flush anuncia o
fim do sinal e faria o reamostrador zerar o estado a cada volta, marcando a
emenda. O WAV do loop também precisa emendar sozinho — `gerar_ambiente` em
`tools/gen_assets.py` roda o filtro do ruído marrom em círculo justamente para
isso.

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

Mais detalhes em [README.md](README.md), [docs/ARQUITETURA.md](docs/ARQUITETURA.md)
e [docs/DESENVOLVIMENTO.md](docs/DESENVOLVIMENTO.md).

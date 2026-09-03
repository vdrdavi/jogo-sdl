# Desenvolvimento

## O que é este documento

Este é o **diário técnico** do projeto: conta como cada coisa foi feita e por quê,
explicando os termos técnicos à medida que aparecem.

Ele existe porque os outros documentos respondem outras perguntas:

| Documento | Responde |
|---|---|
| [`README.md`](../README.md) | o que o jogo é e como compilar |
| [`docs/ARQUITETURA.md`](ARQUITETURA.md) | onde cada coisa mora no código |
| [`CLAUDE.md`](../CLAUDE.md) | os avisos de quem for mexer |
| **este** | **o processo: decisões, alternativas descartadas, o que quebrou e como foi conferido** |

O público é alguém que programa, mas não necessariamente já mexeu com jogos, 3D
ou áudio. Todo termo técnico é explicado na primeira vez que aparece e repetido
no [Glossário](#glossário) no fim.

> **Regra de manutenção:** toda implementação nova ganha uma entrada no
> [Diário](#parte-3--diário-de-implementações), **no mesmo commit** da
> implementação. A ordem está registrada no [`CLAUDE.md`](../CLAUDE.md).

---

## Parte 1 — O alicerce

Esta parte é o vocabulário: os mecanismos que já estavam de pé antes das
implementações do diário, e sem os quais o resto não se explica.

### 1.1 O laço principal e o passo fixo

Um jogo é um **laço** (*loop*): ler entrada, avançar o mundo um pouco, desenhar,
repetir. Cada volta é um **quadro** (*frame*), e **FPS** (*frames per second*) é
quantas voltas cabem em um segundo.

O tempo entre dois quadros — o **delta time** — varia: a máquina engasga, outra
janela rouba a CPU, o monitor é de 60 Hz ou de 240 Hz. Se a física usar esse
delta cru (`posição += velocidade * delta`), o jogo se comporta diferente em
cada máquina, e uma pausa longa faz o jogador atravessar paredes num salto só.

A solução clássica é o **passo fixo** (*fixed timestep*), em
[`src/core/Time.hpp`](../src/core/Time.hpp):

- o tempo real medido a cada quadro entra em um **acumulador**;
- enquanto houver 1/60 s no acumulador, roda-se um passo de simulação de
  exatamente 1/60 s e desconta-se essa fatia;
- o que sobra no acumulador (sempre menos de um passo) vira o **alpha**, uma
  fração em [0, 1).

A simulação, então, só enxerga fatias iguais: é **determinística** (mesma
entrada, mesmo resultado). E como o desenho acontece entre dois passos, cada
cena guarda o estado anterior e o atual e desenha a **interpolação** entre eles
usando o alpha — é isso que faz o movimento parecer suave a 240 Hz mesmo com a
simulação a 60 Hz.

O acumulador tem um teto de 0,25 s por quadro. Sem ele, um travamento de cinco
segundos mandaria rodar 300 passos de uma vez, o que travaria mais ainda, o que
acumularia mais tempo: a **espiral da morte** (*spiral of death*).

Um acoplamento não óbvio mora aí: `Input::marcarConsumido()` só é chamado quando
**algum** passo rodou. Entrada de **borda** (*edge*) — "a tecla acabou de ser
pressionada", diferente de "a tecla está pressionada" — é detectada comparando o
estado deste quadro com o do anterior. A 240 Hz com simulação a 60 Hz, três de
cada quatro quadros não rodam `atualizar()` nenhum; se o estado anterior fosse
arquivado a cada quadro, uma tecla apertada e solta dentro desses 4 ms sumiria.

### 1.2 Coordenadas lógicas e letterbox

Tudo é desenhado em uma resolução fixa de **640×360** — a *resolução lógica* —
e o SDL escala para o tamanho real da janela
(`SDL_SetRenderLogicalPresentation`). Quando a proporção da janela não bate com
16:9, sobram barras pretas: é o **letterbox**. O `App` também converte as
coordenadas do mouse dos eventos para esse espaço, então nenhum código de jogo
precisa saber o tamanho real da janela.

### 1.3 A pilha de cenas

Uma **cena** (`Scene`) é uma tela do jogo: menu, interior da nave, cabine,
pausa. Elas vivem em uma **pilha** (`SceneStack`), e a de cima é a ativa.

Duas perguntas decidem o que acontece com as de baixo:

- `bloqueiaUpdate()` — se `false`, a cena de baixo continua **simulando**;
- `bloqueiaRender()` — se `false`, a cena de baixo continua **aparecendo**.

É assim que a `PauseScene` congela a partida sem escondê-la.

Uma cena sabe quando entra (`aoEntrar`), quando sai (`aoSair`) e quando **volta
a ser o topo** porque a de cima desempilhou (`aoRetomar`) — é deste último que a
`InteriorScene` reabre a cortina ao sair da cabine.

As transições (`empilhar`, `desempilhar`, `substituir`) são **adiadas para o fim
do quadro**. Sem isso, uma cena que se trocasse durante o próprio `atualizar()`
destruiria o objeto no meio da execução do método dele — e invalidaria a pilha
que está sendo percorrida.

### 1.4 Entrada: ações, não teclas

As cenas perguntam por **ações** (`Acao::Interagir`, `Acao::Confirmar`), nunca
por **scancodes** (o código físico da tecla, independente do layout do teclado).
O mapeamento de ação para tecla e para botão de gamepad vive em uma tabela única
no topo de [`src/input/Input.cpp`](../src/input/Input.cpp). Adicionar um comando
é acrescentar um valor ao enum e uma linha na tabela.

### 1.5 Assets

**Asset** é qualquer arquivo de conteúdo: imagem, som, fonte. Ficam em
`assets/` e são carregados por caminho relativo.

As texturas 2D vêm de **atlas**: uma imagem só com vários desenhos lado a lado,
da qual se desenha um **recorte** por vez. Menos trocas de textura na placa de
vídeo e menos arquivos para gerenciar. `assets/textures/interior.png`, por
exemplo, tem os seis **tiles** (peças quadradas de 16×16 px com que o convés é
montado) em fila.

O texto usa uma **fonte bitmap**: em vez de desenhar curvas (**TrueType**), a
fonte já vem rasterizada como um atlas de glifos de tamanho fixo, mais um
arquivo `.fnt` com a métrica da célula e a ordem dos caracteres. Simples,
pixelado de propósito, e não precisa do `SDL3_ttf`.

Os assets versionados são gerados por [`tools/gen_assets.py`](../tools/gen_assets.py),
para nenhum deles ser um binário opaco que ninguém sabe refazer.

### 1.6 Áudio sem mixer

Som digital é **PCM** (*pulse-code modulation*): uma sequência de números, cada
um a amplitude da onda em um instante. A **taxa de amostragem** (*sample rate*)
diz quantos números por segundo (44100 Hz é o padrão de CD); a **profundidade**
diz o tamanho de cada número (16 bits com sinal, de -32768 a 32767). Um **WAV**
é praticamente isso com um cabeçalho.

**Mixar** é somar várias fontes em uma só saída. Como não usamos `SDL3_mixer`,
quem mixa é o próprio dispositivo de áudio do SDL: cada reprodução cria um
`SDL_AudioStream` (um tubo que converte formato/taxa e entrega amostras),
liga-o ao dispositivo e o dispositivo soma todos os tubos ligados nele.

Cada voz tem um **ganho** (multiplicador de amplitude; 1,0 é o original, 0,0 é
silêncio), aplicado na hora em que o dispositivo puxa os dados — mudar o ganho
vale no mesmo instante.

Uma voz só é recolhida 250 ms depois de o SDL terminar de ler seus dados: o
áudio já lido ainda está tocando no buffer do dispositivo, e destruir o tubo
antes cortaria o fim do som.

### 1.7 O 3D sem shaders

Não há OpenGL, Vulkan nem **shaders** (programinhas que rodam na placa de
vídeo). [`Renderer3D`](../src/gfx3d/Renderer3D.cpp) é um **rasterizador por
software**: transforma triângulos 3D em triângulos 2D na CPU e entrega tudo ao
SDL em uma chamada de `SDL_RenderGeometry` por quadro. A conta, face por face:

1. **Transformação**: cada vértice da malha vira posição no mundo
   (`posição + rotação * vértice`). Uma **malha** (*mesh*) é uma lista de
   vértices mais uma lista de triângulos que os indexam.
2. **Descarte de faces traseiras** (*back-face culling*): a **normal** de uma
   face — o vetor perpendicular a ela, obtido pelo **produto vetorial** de dois
   de seus lados — diz para que lado a face aponta. Se aponta no mesmo sentido
   do olhar, é o lado de dentro do sólido: não desenha. Corta metade do trabalho.
3. **Sombreamento flat**: a face inteira recebe uma cor só, proporcional ao
   ângulo entre a normal e a direção da luz (**luz difusa**, ou lei de Lambert),
   mais uma parcela **ambiente** fixa para as faces na sombra não ficarem
   pretas. "Flat" é isso: sem interpolar cor entre vértices.
4. **Recorte no plano próximo** (*near-plane clipping*): o que está atrás da
   câmera não pode ser projetado (dividir por uma profundidade negativa espelha
   a geometria). Corta-se o triângulo contra um plano logo à frente da câmera —
   é o algoritmo de **Sutherland–Hodgman** com um plano só. O resultado pode ter
   quatro vértices, daí o **leque de triângulos** (*triangle fan*) que o
   reconstitui.
5. **Projeção em perspectiva**: `x_tela = x / profundidade * distânciaFocal`. A
   **distância focal** sai do **FOV** (*field of view*, a abertura angular da
   câmera): quanto maior o FOV, mais curta a focal e mais "grande angular" a
   imagem. É por isso que abrir o FOV no turbo dá sensação de velocidade.
6. **Algoritmo do pintor**: sem **z-buffer** (a memória por pixel que guarda a
   profundidade do que já foi desenhado ali), a ordem é resolvida desenhando de
   trás para frente, como um pintor cobrindo o fundo. É barato e funciona para
   sólidos separados; geometria que se interpenetra ordena errado.

Convenção de orientação: a frente é **-Z**. Malhas novas devem ter o nariz em
-Z; a ordem dos vértices não importa, porque `orientarFacesParaFora()` corrige o
**winding** (o sentido horário/anti-horário em que os vértices de um triângulo
são listados, que é o que define para onde a normal aponta) comparando cada
normal com a direção do centro da malha até o centro da face.

### 1.8 O voo é estado, não tela

`Flight` ([`src/sim/Flight.*`](../src/sim/Flight.hpp)) guarda pose, velocidade,
campo de rochas, colisão e o ambiente sonoro. Ele **não** pertence à cabine:
vive na `InteriorScene`, porque a nave continua voando enquanto o piloto anda lá
dentro. Como isso foi parar aí está na
[entrada de diário correspondente](#2026-09-02--o-voo-sai-da-tela-do-voo-6714fc4).

---

## Parte 2 — Como uma mudança é feita e conferida

Não há suíte de testes automatizados neste projeto. O que existe no lugar:

### 2.1 O build é o controle de qualidade

A compilação roda com `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` e hoje
não emite **nenhum** aviso. `-Wconversion` (avisa quando um número muda de tipo
e pode perder informação) e `-Wshadow` (avisa quando uma variável interna
esconde outra de fora) são chatos de propósito: cada aviso novo é uma pergunta
que alguém precisa responder.

### 2.2 Rodar sem tela

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/release/jogo
```

Os **drivers dummy** do SDL fingem ter vídeo e áudio sem precisar de sessão
gráfica nem placa de som. Serve de **teste de fumaça** (*smoke test*): se o jogo
sobe, roda alguns segundos e sai sem erro, o básico não quebrou.

### 2.3 Ver o jogo de fato

Um patch temporário no laço de `App::rodar()` grava o quadro com
`SDL_RenderReadPixels` (lê os pixels do alvo de renderização) + `SDL_SavePNG` e
encerra. Com `SDL_VIDEODRIVER=dummy` a janela sai exatos 1280×720, sem nada do
desktop em volta — foi assim que os PNGs de `docs/` foram gerados.

Injetar teclas no compositor (ydotool) é frágil: a janela perde o foco e as
teclas vão parar em outra aplicação. Para chegar a uma cena específica, o
caminho é apontar `main.cpp` para ela — ou empilhá-la por tempo — em um build
temporário.

### 2.4 Ouvir o jogo de fato

O SDL tem um driver de áudio **disk**, que em vez de tocar escreve o PCM cru em
um arquivo:

```sh
SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=saida.raw ./build/debug/jogo
```

O arquivo sai no formato do dispositivo (aqui, S16LE estéreo a 44100 Hz) e pode
ser medido com qualquer script. É o equivalente sonoro do screenshot, e foi como
o loop do ambiente e os *fades* foram conferidos.

### 2.5 Medir, não achar

Toda afirmação sobre "está suave", "está audível", "está denso" neste projeto
vem de um número:

- **RMS** por janela de tempo, para volume percebido;
- diferença amostra a amostra na virada do loop **comparada com a média** do
  próprio sinal, para provar que a emenda não estala;
- deslocamento em pixels entre dois quadros, para provar que a câmera tremeu;
- contagem de eventos por minuto, para calibrar densidade.

### 2.6 Patches temporários

Todos os experimentos acima entram como patch temporário e saem antes do commit.
Convenção: marcar cada trecho com `// TEMP` e, no fim, `grep -rn "TEMP" src/`
tem que voltar vazio.

---

## Parte 3 — Diário de implementações

### 2026-09-02 — Ambiente sonoro do lado de fora (`f26b945`)

**Pedido:** "um som para o ambiente externo, algo como um brown noise".

#### O que é ruído marrom

**Ruído branco** é um sinal cuja energia é igual em todas as frequências — o
chiado de TV fora do ar. Ele soa agudo e áspero, porque nossa audição divide o
espectro em oitavas e as oitavas agudas são muito mais largas em hertz do que as
graves.

**Ruído marrom** (nome vindo do *movimento browniano*, não da cor) tem energia
caindo a **-6 dB por oitava**: cada vez que a frequência dobra, a amplitude cai
à metade. O resultado é grave e encorpado — cachoeira, motor distante, casco de
nave. Matematicamente é ruído branco **integrado** (somado ao longo do tempo).

Integrar de verdade tem um problema: a soma passeia sem limite (é literalmente
um passeio aleatório) e o sinal sai do intervalo utilizável. A saída é um
**integrador com vazamento** (*leaky integrator*):

```
y[n] = a·y[n-1] + (1-a)·x[n]
```

Isso é um **filtro IIR de um polo** (*infinite impulse response*: a saída
depende das saídas anteriores, então a resposta a um impulso nunca zera
completamente, só decai). Ele é um passa-baixa: acima da frequência de corte
cai a -6 dB/oitava — exatamente o ruído marrom — e abaixo dela é plano, o que
impede o passeio. Com corte em 18 Hz, o "plano" fica abaixo do audível.

#### O problema do loop e a solução

Um som ambiente precisa **repetir sem costura**. Se a última amostra do arquivo
não emenda na primeira, cada volta produz um degrau na forma de onda — e um
degrau é um **clique** audível.

A saída óbvia é o **crossfade** (misturar o fim com o começo), mas isso soma o
sinal com uma cópia deslocada dele mesmo, o que altera o espectro e apaga o
grave — justamente o que se quer preservar.

A saída usada é rodar o filtro **em círculo**: uma passada pelo sinal inteiro só
para aquecer o estado do filtro, e uma segunda passada, que é a que fica
gravada. Como a resposta do filtro decai (`a` elevado ao número de amostras é
desprezível: com corte de 18 Hz e 4 s de áudio, é `e^-452`), o estado ao fim de
uma volta **é** o estado de regime no ponto de partida. A emenda deixa de ser
uma emenda: é o meio do sinal.

Isso é verificável, e foi verificado: o salto entre a última e a primeira
amostra ficou em 571, contra um salto médio de 484 entre amostras vizinhas no
meio do arquivo. Estatisticamente indistinguível.

#### Normalizar por RMS, não por pico

**Pico** é a maior amplitude do arquivo; **RMS** (*root mean square*, a raiz da
média dos quadrados) é a amplitude "média" e corresponde melhor ao volume
percebido. Um ruído marrom tem picos erráticos: normalizar pelo pico deixaria o
volume à mercê de um único estouro raro. O arquivo é normalizado para
RMS = 0,16 e depois limitado, o que na prática nunca ceifa nada (0 amostras
ceifadas na geração).

#### O que mudou no `Audio`

O sistema só sabia disparar sons curtos. Ganhou **vozes contínuas**:

- `tocarEmLoop` devolve um `VozId` — um identificador próprio, e não um índice,
  porque o vetor de vozes remaneja quando uma voz é recolhida;
- a voz é **reabastecida** em `atualizar()` enquanto a fila do fluxo tiver menos
  de meio segundo de áudio;
- é o **único** caso em que não se chama `SDL_FlushAudioStream`. O *flush*
  anuncia "acabou o sinal", e o **reamostrador** (o conversor de taxa: 22050 Hz
  do arquivo para os 44100 Hz do dispositivo) zeraria o estado a cada volta,
  marcando a emenda justamente onde o trabalho foi feito para não haver uma;
- `parar(voz, segundos)` faz **fade** (rampa de ganho até zero) antes de
  recolher: cortar um ambiente longo de uma vez se ouve.

#### Verificação

Gravando pelo driver disk: o RMS por janela mostrou o *fade* de entrada
(0,008 → 0,023 → 0,039 → estável), o de saída (0,032 → 0 em 0,35 s) e nenhum
salto acima de 12× a média nas viradas do loop em 12 s de gravação.

---

### 2026-09-02 — Asteroides e colisão (`659cb6e`)

**Pedido:** a issue #2 — "o voo 3D hoje é só a nave e o campo de estrelas".

#### A malha

Cada rocha é um **icosaedro** (o poliedro regular de 20 faces triangulares e 12
vértices — o menor sólido que já passa por pedra) com cada vértice empurrado ao
longo do próprio raio por um fator aleatório, o que o "amassa".

Depois os vértices são **normalizados** para o maior raio valer 1. Isso não é
estética: com raio 1, a **escala** com que a malha é desenhada **é** o raio da
**esfera de colisão** (a esfera que envolve o objeto e substitui a geometria
real nos testes de colisão). Um número em vez de dois, e nenhuma chance de os
dois saírem de sincronia.

#### O campo com wrap

O campo reusa a ideia do `Starfield`: as rochas vivem em um **cubo** centrado na
nave e, quando uma sai por um lado, ela **envolve** (*wrap*) e entra pelo outro
— um campo infinito com memória constante (200 rochas, sempre).

**Primeiro tropeço.** Wrap puro torna o campo **periódico**: voando reto, as
mesmas pedras voltam na mesma formação a cada travessia do cubo (3,5 s no
cruzeiro). Com estrelas isso passa batido; rocha tem forma e a repetição
aparece. A correção: quem atravessa a borda volta **sorteada de novo** nos eixos
que não viraram. A troca acontece a uma aresta inteira de distância, dentro da
névoa, longe dos olhos.

**Segundo tropeço, mais sério.** A detecção do wrap comparava floats por
igualdade (`envolvido != relativo`). Mas a função de wrap soma e subtrai o raio,
e em ponto flutuante `(d + 110) - 110` **não** devolve `d` exatamente — o erro
de arredondamento em `float` perto de 110 é da ordem de 1e-5. Resultado: quase
toda rocha era considerada "recém-envolvida" a cada quadro, e tinha tamanho e
malha re-sorteados 60 vezes por segundo. O campo piscava.

A correção é comparar por **magnitude**: quem realmente virou andou uma aresta
inteira; o resto é ruído de arredondamento. Verificado capturando dois quadros
com 100 ms de intervalo e conferindo que são as mesmas rochas, apenas movidas.

#### A névoa

Um objeto sólido que aparece inteiro na borda do campo denuncia o truque. A
`Renderer3D` ganhou **névoa** (*fog*): a cor de cada face é misturada com a cor
do fundo conforme a profundidade, entre um início e um fim. A rocha **emerge**
do vazio em vez de aparecer. De brinde, a face que já virou 100% cor de fundo é
descartada antes de ser projetada — a névoa também é um **culling** de distância
de graça.

#### Densidade

Quantas rochas para o campo ser perigoso sem virar sopa? A conta é o **livre
caminho médio** da teoria cinética: `λ = 1/(n·σ)`, onde `n` é a densidade
(rochas por unidade de volume) e `σ` a **seção de choque** (a área do alvo
efetivo, `π·(raio_da_rocha + raio_da_nave)²`).

Com as 90 rochas iniciais em um cubo de meia-aresta 170 e o raio médio das
rochas, dava λ ≈ 3000 unidades — uma batida a cada ~48 s de voo reto no
cruzeiro. Feature morta. Encolhendo o cubo para 110 e subindo para 200 rochas,
λ ≈ 360, ou uma batida a cada ~6 s. Medido de verdade rodando 45 s: 9 batidas.

#### A batida

Bater custa quase toda a velocidade, sacode a câmera, dá um clarão e toca um
baque. A rocha atingida vai para outro canto do cubo: o campo não perde nem
ganha pedras, e nada é alocado durante o voo.

O clarão nasceu forte demais — um retângulo de cor quente cobrindo a tela a 59%
de opacidade transformava o preto do espaço em lavagem marrom. Baixado para 24%
com decaimento quadrático, virou o que devia ser: um susto de um terço de
segundo.

O baque (`impacto.wav`) reusa o gerador de ruído marrom, com corte mais alto
(110 Hz, para ter corpo em vez de só rugir) e um **envelope** de decaimento
exponencial — a curva que multiplica a amplitude ao longo do tempo, aqui com
3 ms de ataque para não estalar e `e^(-9t)` de queda.

---

### 2026-09-02 — O voo sai da tela do voo (`6714fc4`)

**Pedido:** o ambiente externo continuar tocando com o jogador no interior, e a
câmera interna vibrar se a nave bater enquanto ele está lá dentro.

O pedido parece de efeitos, mas é de arquitetura: para bater com o jogador no
convés, a nave precisa **estar voando** com o jogador no convés. E o voo era
estado da `FlightScene` — nascia ao abrir a cabine, morria ao sair dela.

#### As três opções

1. **Inverter a pilha de cenas**: o voo embaixo, o interior em cima com
   `bloqueiaUpdate() == false`. Elegante no papel, mas inverte o fluxo do jogo
   (o menu passaria a empilhar o voo) e a `FlightScene` teria que descobrir que
   não está no topo para não roubar a entrada do jogador que anda.
2. **Um subsistema no `App`**, entregue às cenas pelo `Context`, como `input` e
   `audio`. Coerente com o resto, mas o voo não é infraestrutura: é estado de
   uma partida, e o `App` não deveria saber que existe uma nave.
3. **Estado na `InteriorScene`**, com a `FlightScene` recebendo uma referência.
   A nave em que se anda é a mesma que voa: o dono natural do voo é o interior.

Escolhida a 3. A `FlightScene` passou a ser só piloto e vista do mesmo estado.

#### Piloto automático de graça

`Flight::atualizar` recebe um `Comando` (eixo e turbo). Sem comando, o rolamento
tende a zero, a velocidade tende à de cruzeiro e a nave segue reto — o piloto
automático não precisou de uma linha de código própria.

#### Exatamente um passo por passo fixo

Quem chama `Flight::atualizar` é a cena ativa: a `FlightScene` com o comando do
jogador, a `InteriorScene` com um comando vazio. As duas nunca rodam no mesmo
passo, porque a `FlightScene` bloqueia o update da cena de baixo. E como as
transições de cena são adiadas para o fim do quadro, também não há o quadro
"do meio" em que as duas rodariam. Se alguém mexer nisso, é a propriedade a
preservar: **um passo de voo por passo fixo**, nem zero, nem dois.

A referência que a `FlightScene` guarda é segura porque a pilha só desempilha do
topo — o interior sempre sobrevive à cabine.

#### O ambiente que atravessa o casco

O ganho do ambiente é multiplicado por 0,34 enquanto o jogador está no interior:
não é um filtro de verdade (abafar de verdade seria passa-baixa), mas o efeito
de "está do lado de fora" se sustenta.

A rampa que suaviza a passagem convés ↔ cabine mora no `Flight`, e não na cena.
Se morasse na cena, cada troca de cena seria um degrau — a cena nova começaria a
rampa do zero.

Medido pelo driver disk, em uma viagem convés → cabine → convés: RMS ≈ 0,013
dentro, ≈ 0,04 na cabine, com meio segundo de rampa entre os dois, e os baques
aparecendo como picos em ambos.

#### O tremor da câmera interna

Duas armadilhas:

1. **Realimentação.** A câmera 2D **persegue** o jogador com suavização
   exponencial. Somar o tremor na posição dela faria o passo seguinte perseguir
   a partir da posição já sacudida: o tremor viraria deriva. Solução: o tremor
   entra em uma **cópia** da câmera, só na hora de desenhar.
2. **O vazio preto.** O convés tem exatamente a largura da área visível
   (320 px de mapa, 320 px de viewport com zoom 2), então qualquer sacudida
   horizontal mostraria o nada fora do mapa. Solução: o desenho dos tiles
   deixou de ser grampeado ao mapa — o intervalo desenhado passa dos limites e é
   a **busca** no mapa que é grampeada, repetindo o tile da borda. O casco
   continua para fora da tela e a sacudida não abre buraco.

Verificado medindo o deslocamento entre os quadros da batida e um quadro parado:
(-12, +10), (+7, -6), (-5, -1) pixels de janela (o dobro dos lógicos) —
invertendo de sentido e decaindo.

#### Um detalhe que só apareceu olhando

A câmera de terceira pessoa persegue a nave com suavização exponencial a uma
taxa `k`. Perseguindo um alvo que corre a `v`, ela se estabiliza **atrás** dele
por `v/k` — é o **regime permanente** desse tipo de perseguição. Ao abrir a
cabine, a câmera era posta exatamente na posição desejada, sem esse atraso: a
cena abria com a nave no colo e recuava sozinha no primeiro meio segundo. Agora
ela nasce já com o atraso de regime.

---

### 2026-09-02 — A tela inicial sem título

**Pedido:** tirar o título e o subtítulo do menu.

A remoção em si é de duas linhas. O que veio junto é um detalhe de layout: o
bloco de opções começava em uma coordenada fixa (`y = 165`), escolhida na época
para cair **abaixo** do título. Sem o título, a metade de cima nascia vazia e o
menu ficava encostado embaixo — a captura de conferência mostrou isso de cara.

A posição passou a sair da altura do próprio bloco,
`(altura da tela − altura do bloco) / 2`, com a altura do bloco calculada a
partir da métrica da fonte (`alturaLinha`) e da quantidade de itens. É a mesma
razão pela qual o espaçamento entre itens já era calculado assim e não fixo:
trocar o atlas da fonte muda a célula, e o layout tem que acompanhar sozinho
(já aconteceu — a célula foi de 11×18 para 8×16).

**Verificação:** captura da tela inicial. Com a célula atual (8×16), o bloco
ocupa de 122 a 238 dos 360 pixels lógicos de altura — centrado.

---

### 2026-09-02 — O convés vem de um arquivo

**Pedido:** [issue #1](https://github.com/vdrdavi/jogo-sdl/issues/1) — carregar o
convés de um arquivo em vez de gerar em código, para editar o interior sem
recompilar e abrir caminho para mais de um ambiente.

#### O que existia

`InteriorScene::gerarConves()` montava os 20×13 tiles do convés com um laço
duplo e algumas condições: `y == 0` é teto, `x == 0` é parede, `y == 1` é a faixa
de janelas, e o piso ganhava grades onde `y % 5 == 0` e manchas escuras onde
`(x*5 + y*3) % 17 < 3`. Funciona, mas o cenário só existe compilado: mover uma
janela um tile para o lado é mexer em uma condição aritmética, recompilar e
rodar. E o segundo ambiente seria um segundo `gerar...()`.

#### O formato: texto simples, não JSON nem PNG

A issue sugeria CSV, JSON ou um PNG indexado. Os três foram descartados:

- **JSON** obrigaria uma biblioteca nova. O projeto tem uma regra explícita de só
  depender do SDL3, e um mapa de tiles não é uma estrutura aninhada — é uma
  grade.
- **PNG indexado** deixaria o mapa editável só em um editor de imagem, invisível
  no `git diff` e impossível de comentar. O `SDL_LoadPNG` até já está ali, mas o
  ganho é negativo.
- **CSV** de números resolve a grade, mas `3,3,3,5,5` não desenha nada para quem
  lê: o formato perde a única vantagem real do texto, que é o mapa **parecer** o
  mapa.

O escolhido é um arquivo de texto com **diretivas** (linhas `chave valor`) e uma
grade em ASCII no fim, onde cada caractere é um tile:

```
legenda . piso
legenda # parede
marcador console 8.5 2
mapa
####################
#-ooooo------ooooo-#
```

Uma **diretiva** é um comando de uma linha para quem lê o arquivo; a **legenda**
liga um caractere ao nome de um tile; um **marcador** é um ponto nomeado do
cenário, em coordenadas de tile. O parser é o mesmo estilo do `.fnt` da fonte
bitmap (`std::getline` + `std::istringstream`), então não entrou nenhuma técnica
nova no projeto — só um arquivo a mais.

#### O que ficou fora do arquivo, de propósito

Quais nomes de tile existem, qual o índice de cada um no atlas
`textures/interior.png` e quais bloqueiam o passo continuam em código, numa
tabela única no topo de `src/world/MapaDeTiles.cpp`:

```cpp
constexpr DefinicaoDeTile kDefinicoes[] = {
    {"piso", 0, false},  {"grade", 1, false}, {"piso-escuro", 2, false},
    {"parede", 3, true}, {"faixa", 4, true},  {"janela", 5, true},
};
```

A divisão é essa: o arquivo descreve **o cenário**, o código descreve **a
textura e a colisão**. Se a solidez fosse do arquivo, um mapa mal escrito
poderia declarar a parede atravessável — e um cenário não deveria conseguir
mudar as regras do jogo. O preço é que um tile novo exige recompilar; ele exigiria
de qualquer jeito, porque o recorte precisa existir no atlas.

Pelo mesmo motivo o `kTile = 16` continua constante: o lado do tile é o lado do
recorte no atlas, não uma escolha do cenário.

#### O marcador do console, e por que aceita fração

A posição do painel de pilotagem era `mundo.w * 0.5f - 24.0f` — centro do convés
menos metade da largura do sprite. Se o mapa manda no cenário, o painel também é
do mapa, então virou o marcador `console`. Só que 20 tiles de largura com um
painel de 3 tiles dá `20/2 − 1.5 = 8,5`: o centro **não** cai em uma fronteira de
tile. Em vez de empurrar o painel meio tile (mudaria o desenho) ou alargar o
convés, o marcador lê `float`. Coordenada de tile fracionária é normal para
objetos; quem é obrigado a cair na grade é só a grade.

O nascimento do jogador, que também era calculado do centro do convés, passou a
sair do console (`console_.x + console_.w * 0.5f`): ele nasce de frente para o
painel porque é isso que se quer, e agora isso vale onde quer que o painel esteja.

#### Falhar sem derrubar

Um asset ausente ou torto agora é uma forma de o jogo não abrir. `MapaDeTiles`
loga o motivo (com o caminho, e com a linha e as colunas quando é a grade que
está errada) e monta uma **sala de emergência**: piso cercado de parede, do
tamanho do convés original. A alternativa — mapa vazio — deixaria o jogador
dentro do nada, e a alternativa oposta — abortar — transformaria um erro de
digitação em um jogo que não roda.

#### O tropeço: `#` é comentário e é parede

O primeiro parser cortava a linha no primeiro `#`, como o leitor do `.fnt` faz.
Só que a parede é `#` na grade — e, portanto, `legenda # parede` no cabeçalho.
A linha virava `legenda` sem argumentos e o carregamento morria logo na primeira
execução:

```
ERROR: .../conves.mapa: legenda invalida "legenda"
```

A correção é estreitar a regra: comentário é a **linha inteira** que começa com
`#`, nunca um rabicho no fim. Dentro da grade nem isso — ali todo caractere é
tile. Perde-se o comentário de fim de linha, que este formato não precisa.

#### Verificação

O convés novo tinha que ser o convés velho, pixel por pixel — o objetivo era
mudar de onde o mapa vem, não como ele é. As 13 linhas da grade foram geradas
rodando o algoritmo antigo, e a conferência foi uma captura dos dois:

- `git worktree` em `HEAD`, com o mesmo patch `// TEMP` de captura
  (`SDL_RenderReadPixels` + `SDL_SavePNG` no quadro 90, `SDL_VIDEODRIVER=dummy`,
  `main.cpp` apontado para a `InteriorScene`);
- as duas imagens de 1280×720 comparadas com `ImageChops.difference`.

`getbbox()` da diferença deu `None`: **zero pixels diferentes** entre o convés
gerado em código e o convés lido do arquivo — grade, painel e posição inicial do
jogador incluídos.

Os caminhos de erro foram exercitados um a um, editando a cópia dos assets em
`build/debug/assets/`, e todos os cinco logaram e seguiram rodando:

| O que foi quebrado | O que saiu no log |
|---|---|
| arquivo removido | `Nao foi possivel abrir .../conves.mapa` |
| linha da grade com 19 colunas | `linha 3 da grade tem 19 colunas, esperava 20` |
| `X` na grade | `caractere 'X' fora da legenda` |
| diretiva `tamanho 3` | `diretiva desconhecida "tamanho"` |
| `marcador console` removido | (sem log: o centro do convés é o palpite) |

Build limpo do zero nos dois presets, sem nenhum warning, e o teste de fumaça
sem sessão gráfica passa. `grep -rn "TEMP" src/` não devolve nada.

### 2026-09-02 — A passagem entre o convés e a cabine (issue #3)

**O pedido:** apertar E trocava de tela em corte seco; o convés e a cabine
deviam se emendar por um fade ou por um zoom no painel.

#### O que havia e por que incomodava

`InteriorScene::atualizar` empilhava a `FlightScene` no mesmo passo em que a
tecla era lida. Como a pilha aplica as transições no fim do quadro, o quadro
seguinte já era o espaço: **um quadro de convés, o próximo de estrelas**. Não é
um bug — é o corte. O que falta é o olho entender que as duas imagens são o
mesmo lugar visto de dois jeitos.

#### A ideia da issue e por que ela não foi usada

A issue sugeria uma cena de *overlay* por cima das duas (`bloqueiaRender() ==
false`). Ela não serve, por duas razões que só aparecem quando se tenta:

1. **A cortina teria que sobreviver à troca da cena de baixo.** Uma transição de
   duas metades — escurecer sobre o convés, clarear sobre a cabine — precisa de
   um objeto que atravesse a troca. A `SceneStack` só mexe no **topo**
   (`empilhar`/`desempilhar`/`substituir`), e a cortina *é* o topo: para a
   `FlightScene` entrar embaixo dela seria preciso inventar um "empilhar abaixo
   do topo", ou seja, uma pilha que não é mais uma pilha.
2. **Ela roubaria o passo do voo.** Quem chama `Flight::atualizar` é a cena
   ativa. Uma cortina que bloqueia o update congelaria o voo (ou teria que
   pilotar a nave ela própria); uma que não bloqueia deixaria a cena de baixo
   continuar **lendo o teclado** durante a passagem — um segundo E empilharia
   uma segunda cabine.

#### O que foi feito: a cortina é estado, não cena

`Transicao` ([`src/scenes/Transicao.hpp`](../src/scenes/Transicao.hpp)) é uma
classe de trinta linhas que cada cena **guarda como membro** e desenha por cima
de si mesma, depois do HUD. Ela sabe só duas coisas: em que metade está
(fechando ou abrindo) e quanto já andou.

- `iniciarSaida()` fecha a cortina em 0,24 s e **fica fechada** — não se desliga
  sozinha. É isso que faz as duas metades emendarem: o convés continua coberto o
  tempo todo em que a cabine está por cima, e a volta continua de onde parou.
- `avancar(dt)` devolve `true` no **único** passo em que a saída acaba de cobrir
  a tela. Esse é o instante da troca de cena: quem chama decide o que fazer
  (`empilhar` a cabine, `desempilhar` de volta).
- `iniciarEntrada()` abre a cortina em 0,20 s a partir da tela cheia.

A cor é a mesma dos dois lados — `{14, 30, 48}`, o vidro escuro do painel, entre
o azul do convés e o `{5, 6, 14}` do espaço. É nela que as duas metades se
encontram, e é por isso que o encontro não aparece.

A curva é um **smoothstep** (`t²(3−2t)`), que sai e chega com velocidade zero.
Com rampa linear a mesma duração parecia mais curta e a cortina "batia" no fim.

#### O gancho que faltava: `aoRetomar`

`Scene` tinha `aoEntrar` e `aoSair`: uma cena sabe quando entra e quando sai,
mas **não sabe quando volta a ser o topo**. E é exatamente isso que a
`InteriorScene` precisa saber — é quando a cabine desempilha que ela tem de
reabrir a cortina.

A alternativa sem gancho era deduzir: "meu `atualizar` voltou a rodar enquanto
eu estou coberto, logo a cabine fechou". Funciona, porque a `FlightScene`
bloqueia o update de quem está embaixo — e é justamente por depender de um
detalhe de outra cena que foi descartada. Uma linha em `SceneStack` diz a mesma
coisa em voz alta:

```cpp
case Tipo::Desempilhar:
    if (!pilha_.empty()) {
        pilha_.back()->aoSair(ctx);
        pilha_.pop_back();
        if (!pilha_.empty()) {
            pilha_.back()->aoRetomar(ctx);
        }
    }
```

`aoRetomar` também dispara ao sair da pausa, e por isso a `InteriorScene` só age
se a cortina estiver fechada (`transicao_.saindo()`): despausar não é voltar de
lugar nenhum.

#### O zoom, de graça na volta

Enquanto a cortina fecha, a câmera do convés se inclina sobre o painel: o zoom
vai de 2,0 a 3,1 e o alvo do seguidor caminha do jogador para o centro do
console. O parâmetro dessa interpolação **não é o tempo, é a própria cobertura
da cortina** — então a volta é o mesmo movimento de trás para frente, sem uma
segunda curva para manter em sincronia com a primeira.

Do outro lado a cabine faz o complemento: nasce com o campo de visão 12° mais
fechado (`kFovBase − kAberturaFov`) e alarga na mesma suavização que já existia
para o turbo. O convés fecha em cima do painel, o espaço abre a partir dele.

O som acompanha: `definirAbafado` passa a ser chamado no **começo** da cortina
dos dois lados (antes era ao entrar e ao sair da `FlightScene`, isto é, no fim).
A rampa do `Flight` já transformava o degrau em *swell*; agora o swell começa
junto com a imagem em vez de 0,24 s depois dela.

#### O passo perdido, que só apareceu ao medir

A `FlightScene` antiga saía assim:

```cpp
if (ctx.input.acaoPressionada(Acao::Voltar) || ...) {
    ctx.audio.tocar(somSaida_);
    ctx.cenas.desempilhar();
    return;                 // <- antes de voo_.atualizar()
}
```

O `return` pulava o `voo_.atualizar` daquele passo, e a `InteriorScene` não
cobria a falta (a `FlightScene` ainda estava no topo, bloqueando o update dela).
Ou seja: **cada volta ao convés custava um passo do voo** — 1/60 s de viagem que
não aconteceu. Invisível a olho nu, mas é exatamente a invariante que a Parte 1.8
promete.

A reescrita não podia repetir isso, com uma cortina de 15 passos no meio. O
`atualizar` da cabine passou a ter um caminho só: a tecla apenas *inicia* a
saída, e o `voo_.atualizar` acontece sempre — com o comando do jogador enquanto
ele pilota, com `Comando{}` depois que a cortina começa a fechar (o piloto
largou os controles; o voo segue no automático, como quando ele anda pelo
convés).

#### Verificação

Captura determinística com o patch `// TEMP` de sempre (`SDL_RenderReadPixels` +
`SDL_SavePNG`, `SDL_VIDEODRIVER=dummy`, `main.cpp` apontado para a
`InteriorScene`), com a ida e a volta disparadas por contador de passos em vez de
tecla — injetar tecla no compositor é frágil, e aqui nem seria determinístico.

**A emenda não aparece.** Os quadros dos dois lados da troca, em média e em
extremos de cada canal (1280×720, RGB):

| Passo | Quem desenhou | Média | Mín | Máx |
|---|---|---|---|---|
| 44 | convés, cortina cheia | (14,0 / 30,0 / 48,0) | (14,30,48) | (14,30,48) |
| 45 | **cabine**, cortina abrindo | (13,1 / 29,1 / 46,1) | (13,29,46) | (17,34,51) |
| 84 | cabine, cortina cheia | (14,0 / 30,0 / 48,0) | (14,30,48) | (14,30,48) |
| 85 | **convés**, cortina abrindo | (13,9 / 30,1 / 47,3) | (13,29,46) | (19,35,52) |

No quadro em que a cena de baixo troca, a tela muda **no máximo 2 níveis por
canal** — contra o salto de imagem inteira do corte seco. (Os 13/29/46 do lado
que já está abrindo não são erro: com a cortina em 0,98 o renderer de software
mistura um fio do fundo por baixo, e o arredondamento inteiro do blend tira o
resto.)

**O voo não perde nem ganha passo.** Um contador `// TEMP` dentro de
`Flight::atualizar`, contra os 101 passos fixos que o `App` rodou no roteiro
(entrar na cabine, voar, voltar):

| Caminho | Passos do voo em 101 passos fixos |
|---|---|
| corte seco (antes) | **100** |
| com a cortina | **101** |

A posição avança 1,0333 unidade em **todos** os passos, inclusive nos dois em que
a cena de baixo troca: nem buraco, nem passo dobrado.

Build limpo do zero nos dois presets, sem nenhum warning, e o teste de fumaça sem
sessão gráfica passa. `grep -rn "TEMP" src/` não devolve nada.


### 2026-09-02 — O personagem ganha passos (issue #4)

**O pedido:** o `Sprite` desenha um recorte de atlas, mas o recorte é fixo; um
componente simples de animação — linha do atlas, número de quadros e um
temporizador — daria passos ao personagem dentro da nave.

#### O que havia

`assets/textures/player.png` era **uma** imagem de 32×32 e a `InteriorScene`
desenhava sempre ela. O único movimento do personagem era um truque no desenho:

```cpp
draw::sprite(ctx.renderer, camera, jogador,
             SDL_FPoint{desenhada.x, desenhada.y + std::sin(tempo_ * 5.0f) * 0.8f});
```

Um seno somado à posição de desenho — a figura inteira flutuava 0,8 unidade,
pés e antena juntos. Com a câmera em zoom 2× isso dá menos de dois pixels de
tela, e o que se via não era alguém respirando: era a imagem toda tremendo, sem
ponto de apoio. Andando, então, o personagem **deslizava**: mudava de lugar sem
mudar de forma.

#### O componente: quem anima não é quem desenha

[`gfx/Animacao.*`](../src/gfx/Animacao.hpp) é um temporizador sobre um atlas em
**grade regular**: a linha escolhe o clipe, a coluna escolhe o quadro.

```cpp
struct Clipe {      // só descrição: qual linha, quantos quadros, a que ritmo
    int linha{0};
    int quadros{1};
    float fps{8.0f};
};
```

O estado (que quadro está no ar) fica na `Animacao`, não no `Clipe` — assim o
mesmo clipe pode ser tocado por vários personagens, cada um no seu tempo. E a
`Animacao` não desenha nada: ela devolve um `SDL_FRect`, que é exatamente o
`Sprite::recorte` que o `Draw` já sabia usar desde o primeiro dia. **O `Sprite`
continua sem saber o que é uma animação.**

A grade regular é a decisão que faz o componente caber em cinquenta linhas. A
alternativa seria uma lista de recortes por quadro (o que um empacotador de
atlas produz, com cada quadro num canto diferente da textura). Ela é mais geral
e paga por isso: um arquivo de metadados por folha, para um projeto em que as
folhas saem de um script Python de trinta linhas. Enquanto `gen_assets.py` puder
desenhar a folha em grade, `linha × coluna` é o suficiente.

Duas escolhas menores, ambas por causa de um erro que dá para prever:

- **`tocar()` só reinicia se a linha for outra.** As cenas chamam `tocar()` a
  cada passo com o clipe do estado atual — é o jeito natural de escrever isso —,
  e reiniciar sempre travaria o personagem no quadro 0 enquanto ele anda.
  A linha do atlas é a identidade do clipe.
- **O acumulador desconta a duração em vez de zerar.** Com `tempo_ = 0` a cada
  troca de quadro, um clipe mais rápido que o passo fixo (fps > 60) andaria no
  ritmo do passo, e a cadência dependeria do `dt`. Descontando, o resto conta
  para o quadro seguinte.

E `atualizar()` é chamado do `atualizar` da cena, nunca do `desenhar`: com o
passo fixo do `App`, a animação fica igual em qualquer taxa de quadros — a mesma
razão pela qual a simulação já vivia lá.

#### A folha de sprites

`gerar_jogador` em `tools/gen_assets.py` passou a desenhar 4×2 células de 32 px
(128×64): linha 0 parado, linha 1 andando. Cada quadro é uma tripla de
deslocamentos — corpo, pé esquerdo, pé direito:

| Linha | Quadros | O que muda |
|---|---|---|
| 0 — parado | `(0,0,0) (0,0,0) (1,0,0) (1,0,0)` | o tronco desce um pixel e volta |
| 1 — andando | `(0,-4,0) (1,0,0) (0,0,-4) (1,0,0)` | um pé sobe de cada vez; no meio do passo o corpo abaixa |

O personagem ganhou pernas para o ciclo poder ser lido: o corpo, que ia até o
pixel 29 da célula, foi encurtado para 21, e duas pernas de 5×10 saem debaixo
dele. As pernas são desenhadas **antes** do corpo, para a junta ficar escondida
atrás dele, e o pé levantado sobe topo e base juntos (senão a perna estica em
vez de levantar — foi assim que saiu na primeira tentativa, com o pé
desaparecendo atrás do corpo).

O balanço do parado agora é o clipe 0, e o seno saiu do desenho. A diferença é
que o tronco desce **e as pernas ficam onde estão**: há um ponto de apoio.

#### Verificação

A cadência foi medida por um log `// TEMP` dentro do `animarJogador`, contando
quantos passos fixos cada quadro dura (a taxa de quadros do teste é irrelevante,
e é esse o ponto):

| Clipe | Passos por quadro | Esperado |
|---|---|---|
| andando (8 fps) | 7, 7, 8, 7, 8, 7, 8, 7 … | 60/8 = **7,5** |
| parado (2,5 fps) | 23, 24, 24, 24, 24 … | 60/2,5 = **24** |

O 7/8 alternado é o acumulador funcionando: nenhum dos dois valores é 7,5, mas a
média é — e não há deriva ao longo do tempo. A 96 u/s, um ciclo de quatro
quadros (30 passos, 0,5 s) cobre 48 unidades de mundo, uma passada e meia do
personagem: é essa relação que faz o pé parecer preso ao chão em vez de patinar.

Visualmente, com o `SDL_RenderReadPixels` + `SDL_SavePNG` de sempre e o
`main.cpp` apontado para a `InteriorScene`, dezesseis quadros seguidos com o
personagem andando para a direita: contando os pixels da cor da perna
(52,104,148) em cada captura, o total alterna entre **576** (dois pés no chão) e
**480** (um pé levantado, escondido atrás do corpo), e o topo do corpo oscila
entre y=255 e y=251 — o agachamento de um pixel lógico (dois de tela, no zoom
2×).

Build limpo do zero nos dois presets, sem nenhum warning, e o teste de fumaça sem
sessão gráfica passa. `grep -rn "TEMP" src/` não devolve nada.

### 2026-09-02 — As preferências sobrevivem ao fechamento (issue #5)

**O pedido:** volume, tela cheia e remapeamento de controles se perdiam ao
fechar o jogo; `SDL_GetPrefPath()` dá o diretório certo por plataforma para um
arquivo de configuração.

#### Onde escrever, e por que não é ao lado dos assets

`paths::assetsRoot()` prefere o diretório do executável. Escrever ali seria
errado por dois motivos independentes: uma instalação de verdade fica em um
lugar **somente leitura** (`/usr/bin`, `Program Files`), e mesmo quando dá para
escrever, um arquivo ao lado do binário não acompanha o usuário — dois usuários
da mesma máquina dividiriam o mesmo volume.

`SDL_GetPrefPath("jogo-sdl", "jogo")` responde a convenção de cada sistema
(`~/.local/share/…` no Linux, `%APPDATA%` no Windows,
`~/Library/Application Support/…` no macOS) e **já cria o diretório**. Virou
`paths::prefRoot()`, irmão de `assetsRoot()`: as duas raízes resolvidas uma vez
só, com a diferença de que a de preferências é a única em que o jogo escreve.

#### A decisão principal: a configuração não guarda os valores

O caminho óbvio seria uma classe `Config` com `float volume`, `bool telaCheia`,
uma tabela de teclas — e o jogo lendo dela. O problema aparece na segunda linha:
o volume **já tem dono**, o `Audio`; a tela cheia já é estado da janela; os
vínculos já são do `Input`. Uma cópia em `Config` seria um segundo estado, e
todo lugar que mexe em um teria de lembrar do outro. O primeiro esquecimento é
um bug silencioso: o jogo faz a coisa certa e salva a errada.

Então [`core/Config.*`](../src/core/Config.hpp) não é uma classe, é um par de
funções sem estado:

```cpp
bool carregar(SDL_Window* janela, Audio& audio, Input& input);
bool salvar(SDL_Window* janela, const Audio& audio, const Input& input);
```

Carregar é aplicar o arquivo **sobre** os donos; salvar é perguntar a eles. Não
existe um terceiro lugar para desincronizar.

Uma consequência disso é que o `Input` mudou: a tabela de vínculos era
`constexpr` no `.cpp` e virou estado do objeto (`mapeamento`,
`definirMapeamento`, `restaurarPadroes`), semeado com os vínculos de fábrica que
continuam `constexpr` no mesmo lugar. É o que torna o remapeamento possível —
sem isso, "remapeamento de controles" não teria o que persistir.

#### O formato: uma linha por vínculo

Texto `chave=valor`, com `#` de comentário — o mesmo tipo de arquivo que
`conves.mapa` já é. JSON ou binário custariam uma dependência ou um parser, e o
arquivo **precisa ser editável à mão**: hoje ele é o único jeito de remapear um
controle, porque não há tela de remapeamento.

A primeira versão punha os vínculos de uma ação em uma linha separada por
vírgulas. Não funciona: `SDL_GetScancodeName` devolve o nome da tecla, e a
tecla vírgula se chama `,`. Ponto e vírgula tem o mesmo problema, e espaço
também (`Left Shift`, `Keypad Enter`). Qualquer separador de um caractere colide
com alguma tecla. Por isso **cada vínculo tem sua linha, numerada**, e o valor é
o resto da linha inteiro:

```ini
volume=0.6
tela-cheia=0

tecla.interagir.1=E
botao.interagir.1=x
```

Os nomes vêm e voltam pelo SDL (`SDL_GetScancodeName` /
`SDL_GetScancodeFromName`, `SDL_GetGamepadStringForButton` /
`SDL_GetGamepadButtonFromString`), então o arquivo fala em `Left` e `dpleft`, e
não em números que ninguém consegue editar.

O arquivo **nasce na primeira execução**, mesmo com tudo no padrão. Um arquivo
que só aparece depois de a pessoa mexer no volume é um arquivo que ninguém
encontra — e ele é a interface de remapeamento.

Três detalhes de robustez, porque um arquivo editado à mão erra:

- Chave desconhecida, valor torto, linha sem `=`: loga e segue. O que não for
  lido fica no padrão. Um `config.ini` estragado não pode derrubar o jogo.
- **Citar uma ação substitui os vínculos de fábrica dela.** Sem isso, apagar
  uma linha à mão não teria efeito: o vínculo de fábrica voltaria por baixo.
- **Mas a substituição só acontece depois que a linha se prova boa.** Na
  primeira versão, `tecla.cima.1=Teclado Magico` limpava os vínculos de "cima"
  e *depois* descobria que o nome não existe — resultado: um erro de digitação
  deixava o jogador sem andar para cima, com um erro no log que ninguém leu.
  Agora a linha ruim é só ignorada.

#### Quando gravar: nem a cada mudança, nem só no fim

Gravar a cada mudança é o reflexo natural — e no volume ele custa caro: segurar
a seta muda a preferência **a cada passo fixo**, isto é, 60 reescritas do arquivo
por segundo para registrar só o último valor. Gravar apenas ao fechar perde tudo
se o jogo cair.

O `App` guarda uma espera: `marcarConfigSuja()` arma 0,6 s e cada nova mudança
rearma; quando a espera vence, grava. E o `encerrar()` grava o que ainda estiver
pendente, porque fechar o jogo não pode ser o jeito de perder a última mudança.

A tela cheia tem uma sutileza própria: quem marca a preferência como suja **não**
é `alternarTelaCheia()`, e sim os eventos `SDL_EVENT_WINDOW_ENTER_FULLSCREEN` /
`LEAVE_FULLSCREEN`. O pedido ao SDL pode demorar ou não ser atendido, e o
gerenciador de janelas pode trocar o modo por conta própria — o evento é o
único sinal que fala do que **aconteceu**, e não do que foi pedido.

#### Um lugar para mexer no volume

Não adiantaria persistir um volume que ninguém consegue mudar: ele nascia 0,6 no
`Audio` e ficava lá. A `MenuScene` ganhou duas linhas — `Volume: 60%` e
`Tela cheia: sim/não` — ajustadas com esquerda/direita, de 5% em 5%. O rótulo
mostra o valor, então ele deixou de ser constante e virou uma função. O *blip* de
navegação toca **depois** de aplicar o volume novo, e por isso já sai no volume
novo: o som é a prévia do ajuste.

O volume passa pelo `App` (`ctx.app.definirVolume`) e não direto pelo `ctx.audio`
porque é no `App` que a mudança vira arquivo.

#### Verificação

Tudo com `SDL_VIDEODRIVER=dummy`, olhando o arquivo em
`~/.local/share/jogo-sdl/jogo/config.ini`.

**Nasce sozinho.** Apagado o arquivo, uma execução o recria com os 8 conjuntos
de vínculos de fábrica, `volume=0.6` e `tela-cheia=0`.

**A ida e a volta.** Editado à mão para `volume=0.25`,
`tecla.interagir.1=Q`, `tecla.interagir.2=Keypad Enter`, `botao.interagir.1=y`,
a execução seguinte lê (log `// TEMP`):

```
TEMP volume=0.250 cheia=0 interagir=[Q|Keypad Enter|] botao=[y]
```

Note o `Keypad Enter` inteiro: é o caso que derrubaria um separador por espaço.

**O lixo é ignorado, e só ele.** Com cinco linhas erradas acrescentadas de
propósito:

| Linha | O que o jogo faz |
|---|---|
| `volume-total=9` | `config: chave desconhecida "volume-total"` |
| `tecla.voar.1=Z` | `config: chave desconhecida "tecla.voar.1"` |
| `tecla.pausar.9=Z` | `config: tecla 9 fora de 1..3 em "pausar"` |
| `tecla.cima.1=Teclado Magico` | `config: tecla desconhecida "Teclado Magico"` |
| `linha sem igual` | `config: linha sem '=' ignorada` |

e, na mesma execução, `TEMP cima=[Up|W|]` — a ação da linha com o nome errado
ficou **intacta**, que é o que a correção do item anterior garante.

**A espera funciona.** Um patch `// TEMP` chamando `definirVolume` em 30 passos
fixos seguidos (valores de 0,10 a 0,39): o log de gravação aparece **uma única
vez**, e o arquivo termina com `volume=0.39`. Sem a espera seriam 30 reescritas.

Build limpo do zero nos dois presets, sem nenhum warning, e o teste de fumaça sem
sessão gráfica passa. `grep -rn "TEMP" src/` não devolve nada.

## Glossário

| Termo | O que é |
|---|---|
| **Acumulador** | Onde o tempo real vai se juntando até dar um passo fixo inteiro. |
| **Algoritmo do pintor** | Desenhar de trás para frente para resolver oclusão sem z-buffer. |
| **Alpha (interpolação)** | Fração do passo ainda não simulada, usada para interpolar o desenho entre dois estados. |
| **Asset** | Arquivo de conteúdo: imagem, som, fonte. |
| **Atlas** | Uma imagem com vários desenhos lado a lado, dos quais se recorta um por vez. |
| **Back-face culling** | Descartar as faces que apontam para o lado oposto ao da câmera. |
| **Borda (entrada)** | O instante em que uma tecla passa de solta a pressionada (ou o contrário). |
| **Crossfade** | Misturar o fim de um trecho com o começo do outro. Altera o espectro. |
| **Ciclo de caminhada** | Sequência curta de quadros que se repete enquanto o personagem anda. |
| **Clipe (animação)** | Uma linha da folha de sprites tocada como sequência: quantos quadros e a que ritmo. |
| **Cortina (transição)** | Retângulo de cor que cobre a tela para emendar duas cenas sem corte. |
| **dB por oitava** | Quanto a amplitude cai cada vez que a frequência dobra. |
| **Delta time** | Tempo real decorrido entre dois quadros. |
| **Determinístico** | Mesma entrada, mesmo resultado, sempre. |
| **Diretiva** | Linha `chave valor` de um arquivo de dados, lida como um comando por quem carrega. |
| **Diretório de preferências** | Onde cada sistema guarda a configuração de um programa, por usuário (`SDL_GetPrefPath`). |
| **Driver dummy / disk** | Backends do SDL que fingem vídeo/áudio ou gravam o áudio em arquivo. |
| **Envelope** | Curva que multiplica a amplitude de um som ao longo do tempo (ataque, decaimento). |
| **Esfera de colisão** | Esfera que substitui a geometria real nos testes de colisão. |
| **Espera (debounce)** | Adiar uma ação até que as mudanças parem, para não repeti-la a cada instante. |
| **Espiral da morte** | Quando processar o atraso gera mais atraso do que o quadro consegue absorver. |
| **Filtro IIR de um polo** | Filtro cuja saída depende da saída anterior; aqui, um passa-baixa de -6 dB/oitava. |
| **FOV** | Abertura angular da câmera. Maior FOV = mais "grande angular". |
| **Fonte bitmap** | Fonte já rasterizada como atlas de glifos, em vez de curvas (TrueType). |
| **Folha de sprites** | Atlas em que cada célula é um quadro de animação; aqui, uma linha por clipe. |
| **Ganho** | Multiplicador de amplitude de um som. |
| **Icosaedro** | Poliedro de 20 faces triangulares e 12 vértices. |
| **Integrador com vazamento** | Soma acumulada que esquece o passado aos poucos, e por isso não passeia sem limite. |
| **Legenda (de mapa)** | A tabela que liga cada caractere da grade a um tile. |
| **Letterbox** | Barras pretas quando a proporção da janela não bate com a do jogo. |
| **Livre caminho médio** | Distância média percorrida entre duas colisões: `1/(densidade × seção de choque)`. |
| **Malha (mesh)** | Lista de vértices mais a lista de triângulos que os indexam. |
| **Marcador** | Ponto nomeado do cenário (ex.: onde fica o console), em coordenadas de tile. |
| **Mixar** | Somar várias fontes de áudio em uma saída. |
| **Névoa (fog)** | Misturar a cor das faces com a cor do fundo conforme a distância. |
| **Normal** | Vetor perpendicular a uma face; diz para que lado ela aponta. |
| **Overlay** | Cena desenhada por cima de outra, que continua aparecendo atrás. |
| **Passo fixo** | Simular sempre em fatias de tempo iguais, independentemente da taxa de quadros. |
| **PCM** | Som como sequência de amplitudes amostradas. |
| **Pico / RMS** | Maior amplitude / amplitude quadrática média (esta corresponde melhor ao volume percebido). |
| **Rasterizador por software** | Quem transforma triângulos em pixels usando a CPU, sem placa de vídeo. |
| **Reamostrador** | Conversor de taxa de amostragem (ex.: 22050 → 44100 Hz). |
| **Recorte no plano próximo** | Cortar os triângulos contra um plano à frente da câmera antes de projetar. |
| **Regime permanente** | O estado de equilíbrio para o qual um sistema converge; aqui, o atraso `v/k` da câmera. |
| **Resolução lógica** | A resolução fixa em que o jogo desenha (640×360), escalada depois para a janela. |
| **Ruído branco / marrom** | Energia igual em todas as frequências / caindo a -6 dB por oitava. |
| **Scancode** | Código físico da tecla, independente do layout do teclado. |
| **Seção de choque** | Área efetiva do alvo em um cálculo de colisão. |
| **Shader** | Programa que roda na placa de vídeo. Este projeto não usa nenhum. |
| **Sombreamento flat** | Uma cor por face, sem interpolar entre vértices. |
| **Smoothstep** | Curva `t²(3−2t)`: vai de 0 a 1 saindo e chegando parada. |
| **Sutherland–Hodgman** | Algoritmo de recorte de polígono contra um plano. |
| **Teste de fumaça** | Verificação mínima de que o programa sobe e roda. |
| **Tile** | Peça quadrada com que um cenário 2D é montado. |
| **Vínculo (binding)** | Ligação entre uma tecla ou botão e uma ação lógica do jogo. |
| **Vsync** | Sincronizar a apresentação do quadro com a atualização do monitor. |
| **Winding** | Sentido (horário/anti-horário) em que os vértices de um triângulo são listados. |
| **Wrap** | Reposicionar por envolvimento: quem sai por um lado entra pelo outro. |
| **Z-buffer** | Memória de profundidade por pixel. Este projeto não usa. |

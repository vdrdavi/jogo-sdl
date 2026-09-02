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
| **dB por oitava** | Quanto a amplitude cai cada vez que a frequência dobra. |
| **Delta time** | Tempo real decorrido entre dois quadros. |
| **Determinístico** | Mesma entrada, mesmo resultado, sempre. |
| **Driver dummy / disk** | Backends do SDL que fingem vídeo/áudio ou gravam o áudio em arquivo. |
| **Envelope** | Curva que multiplica a amplitude de um som ao longo do tempo (ataque, decaimento). |
| **Esfera de colisão** | Esfera que substitui a geometria real nos testes de colisão. |
| **Espiral da morte** | Quando processar o atraso gera mais atraso do que o quadro consegue absorver. |
| **Filtro IIR de um polo** | Filtro cuja saída depende da saída anterior; aqui, um passa-baixa de -6 dB/oitava. |
| **FOV** | Abertura angular da câmera. Maior FOV = mais "grande angular". |
| **Fonte bitmap** | Fonte já rasterizada como atlas de glifos, em vez de curvas (TrueType). |
| **Ganho** | Multiplicador de amplitude de um som. |
| **Icosaedro** | Poliedro de 20 faces triangulares e 12 vértices. |
| **Integrador com vazamento** | Soma acumulada que esquece o passado aos poucos, e por isso não passeia sem limite. |
| **Letterbox** | Barras pretas quando a proporção da janela não bate com a do jogo. |
| **Livre caminho médio** | Distância média percorrida entre duas colisões: `1/(densidade × seção de choque)`. |
| **Malha (mesh)** | Lista de vértices mais a lista de triângulos que os indexam. |
| **Mixar** | Somar várias fontes de áudio em uma saída. |
| **Névoa (fog)** | Misturar a cor das faces com a cor do fundo conforme a distância. |
| **Normal** | Vetor perpendicular a uma face; diz para que lado ela aponta. |
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
| **Sutherland–Hodgman** | Algoritmo de recorte de polígono contra um plano. |
| **Teste de fumaça** | Verificação mínima de que o programa sobe e roda. |
| **Tile** | Peça quadrada com que um cenário 2D é montado. |
| **Vsync** | Sincronizar a apresentação do quadro com a atualização do monitor. |
| **Winding** | Sentido (horário/anti-horário) em que os vértices de um triângulo são listados. |
| **Wrap** | Reposicionar por envolvimento: quem sai por um lado entra pelo outro. |
| **Z-buffer** | Memória de profundidade por pixel. Este projeto não usa. |

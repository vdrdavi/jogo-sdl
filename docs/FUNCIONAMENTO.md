# Como o jogo funciona por dentro

## O que é este documento

Um passeio pelo programa inteiro, peça por peça: o que cada parte faz, por que
ela existe e como conversa com as vizinhas.

O público é quem chegou agora. Assume-se que você já programou alguma coisa e
sabe ler C++ sem susto, mas **não** que já tenha escrito um jogo, um
rasterizador 3D ou um sistema de áudio. Todo termo técnico é explicado onde
aparece pela primeira vez e repetido no [Glossário](#glossário) no fim.

Os outros documentos do repositório respondem outras perguntas:

| Documento | Responde |
|---|---|
| [`README.md`](../README.md) | o que o jogo é, como compilar e quais são os controles |
| [`docs/ARQUITETURA.md`](ARQUITETURA.md) | onde cada coisa mora e como fazer alterações comuns |
| [`CLAUDE.md`](../CLAUDE.md) | os avisos para quem for mexer no código |
| **este** | **como cada peça do programa funciona, e por quê** |

Leia na ordem em que está: as seções 1 a 8 montam o vocabulário (laço,
coordenadas, cenas, entrada) e as seguintes usam esse vocabulário para explicar
o desenho 2D, o 3D, o voo, as telas e o som. Cada seção aponta os arquivos de
que fala, então dá para ler com o código do lado.

Um aviso sobre números: as constantes citadas aqui (velocidades, raios, tempos)
são as que estão no código hoje e servem para dar escala ao que se explica.
Quando divergirem, o código é quem tem razão.

---

## 1. O programa em uma página

O `main` cabe em oito linhas ([`src/main.cpp`](../src/main.cpp)): cria um `App`,
manda `iniciar` e entrega a primeira cena para `rodar`.

```cpp
jogo::App app;
if (!app.iniciar("Jogo SDL3")) { return 1; }
app.rodar(std::make_unique<jogo::MenuScene>());
```

Tudo o mais é consequência disso. O `App`
([`src/core/App.cpp`](../src/core/App.cpp)) abre a janela, liga os subsistemas e
entra em um laço que só sai quando o jogo pede para sair.

```
App ──┬── janela, renderer
      │
      ├── Input        ─┐
      ├── Assets        │
      ├── Audio         ├─►  Context  ─►  entregue a cada chamada de cena
      ├── BitmapFont    │   (referências)
      └── SceneStack   ─┘
           │
           └── pilha:  MenuScene ▸ InteriorScene ▸ FlightScene ▸ PauseScene
                       (a de cima é a ativa)
```

**Não há estado global.** Quem é dono de janela, renderer e subsistemas é o
`App`, e as cenas recebem um `Context`
([`src/core/Context.hpp`](../src/core/Context.hpp)) — uma struct com
*referências* para o que existe, criada na hora da chamada. Isso é uma escolha,
e a razão dela é que um singleton global esconde as dependências: com o
`Context`, a assinatura de `atualizar(Context&, float)` já diz tudo a que a cena
tem acesso, e nada pode usar o áudio "por debaixo do pano".

### A vida de um quadro

Um **quadro** (*frame*) é uma volta do laço. É o esqueleto do programa inteiro,
e vale decorar — está em `App::rodar()`:

1. **medir o tempo real** que passou desde a volta anterior (`relogio_.novoQuadro()`);
2. **ler a entrada**: teclado, mouse e gamepads viram um retrato do momento
   (`input_.novoQuadro`), e a fila de eventos do SDL é esvaziada
   (`processarEventos`);
3. **simular**, zero ou mais vezes, em fatias fixas de 1/60 s
   (`while (relogio_.consumirPasso()) cenas_.atualizar(...)`);
4. **manter o áudio**: recolher vozes que acabaram, reabastecer os loops
   (`audio_.atualizar()`);
5. **desenhar**: limpar a tela, pedir às cenas que se desenhem, apresentar
   (`SDL_RenderPresent`);
6. **aplicar as trocas de cena** que foram pedidas durante o passo 3
   (`cenas_.aplicarPendentes`);
7. **tarefas de fundo**: gravar as preferências se houver mudança esperando,
   atualizar o "FPS" no título da janela.

Repare que ler entrada, simular e desenhar são três coisas separadas, nessa
ordem, e que **a troca de cena não acontece no meio de nada** — ela espera o fim
do quadro. As seções 2 e 5 explicam por quê.

---

## 2. O laço principal e o passo fixo

Arquivos: [`src/core/Time.hpp`](../src/core/Time.hpp),
[`src/core/App.cpp`](../src/core/App.cpp).

### O problema

O tempo entre dois quadros — o **delta time**, ou `dt` — varia o tempo todo: a
máquina engasga, outra janela rouba a CPU, o monitor é de 60 Hz ou de 240 Hz. A
maneira ingênua de mover alguma coisa é usar esse `dt` cru:

```cpp
posicao += velocidade * dt;   // ingênuo
```

Isso parece certo, e para movimento em linha reta até é. Mas basta a física
ficar um pouco menos trivial (aceleração, atrito, colisão) para o resultado
depender do tamanho do passo, e aí o jogo se comporta diferente em cada máquina.
Pior: se o programa travar meio segundo, o quadro seguinte terá `dt = 0,5` e o
jogador **atravessa a parede num salto só**, porque a colisão nunca chegou a ver
a posição intermediária.

### A solução: fatias sempre iguais

O **passo fixo** (*fixed timestep*) separa o relógio da simulação do relógio da
tela. `StepTimer` guarda um **acumulador**:

- a cada quadro, o tempo real medido entra no acumulador;
- enquanto houver 1/60 s guardado, roda-se um passo de simulação de exatamente
  1/60 s e desconta-se essa fatia;
- o que sobra (sempre menos de um passo) é o **alpha**, uma fração em [0, 1).

```
tempo real:  |------ 16,7 ms ------|-- 4 ms --|--------- 33 ms ---------|
acumulador:  [enche][passo][sobra] [enche]    [enche][passo][passo][sobra]
simulação:          1 passo          0 passos          2 passos
```

A simulação, então, só enxerga fatias idênticas. Ela fica **determinística**:
mesma entrada, mesmo resultado, em qualquer máquina. Todo `atualizar(ctx, dt)`
neste projeto recebe o mesmo `dt` (1/60 s) — o parâmetro existe para deixar as
fórmulas legíveis, não porque ele varie.

### O alpha e a interpolação

Se a simulação roda a 60 Hz e a tela a 240 Hz, três de cada quatro quadros
desenham um mundo que não mudou — e o movimento fica em degraus. A saída é cada
cena guardar **o estado anterior e o atual** e desenhar a mistura entre os dois,
ponderada pelo alpha:

```cpp
// InteriorScene::desenhar
const SDL_FPoint desenhada = interpolar(posicaoAnterior_, posicao_, alpha);
```

**Interpolar** é isto: dado que o passo atual ainda está 40% no futuro, desenhe
o personagem 40% do caminho entre onde ele estava e onde ele está. O
`Flight::interpolada` faz o mesmo com a pose da nave. Por isso `desenhar` recebe
`alpha` e `atualizar` recebe `dt`: são funções de naturezas diferentes, e
misturá-las (mover algo dentro do `desenhar`) quebra o determinismo.

### O teto de 0,25 s

`kMaximoPorQuadro` corta qualquer delta maior que 0,25 s antes de somá-lo ao
acumulador. Sem esse teto, um travamento de cinco segundos mandaria rodar 300
passos de uma vez; isso demoraria, o que acumularia mais tempo, o que mandaria
rodar ainda mais passos. É a **espiral da morte** (*spiral of death*): o jogo
nunca mais alcança o relógio. Com o teto, o mundo simplesmente "perde" o tempo
que passou congelado, que é o mal menor.

### O acoplamento com a entrada

Uma linha do laço não é óbvia:

```cpp
if (simulou) { input_.marcarConsumido(); }
```

Entrada de **borda** (*edge*) é "a tecla **acabou de** ser pressionada", em
oposição a "a tecla **está** pressionada". Ela é detectada comparando o estado
deste quadro com o do quadro anterior — e por isso alguém precisa decidir quando
arquivar o estado anterior. Se fosse a cada quadro, com a tela a 240 Hz e a
simulação a 60 Hz, uma tecla apertada e solta dentro de 4 ms cairia num quadro
que não rodou `atualizar()` nenhum e **sumiria sem nunca ter sido lida**.

Então quem arquiva é o `Input`, e só depois que a simulação avisou que já leu
(`marcarConsumido`). Se você mexer no laço, preserve essa relação.

---

## 3. Coordenadas: 640×360 e o letterbox

Todo o jogo é desenhado em uma resolução fixa de **640×360** — a *resolução
lógica* — e o SDL escala para o tamanho real da janela:

```cpp
SDL_SetRenderLogicalPresentation(renderer, 640, 360, SDL_LOGICAL_PRESENTATION_LETTERBOX);
```

Vantagens: nenhum código de jogo precisa saber o tamanho da janela; a arte em
pixel escala por números inteiros (a janela padrão, 1280×720, é exatamente 2×); e
tela cheia não muda nada no desenho.

Quando a proporção da janela não bate com 16:9, o SDL centraliza a imagem e
sobram barras pretas — é o **letterbox**. Como o mouse chega em pixels de janela,
o `App` converte cada evento antes de repassá-lo:

```cpp
SDL_ConvertEventToRenderCoordinates(renderer_.get(), &evento);
```

A regra prática: **nunca escreva em pixels de janela**. Se um número seu depende
de 1280 ou de 720, ele está errado.

---

## 4. Quem é dono do quê

Arquivos: [`src/core/App.hpp`](../src/core/App.hpp),
[`src/core/SdlPtr.hpp`](../src/core/SdlPtr.hpp),
[`src/core/Context.hpp`](../src/core/Context.hpp).

O SDL é uma biblioteca C: cada `SDL_CreateX` tem um `SDL_DestroyX` que alguém
precisa lembrar de chamar, inclusive nos caminhos de erro. `SdlPtr.hpp` resolve
isso de uma vez, com um `unique_ptr` que já sabe destruir cada tipo:

```cpp
using TexturePtr = std::unique_ptr<SDL_Texture, SdlDeleter<SDL_DestroyTexture>>;
```

Um `unique_ptr` destrói o que aponta quando sai de escopo. Isso é **RAII**
(*resource acquisition is initialization*): o tempo de vida do recurso é o tempo
de vida do objeto, e sair da função por um `return` antecipado ou por uma exceção
não vaza nada. Por isso não existe `delete` neste projeto.

O `App` é dono da janela, do renderer, do `Input`, do `Assets`, do `Audio`, da
`BitmapFont`, da `SceneStack` e do `StepTimer`. As cenas não são donas de nada
disso — recebem o `Context` com referências e o usam durante a chamada. Como o
`Context` é montado na hora (`App::contexto()`), ele nunca fica desatualizado.

O `Log.hpp` é o menor arquivo do projeto e existe por um motivo específico:
`JOGO_ERRO_SDL("SDL_CreateWindow")` já anexa o `SDL_GetError()`, que é a única
forma de saber **por que** uma chamada do SDL falhou.

---

## 5. A pilha de cenas

Arquivos: [`src/scene/Scene.hpp`](../src/scene/Scene.hpp),
[`src/scene/SceneStack.cpp`](../src/scene/SceneStack.cpp).

Uma **cena** é uma tela do jogo: o menu, o interior da nave, a cabine, a pausa.
`Scene` é a interface que todas implementam:

| Método | Quando é chamado |
|---|---|
| `aoEntrar` | ao ser empilhada |
| `aoSair` | ao ser desempilhada |
| `aoRetomar` | quando a cena **de cima** saiu e esta voltou a ser o topo |
| `aoEvento` | para cada evento do SDL (só o topo recebe) |
| `atualizar(ctx, dt)` | uma vez por passo fixo |
| `desenhar(ctx, alpha)` | uma vez por quadro |

### Por que uma pilha, e não uma variável "cena atual"

Porque telas se empilham de verdade: a pausa aparece **por cima** da partida,
que continua existindo atrás dela e volta intacta quando a pausa sai. Duas
perguntas decidem o que acontece com as cenas de baixo:

- `bloqueiaUpdate()` — se `false`, a cena de baixo continua **simulando**;
- `bloqueiaRender()` — se `false`, a cena de baixo continua **aparecendo**.

A `PauseScene` responde `true` para update e `false` para render: congela a
partida sem escondê-la. A `FlightScene` responde `true` para os dois: a cabine
cobre o interior por inteiro. A `StatusScene` — o diagnóstico do casco — responde
como a pausa, mas com uma diferença que a seção 12 explica: bloqueando o update
do convés, ela **herda a obrigação de dar o passo do voo**, e o faz.

`SceneStack::atualizar` desce do topo até encontrar a primeira cena que bloqueia
o update; `desenhar` faz o contrário — procura a cena mais alta que preenche a
tela e desenha dali para cima, para não gastar tempo com o que está coberto.

### Por que as transições são adiadas

`empilhar`, `desempilhar` e `substituir` **não** mexem na pilha na hora: elas
enfileiram um comando, e o `App` aplica tudo no fim do quadro.

O motivo é concreto. A `InteriorScene` empilha a `FlightScene` de dentro do
próprio `atualizar()`. Se a pilha mudasse ali, o `for` que a `SceneStack` está
percorrendo passaria a iterar um vetor que acabou de crescer (ponteiros
invalidados), e um `substituir` chegaria a destruir o objeto no meio da execução
de um método dele. Adiando, nada disso acontece — e uma cena pode se trocar por
outra com segurança.

`aplicarPendentes` move a lista de comandos antes de processá-la, porque um
`aoEntrar` pode enfileirar novos comandos; esses ficam para o quadro seguinte.

### O gancho `aoRetomar`

`aoEntrar`/`aoSair` contam metade da história: uma cena sabe quando saiu de cena,
mas não sabe quando **voltou** a ser o topo. `aoRetomar` fecha esse par, e é dele
que a `InteriorScene` reabre a cortina quando a cabine se fecha (seção 13).

---

## 6. A entrada

Arquivos: [`src/input/Input.hpp`](../src/input/Input.hpp),
[`src/input/Input.cpp`](../src/input/Input.cpp).

### Ações, não teclas

As cenas nunca perguntam "a tecla E está apertada?". Elas perguntam por uma
**ação**:

```cpp
if (ctx.input.acaoPressionada(Acao::Interagir)) { /* ... */ }
```

Um **scancode** é o código físico da tecla, independente do layout do teclado (o
scancode de "E" é o mesmo em ABNT2 e em Dvorak, ainda que a letra impressa mude).
Se as cenas falassem em scancodes, remapear um comando exigiria caçar o scancode
espalhado pelo código, e o gamepad precisaria de um segundo `if` em cada lugar.

O mapeamento mora em uma tabela única no topo de `Input.cpp`, na mesma ordem do
enum `Acao`:

```cpp
// Interagir
{{{SDL_SCANCODE_E, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN}},
 {{SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_INVALID}}},
```

Cada ação aceita até três teclas e dois botões; as posições não usadas ficam em
`UNKNOWN`/`INVALID`, que as consultas ignoram — assim um vínculo pode ser
removido sem abrir buraco na tabela. **Acrescentar um comando ao jogo é
acrescentar um valor ao enum, um nome na tabela `kNomes` e uma linha em
`kPadrao`.** Foi só isso que custou o `Q` do diagnóstico do casco
(`Acao::Diagnostico`, seção 13) — nenhuma cena precisou saber que tecla é essa.
Como o nome da ação é a chave no arquivo de preferências, a ação nova entra
**no fim do enum**: os vínculos já gravados continuam valendo.

Essa tabela é o **ponto de partida**, não a palavra final: o mapa em vigor é
estado do `Input` (`mapa_`), e é justamente por isso que a configuração pode
reescrevê-lo com o que estiver no arquivo de preferências (seção 8).

### Três perguntas diferentes

```cpp
input.acaoAtiva(Acao::Confirmar);       // está pressionada agora?  (turbo)
input.acaoPressionada(Acao::Interagir); // acabou de ser pressionada? (abrir painel)
input.acaoSolta(Acao::Confirmar);       // acabou de ser solta?
```

As duas últimas são as consultas de **borda**, e funcionam porque o `Input`
guarda dois retratos: `teclas_` (agora) e `teclasAntes_` (o retrato anterior).
"Acabou de ser pressionada" é `teclas_[i] && !teclasAntes_[i]`. Quando o retrato
anterior é atualizado é o assunto da seção 2 — é o detalhe que faz nenhuma tecla
se perder em quadros que não simularam.

### Teclado e gamepad na mesma direção

`eixoMovimento()` devolve um vetor em [-1, 1] combinando as quatro ações
direcionais com o analógico esquerdo. Três detalhes valem nota:

- **Zona morta** de 25%: analógico parado raramente lê zero exato, e sem esse
  limiar o personagem andaria sozinho. O valor é *reescalado* depois de
  descontar a zona morta, para o movimento começar do zero e não dar um pulo de
  25% assim que o limiar é cruzado.
- **O analógico tem precedência** sobre o teclado quando está fora da zona morta.
- **A diagonal é normalizada**: sem isso, andar na diagonal seria √2 ≈ 1,41 vez
  mais rápido que andar reto, um bug clássico de jogo 2D.

O gamepad é lido por *polling* (`SDL_GetGamepadButton` a cada quadro), mas
conectar e desconectar chega por evento — daí `Input::onEvent` tratar o
**hotplug** e abrir/fechar o `SDL_Gamepad` correspondente. Com mais de um
controle ligado, vale o eixo de maior deflexão.

---

## 7. Onde ficam os arquivos

Arquivos: [`src/core/Paths.cpp`](../src/core/Paths.cpp),
[`src/gfx/Assets.cpp`](../src/gfx/Assets.cpp).

**Asset** é qualquer arquivo de conteúdo: imagem, som, fonte, mapa. Todos vivem
em `assets/` e são pedidos por caminho relativo:

```cpp
SDL_Texture* tex = ctx.assets.textura("textures/player.png");
```

`paths::assetsRoot()` decide a raiz **uma vez só** (é um `static` local, então a
busca acontece na primeira chamada) e nesta ordem:

1. `assets/` ao lado do executável, via `SDL_GetBasePath()` — é o caso do build e
   da instalação;
2. o `JOGO_ASSETS_DIR` que o CMake grava com o caminho das fontes — a rede de
   segurança de quem roda o binário de outro lugar.

Há um segundo diretório, e a distinção importa: `paths::prefRoot()`
(`SDL_GetPrefPath`) é o **diretório de preferências** do usuário —
`~/.local/share/jogo-sdl/jogo/` no Linux, `%APPDATA%` no Windows,
`~/Library/Application Support` no macOS. É o **único lugar onde o jogo
escreve**, porque o diretório de assets pode ser somente leitura (uma instalação
em `/usr`) e não acompanha o usuário.

`Assets` é um **cache**: um `unordered_map` de caminho para textura. Pedir a
mesma imagem duas vezes não recarrega nada, e as texturas vivem enquanto o
`Assets` viver — por isso as cenas podem guardar tranquilamente os `SDL_Texture*`
crus que ele devolve. Toda textura nasce com `SDL_SCALEMODE_NEAREST`: em arte de
pixel, a filtragem linear borra a imagem ao ampliar.

### Atlas

Quase nenhuma textura do jogo é uma imagem só. Um **atlas** é uma imagem com
vários desenhos lado a lado, da qual se desenha um **recorte** por vez:
`assets/textures/interior.png` traz os seis **tiles** do convés em fila, e
`player.png` traz os quadros da animação em grade. Menos arquivos para
gerenciar e menos trocas de textura na placa de vídeo — trocar de textura é uma
das operações caras de um renderizador.

---

## 8. As preferências

Arquivos: [`src/core/Config.cpp`](../src/core/Config.cpp),
[`src/core/App.cpp`](../src/core/App.cpp).

Volume, tela cheia e vínculos de controle sobrevivem ao fechamento do jogo, em um
`config.ini` dentro do diretório de preferências.

### A decisão central: não guardar cópia dos valores

`config::carregar` e `config::salvar` são só duas funções livres. Não existe uma
classe `Config` com `volume_`, `telaCheia_` e um mapa de teclas — e isso é
deliberado. Cada preferência **já tem um dono**: o volume é do `Audio`, a tela
cheia é da janela (`SDL_GetWindowFlags`), os vínculos são do `Input`. Duplicá-los
criaria dois estados para manter sincronizados, e a pergunta "quem está certo?"
não teria resposta boa. Então o módulo só leva os valores dos donos para o disco
e de volta:

```cpp
bool carregar(SDL_Window* janela, Audio& audio, Input& input);
bool salvar(SDL_Window* janela, const Audio& audio, const Input& input);
```

### O formato

Texto simples, `chave=valor`, para poder ser editado à mão — hoje é o único jeito
de remapear um controle:

```ini
volume=0.6
tela-cheia=0

tecla.interagir.1=E
botao.interagir.1=x
```

A chave de um vínculo é `tecla.<ação>.<número>`, com os nomes que o próprio SDL
dá às teclas e botões (`SDL_GetScancodeName`,
`SDL_GetGamepadStringForButton`) — assim ninguém precisa saber o que é um
scancode para editar o arquivo. Uma regra sutil: **citar uma ação no arquivo
substitui todos os vínculos de fábrica dela**. Sem isso, apagar à mão a linha de
uma tecla não teria efeito nenhum, porque o padrão voltaria por baixo.

Nada disso é fatal: chave desconhecida ou valor torto viram log, e o que não foi
lido fica como estava. O arquivo é escrito já na primeira execução, mesmo com
tudo no padrão — um arquivo que só aparece depois de mexer no volume é um arquivo
que ninguém encontra.

### Quando gravar

Nem a cada mudança, nem só ao fechar. Quem muda uma preferência chama
`marcarConfigSuja()`, que arma uma **espera** (*debounce*) de 0,6 s; o laço
desconta essa espera e grava quando ela zera. Segurar a seta no volume muda o
valor 60 vezes por segundo, e reescrever o arquivo 60 vezes para gravar só o
último valor é trabalho jogado fora. O destrutor do `App` grava o que ainda
estiver esperando — fechar o jogo não pode ser o jeito de perder uma mudança.

Um detalhe de coreografia: quem marca a tela cheia como suja **não** é
`alternarTelaCheia()`, e sim o evento `SDL_EVENT_WINDOW_ENTER_FULLSCREEN`. O
pedido pode demorar, pode ser recusado, e o gerenciador de janelas pode trocar o
modo por conta própria; o que vale é o que aconteceu, não o que foi pedido.

---

## 9. Desenhar em 2D

Arquivos: [`src/gfx/`](../src/gfx/).

### Sprite e Draw

`Sprite` é só **descrição**: qual textura, que recorte do atlas, que tamanho em
unidades de mundo, onde fica a **âncora** (o ponto do desenho que cai sobre a
posição dada: `{0.5, 0.5}` é o centro, `{0.5, 0.72}` põe os pés do personagem no
chão), rotação, espelhamento e **tinta** (uma cor multiplicada sobre a imagem).
Ele não sabe desenhar a si mesmo.

Quem desenha é `draw::`, e em duas variedades que não devem ser confundidas:

- `draw::sprite(...)` recebe uma **posição de mundo** e passa pela câmera;
- `draw::spriteTela(...)` recebe uma **posição de tela** e ignora a câmera — é o
  que HUD e menus usam.

### Camera

A `Camera` converte entre os dois espaços:

```cpp
tela = (mundo - posicao) * zoom + viewport/2
```

Três comportamentos merecem explicação.

**`seguir(alvo, dt)`** usa suavização exponencial, não uma interpolação linear
crua:

```cpp
const float fator = 1.0f - std::exp(-suavidade * dt);
posicao_ += (alvo - posicao_) * fator;
```

A forma ingênua (`posicao += (alvo - posicao) * 0.1f`) tem uma armadilha: ela
depende de quantas vezes por segundo é chamada, e por isso a câmera ficaria mais
rápida em máquinas mais velozes. `1 - e^(-k·dt)` dá a mesma resposta física
qualquer que seja o `dt`. (Aqui o passo é fixo de qualquer forma, mas a fórmula
está certa por construção — e a mesma aparece no `Flight` e na `FlightScene`.)

Uma consequência que reaparece na seção 13: perseguir a taxa `k` um alvo que
corre à velocidade `v` deixa a câmera estabilizada `v/k` atrás dele. Isso é o
**regime permanente** do sistema, e a `FlightScene` usa esse número para nascer
já na posição certa.

**`limitarA(limites)`** impede a câmera de mostrar fora do mundo. Quando o mundo
é menor que a área visível naquele eixo, ela centraliza em vez de grampear — e
isso descreve o convés: 20×13 tiles de 16 unidades dão 320×208; com zoom 2 a
área visível é 320×180. Horizontalmente cabe exatamente, então o convés não rola
para os lados; verticalmente sobram 28 unidades, e aí sim a câmera acompanha.

**`areaVisivel()`** devolve o retângulo do mundo que está na tela. É o que
permite à `InteriorScene` desenhar apenas os tiles visíveis (**culling**:
descartar cedo o que não aparece) em vez dos 260 do mapa.

### Animacao

A separação aqui é a lição:

- `Clipe` é **descrição** — que linha do atlas, quantos quadros, a que ritmo;
- `Animacao` é **estado** — que quadro está no ar e há quanto tempo;
- `Sprite` continua sem saber o que é uma animação, porque tudo o que a
  `Animacao` devolve é um `recorte()` que ele já sabia desenhar.

Duas decisões dentro de `Animacao`:

`tocar(clipe)` **só reinicia se a linha for outra**. As cenas chamam isso a cada
passo com o clipe do estado atual (`andando ? kAndando : kParado`); se reiniciar
sempre, o personagem ficaria travado no quadro 0 enquanto anda.

`atualizar(dt)` desconta a duração em um `while` em vez de zerar o acumulador:

```cpp
tempo_ += dt;
while (tempo_ >= duracao) { tempo_ -= duracao; quadro_ = (quadro_ + 1) % clipe_.quadros; }
```

O resto de um quadro conta para o próximo. Sem isso, um clipe mais rápido que o
passo fixo (mais de 60 quadros por segundo) andaria devagar, e a cadência
dependeria do `dt`.

E `Animacao::atualizar` é chamada do `atualizar` da cena, nunca do `desenhar`:
animação é estado da simulação, e no `desenhar` ela avançaria a 240 Hz.

### BitmapFont

Uma **fonte bitmap** já vem rasterizada como um atlas de glifos de tamanho fixo,
em vez de descrita por curvas (**TrueType**) que alguém precisa transformar em
pixels. Isso evita a dependência de `SDL3_ttf` e combina com a arte em pixel.

São dois arquivos: `assets/fonts/mono.png` (o atlas) e `mono.fnt` (os
metadados):

```
cell 8 16
cols 16
chars !"#$%&'()*+,-./0123456789:;<=>?@ABC...ÀÁÂÃÇÉÊÍÓÔÕÚ...→←↑↓
```

**A ordem do charset é o índice no atlas**: o 5º caractere da linha `chars` é a
5ª célula da grade. `carregar` monta um mapa de codepoint para índice; `desenhar`
decodifica o texto em UTF-8 (byte inválido vira `?`, para texto malformado não
quebrar o desenho) e emite uma célula por caractere. O espaço fica fora do atlas
e é tratado como avanço em branco.

Uma regra de uso: **posicione texto com `medir()` e `alturaLinha()`**, nunca com
constantes. A célula já foi 11×18 e hoje é 8×16; qualquer número cravado quebra
quando a fonte muda.

---

## 10. O convés: mapa de tiles e colisão

Arquivos: [`src/world/MapaDeTiles.cpp`](../src/world/MapaDeTiles.cpp),
[`assets/maps/conves.mapa`](../assets/maps/conves.mapa),
[`src/scenes/InteriorScene.cpp`](../src/scenes/InteriorScene.cpp).

Um **tile** é uma peça quadrada com que um cenário 2D é montado — aqui, 16×16
unidades de mundo, o mesmo tamanho das células do atlas `interior.png`. O
cenário é uma grade de índices desse atlas.

### O cenário vem de um arquivo

O convés é lido de `assets/maps/conves.mapa`, um texto com três diretivas
(**diretiva**: uma linha `chave valor` lida como um comando por quem carrega o
arquivo):

```
legenda . piso            liga um caractere da grade a um tile
marcador console 8.5 2    ponto nomeado, em tiles (aceita fração)
mapa                      abre a grade: o resto do arquivo é ela
```

```
####################
#-ooooo------ooooo-#
#.....,..,......,..#
...
```

Editar o cenário não exige recompilar, e cabe mais de um ambiente no jogo. O
marcador aceita fração porque o painel tem 3 tiles de largura e o convés tem 20:
`8.5` é o que centraliza o console de verdade.

**O que ficou de fora do arquivo, de propósito**: quais nomes de tile existem,
qual é o índice de cada um no atlas e quais são sólidos. Isso vive na tabela
`kDefinicoes` no topo de `MapaDeTiles.cpp`, porque descreve a **textura e a
colisão**, não o cenário. Acrescentar um tile é acrescentar um recorte ao atlas e
uma linha nessa tabela.

Um tropeço registrado no próprio código: **comentário é a linha inteira que
começa com `#`, nunca um rabicho no fim da linha**. Não dá para descartar tudo
depois de um `#`, porque `#` é também o caractere da parede na grade — e é um
símbolo legítimo de legenda (`legenda # parede`).

Qualquer falha (arquivo ausente, linha da grade com comprimento diferente,
caractere fora da legenda) é registrada no log e substituída por uma **sala de
emergência**: um piso cercado de parede. Um cenário quebrado degrada o jogo, não
o derruba. Fora da grade, `tile()` grampeia a busca, então a borda se repete em
vez de abrir um vazio preto quando a câmera é empurrada para fora.

### A colisão, um eixo por vez

O jogador é um retângulo de 20×12 unidades — a caixa fica nos **pés**, não no
corpo inteiro, que é o que dá a sensação de profundidade em vista de cima.
`moverComColisao` tenta o deslocamento em X e depois em Y, separadamente:

```cpp
tentar(deslocamento.x, 0.0f);
tentar(0.0f, deslocamento.y);
```

Cada tentativa converte a caixa candidata na faixa de tiles que ela cobre e
recusa o movimento se qualquer um for sólido. Testar os dois eixos de uma vez
faria o jogador **travar** ao encostar em uma parede na diagonal; um eixo por vez
faz ele **deslizar** ao longo dela, que é o comportamento que todo mundo espera
sem saber explicar.

---

## 11. O 3D sem shaders

Arquivos: [`src/gfx3d/`](../src/gfx3d/).

Não há OpenGL, Vulkan nem **shaders** (programinhas que rodam na placa de vídeo).
`Renderer3D` é um **rasterizador por software**: ele transforma triângulos 3D em
triângulos 2D usando a CPU e entrega o lote pronto ao SDL em **uma** chamada de
`SDL_RenderGeometry`.

### Vetores e matrizes

`Math3D.hpp` traz o mínimo: `Vec3` com as operações usuais e duas que talvez
sejam novas.

O **produto escalar** `dot(a, b)` mede o quanto dois vetores apontam para o mesmo
lado: positivo se o ângulo é agudo, zero se são perpendiculares, negativo se
opostos. Ele aparece três vezes neste projeto — no descarte de faces, no cálculo
da luz e na colisão.

O **produto vetorial** `cross(a, b)` devolve um vetor **perpendicular** aos dois.
É como se obtém a **normal** de um triângulo: `cross(b - a, c - a)` é
perpendicular ao plano dele, ou seja, aponta para o lado para o qual a face
"olha".

`Mat3` é uma base ortonormal guardada por colunas: direita, cima e "para trás".
Multiplicar leva do espaço local para o pai; como as colunas são ortonormais, a
**transposta é a inversa** (`aplicarTransposta`) — é assim que se vai do mundo
para o espaço da câmera, de graça.

**Convenção de orientação: a frente é -Z.** `Mat3::frente()` devolve
`-colunas[2]`. Malhas novas precisam ter o nariz em -Z.

### Malhas

Uma **malha** (*mesh*) é uma lista de vértices mais uma lista de triângulos que
os indexam por número. Aqui cada face carrega também a própria cor — é o modelo
*low poly* com sombreamento chapado, sem texturas.

A nave é digitada à mão: oito vértices (nariz, pontas das asas, dorso, ventre,
cauda, cabine) e doze faces. O asteroide parte de um **icosaedro** (20 faces
triangulares, 12 vértices — o menor sólido que ainda passa por rocha depois de
amassado) e é deformado por um gerador com semente:

```cpp
v = normalizar(v) * rng.entre(0.62f, 1.10f);   // amassa ao longo do próprio raio
...
v = v * (1.0f / maior);                        // normaliza: o maior raio vira 1
```

Essa normalização não é estética: com raio 1, **a escala com que a rocha é
desenhada já é o raio da esfera de colisão**. Um número só descreve as duas
coisas, e elas nunca podem discordar.

`orientarFacesParaFora()` conserta o **winding** — o sentido (horário ou
anti-horário) em que os vértices de um triângulo são listados, que é o que define
para onde a normal aponta. Ele compara cada normal com a direção do centro da
malha até o centro da face e troca dois índices quando estão invertidos. Assim
quem digita uma malha nova não precisa acertar a ordem dos vértices.

### O caminho de uma face até a tela

`Renderer3D::submeter()` faz, para cada face:

**1. Transformação.** Cada vértice vira posição no mundo:
`posicao + rotacao * (vertice * escala)`.

**2. Descarte de faces traseiras** (*back-face culling*). Se a normal aponta no
mesmo sentido do olhar, aquela é a face de dentro do sólido — invisível.
`dot(normal, a - camera.posicao) >= 0` e a face é descartada. Corta cerca de
metade do trabalho de graça.

**3. Sombreamento flat.** A face inteira recebe uma cor só, proporcional ao
ângulo entre a normal e a direção da luz (**luz difusa**, a lei de Lambert), mais
uma parcela **ambiente** fixa de 30% para as faces na sombra não ficarem pretas.
"Flat" é isso: sem interpolar cor entre vértices.

**4. Névoa** (*fog*). A cor é misturada com a cor do fundo conforme a
profundidade. Isso existe por um motivo prático (seção 11.1): sem ela, uma rocha
reposicionada apareceria inteira na borda do campo. De quebra, a face que já
virou 100% cor de fundo é descartada — desenhá-la não mudaria um pixel.

**5. Recorte no plano próximo** (*near-plane clipping*). O que está atrás da
câmera não pode ser projetado: dividir por uma profundidade negativa espelha a
geometria e produz triângulos absurdos atravessando a tela. O triângulo é cortado
contra um plano logo à frente da câmera (0,15 unidade) — é o algoritmo de
**Sutherland–Hodgman** com um plano só. O corte de um triângulo pode dar um
quadrilátero, daí o **leque de triângulos** (*triangle fan*) que o reconstitui.

**6. Projeção em perspectiva.**

```cpp
tela.x = largura/2 + vista.x * (distanciaFocal / -vista.z);
tela.y = altura/2  - vista.y * (distanciaFocal / -vista.z);
```

Dividir pela profundidade é o que faz o longe parecer pequeno. A **distância
focal** sai do **FOV** (*field of view*, a abertura angular da câmera):
`focal = (altura/2) / tan(fov/2)`. Quanto maior o FOV, mais curta a focal e mais
"grande angular" a imagem — é por isso que abrir o FOV no turbo dá sensação de
velocidade sem mudar velocidade nenhuma.

Finalmente, `desenhar()` resolve a oclusão pelo **algoritmo do pintor**: sem
**z-buffer** (a memória por pixel que guardaria a profundidade do que já foi
desenhado ali), ordena as faces da mais distante para a mais próxima e desenha
nessa ordem, como um pintor cobrindo o fundo. É barato e funciona bem para
sólidos separados; **geometria que se interpenetra ou translúcida ordena
errado**, e essa é a limitação que se aceitou.

### 11.1 Campos infinitos com memória constante

`Starfield` e `AsteroidField` resolvem o mesmo problema: encher um espaço sem fim
sem alocar infinito.

A ideia é um **cubo** de objetos centrado na câmera. Quando a nave anda, cada
objeto que sai por uma face do cubo é reposicionado na face oposta — o **wrap**:

```cpp
float envolver(float distancia, float raio) {   // mantém em [-raio, raio)
    float valor = std::fmod(distancia + raio, raio * 2.0f);
    if (valor < 0.0f) { valor += raio * 2.0f; }
    return valor - raio;
}
```

Memória constante, campo infinito. São 1800 estrelas em um cubo de raio 150 e 200
rochas em um cubo de raio 110.

**O rastro das estrelas** é um truque de perspectiva: a cauda é a projeção da
estrela **deslocada de `+v·Δt`**. Como quem andou foi a câmera, deslocar a
estrela para a frente equivale a mostrar onde ela aparecia um instante atrás. As
estrelas são desenhadas com mistura **aditiva** (`SDL_BLENDMODE_ADD`): elas somam
luz ao vazio em vez de cobrir o que está atrás, que é como luz se comporta.

**O asteroide precisa de mais que wrap.** Wrap puro deixa o campo **periódico**:
voando reto, as mesmas pedras voltam na mesma formação a cada travessia do cubo.
Com estrelas isso passa batido; com rochas, que têm forma, a repetição aparece.
Então quem atravessa a borda é **sorteado de novo** nos eixos que *não* viraram,
e ganha raio, giro e malha novos. A troca acontece a uma aresta inteira de
distância, dentro da névoa — longe dos olhos.

Um detalhe fino de ponto flutuante mora aí: o teste de "virou" é por magnitude
(`fabs(envolvido - relativo) > raio`), não por igualdade. `envolver()` soma e
subtrai `raio`, então nem sempre devolve exatamente o mesmo `float` que entrou;
por igualdade, quase toda rocha seria sorteada de novo a cada quadro.

**A colisão é entre esferas**, o teste mais barato que existe: dois objetos se
tocam quando a distância entre os centros é menor que a soma dos raios. Compara-
se o quadrado das duas grandezas, para não pagar uma raiz quadrada:

```cpp
if (dot(delta, delta) < alcance * alcance) { /* bateu */ }
```

E a rocha atingida não some: ela é mandada para outro canto do cubo. O campo
mantém a mesma densidade sem alocar nem liberar nada.

---

### 11.2 Os destroços: quebrar uma malha sem um segundo modelo

Arquivo: [`src/gfx3d/Destrocos.cpp`](../src/gfx3d/Destrocos.cpp).

Quando a nave se perde, ela **não é trocada por um modelo de "nave quebrada"**.
Cada face da malha vira um caco, e cada caco é um **tetraedro**: os três vértices
da face mais o centro da malha. Isso dá duas coisas de graça.

A primeira é que os cacos, juntos e parados, ainda *são* a nave — a mesma pele,
nas mesmas posições. No instante da destruição nada pisca nem troca de forma: o
que estava lá começa a se afastar. (O caco nasce centrado no próprio centroide,
para girar em torno de si mesmo; o que sobra vira a posição dele no mundo.)

A segunda é que um tetraedro é **fechado**. Um triângulo solto seria descartado
sempre que fosse visto por trás (o culling da seção 11 olha a normal) e piscaria
a cada tombo; um sólido nunca some. As três faces internas ganham a cor da face
original escurecida: o caco tem frente e avesso em vez de parecer papel.

O resto é integração simples, e nenhuma parte disso é simulação: os cacos não
colidem com nada e ninguém depende deles.

- Cada caco sai **para fora do centro da nave**, na direção em que a peça já
  estava — por isso a nave *se abre* em vez de espirrar para um lado só — e
  **herda a velocidade que a nave tinha**, senão a explosão ficaria para trás da
  câmera no primeiro segundo.
- O tombo é recomposto do ângulo a cada quadro e aplicado **por cima** da
  orientação de origem (`base * deEuler(yaw, pitch, 0)`): o mesmo cuidado do
  campo de asteroides, para não acumular erro, mais a garantia de que no primeiro
  quadro o conjunto ainda é a nave inteira.
- As **faíscas** não são malhas. São pontos projetados e desenhados como brilhos
  aditivos (`draw::brilhoAditivo`, o mesmo do escapamento do motor), com a
  direção sorteada por rejeição dentro de um cubo — sortear dois ângulos
  acumularia faísca nos polos — e o brilho caindo com o quadrado do que resta de
  vida, porque o apagar linear parecia desligar de uma vez.

---

## 12. O voo

Arquivo: [`src/sim/Flight.cpp`](../src/sim/Flight.cpp).

**O voo não é uma cena.** `Flight` guarda a pose da nave, a velocidade, o campo
de rochas, a colisão e o ambiente sonoro — tudo o que é *estado da viagem*. Ele
vive na `InteriorScene`, e não na `FlightScene`, porque **a nave continua voando
enquanto o piloto anda lá dentro**. A cabine só pilota e desenha um estado que
não é dela.

`atualizar(ctx, dt, comando)` recebe o comando do piloto:

```cpp
struct Comando { SDL_FPoint eixo{0, 0}; bool turbo{false}; };
```

Quem chama é a cena ativa: a `FlightScene` passa o comando do jogador, a
`InteriorScene` passa `Comando{}`. E aí está o **piloto automático, sem uma linha
de código a mais**: sem comando, o rolamento e a guinada tendem a zero, a
velocidade tende ao cruzeiro e a nave segue reto — inclusive contra uma pedra.

Como a `FlightScene` bloqueia o update da cena de baixo, apenas uma das duas
chama `Flight::atualizar` em cada passo. O voo avança **exatamente um passo por
passo fixo** nos dois casos, e é isso que faz a passagem entre o convés e a
cabine não ter buraco nem repetição. Se mexer nisso, confira que continua assim.

O que acontece em um passo:

- **Manobra.** A guinada (*yaw*) responde direto ao eixo horizontal; o
  **rolamento** (*roll*) persegue `-eixo.x * 0.85` com suavização exponencial,
  que é o que faz a nave *inclinar na curva* como um caça em vez de girar como
  uma torre. A **arfagem** (*pitch*) é grampeada em ±1,15 rad, para a nave não
  dar cambalhota.
- **Velocidade.** Persegue 62 u/s no cruzeiro ou 185 u/s no turbo — não pula
  entre os dois. `fatorTurbo()` devolve onde ela está entre os dois valores, de 0
  a 1, e é a medida de esforço do motor que a HUD, o brilho do escapamento e o
  volume do ambiente usam.
- **Posição.** `posicao += frente() * velocidade * dt`. Tudo o mais é
  consequência da pose.
- **Campo e colisão.** As rochas tombam, o cubo é recentrado na nave e testa-se a
  esfera de raio 2 da nave contra elas.
- **Ambiente sonoro.** O ganho persegue um alvo que depende do turbo e do casco
  (adiante).
- **Alarme.** Com o casco em estado crítico, a sirene e a luz de emergência do
  convés andam mais um ciclo (adiante).

Na batida, a nave quase para (18 u/s), a rocha é reposicionada, um som toca e
`batida_` vai a 1 e decai a 3,4 por segundo. `Flight` não sacode nada: ele expõe
`batida()` e **cada cena sacode do seu jeito** — o convés inteiro treme na
`InteriorScene`, a câmera treme na `FlightScene`, e a `StatusScene` pisca a
palavra do mostrador.

A batida também **cobra o casco**: `casco_` começa em 1, cai 0,125 por rocha e
para no zero — oito batidas do casco inteiro ao nada. É um valor da *viagem*, e
por isso mora aqui e não na tela que o mostra: a nave se estraga batendo com o
piloto no convés tanto quanto na cabine, e o mostrador é só quem lê `casco()`.

### Quando o casco zera

Não há um segundo estado para manter em dia: `destruida()` **é** `casco() <= 0`.
Um `bool` separado seria mais uma coisa que pode discordar do mostrador.

A oitava rocha soa diferente das outras — o estouro é tocado aqui, e não pela
cena, porque o casco pode ceder com o jogador no convés, no painel ou na cabine,
e o som do fim da nave não pode depender de quem estava desenhando. Do passo
seguinte em diante o `Flight` muda de comportamento:

- **não manobra e não acelera.** Só a integração da posição fica fora do `if`:
  sem motor e sem atrito no vazio, o que sobra segue na direção e na velocidade
  em que estava (18 u/s, a velocidade que a batida deixou). É essa deriva que dá
  à câmera o que seguir enquanto os destroços se abrem;
- **não colide.** Sem isso o casco continuaria "batendo" em rochas e tocando
  baques depois de ter deixado de existir;
- **cala o ambiente.** O alvo do ganho vai a zero pela mesma rampa de sempre, e a
  sequência termina em silêncio — que é de onde a tela de fim começa.

`Pose` guarda posição e os três ângulos, e o `Flight` mantém a pose anterior para
`interpolada(alpha)` (seção 2).

### O som que atravessa o casco

O ambiente do lado de fora **toca a viagem inteira**, começando em ganho zero e
subindo — entrar na viagem não estoura um rugido do nada. Enquanto o jogador está
no convés, o casco o abafa: `definirAbafado(true)` multiplica o alvo por 0,34.

A rampa que faz essa mudança acontecer aos poucos fica no `Flight`, não na cena.
Se cada cena aplicasse o próprio ganho ao entrar, a passagem convés → cabine
seria um **degrau** audível. Estando no `Flight`, ela é um *swell*: o casco para
de abafar no instante em que a cortina começa a fechar, e o som abre junto com a
imagem.

### O alarme do casco crítico

Abaixo de 30% de casco (`Flight::kCascoCritico`) a nave passa a avisar: uma
**sirene** toca e a iluminação do convés vai e volta do vermelho. As duas coisas
são, por dentro, a mesma coisa — e é isso que vale registrar. Cada passo,
`Flight::atualizar` calcula **um número só**, `alarme()`, entre 0 e 1:

```cpp
faseAlarme_ = fmod(faseAlarme_ + kTau * kFreqAlarme * dt, kTau);   // ~0,85 ciclo/s
intensidadeAlarme_ = aproximar(intensidadeAlarme_, critico() ? 1 : 0, kTaxaAlarme, dt);
alarme_ = intensidadeAlarme_ * (0.5f - 0.5f * cos(faseAlarme_));
```

A **fase** anda sempre, com o casco inteiro ou não; quem entra e sai é a
**intensidade**, pela mesma suavização exponencial do ambiente. O alarme abre e
fecha em rampa em vez de estalar no meio de um ciclo, e uma nave já perdida se
cala junto com o resto — `critico()` exige `!destruida()`.

Desse número saem as duas manifestações: o ganho da voz da sirene, ali mesmo no
`Flight`, e o alfa do vermelho que a `InteriorScene` pinta sobre o convés. **Um
número só, de propósito.** A ida e a volta da sirene poderiam estar gravadas no
próprio WAV, o que seria até mais simples de gerar; mas aí o som andaria pelo
relógio do dispositivo de áudio e a luz pelo passo fixo da simulação, que são
dois relógios diferentes, e em poucos minutos de viagem o pisca-pisca estaria
fora do compasso do som — sem como realinhá-los depois. Por isso o WAV é só o
*timbre* (uma fundamental de 620 Hz com a quinta e a oitava por cima, em loop) e
o vaivém é do jogo.

A voz da sirene nasce no `iniciar` e toca a viagem inteira, **calada**, como o
ambiente: o que muda com o casco é o ganho, não a existência da voz. Assim não há
loop nascendo e morrendo no meio do voo — e, como o limite de 16 vozes nunca
engole um *loop* (seção 14), a nave não fica sem o aviso justo quando ele
importa.

A fronteira dos 30% é uma só: quem escreve `CRITICO` no diagnóstico (`faixaDo`,
em `StatusScene.cpp`) lê a mesma `Flight::kCascoCritico`. A sirene não pode estar
tocando sobre um mostrador que ainda diz `AVARIADO`.

---

## 13. As cenas, uma a uma

Arquivos: [`src/scenes/`](../src/scenes/).

### MenuScene

A tela inicial: navegação por teclado ou gamepad, som nas transições, e as
preferências (volume e tela cheia) ajustadas com esquerda/direita sobre a opção
selecionada.

Dois detalhes que parecem miudezas e não são. O rótulo da opção é montado na hora
(`"Volume: " + ...`), porque ele mostra o valor de uma preferência que muda. E o
*blip* de confirmação toca **depois** de o volume mudar, e por isso já sai no
volume novo: o som vira a própria prévia do ajuste.

### InteriorScene

O convés. É a cena mais cheia do jogo, e vale ler o `atualizar()` de cima para
baixo, porque a ordem das coisas ali é toda deliberada:

```cpp
voo_.atualizar(ctx, dt, Flight::Comando{});    // sempre, antes de qualquer saída
if (transicao_.ativa()) { ... return; }        // cortina em cena: ninguém comanda nada
if (voo_.destruida()) { ... return; }          // entrega a vista externa
if (acaoPressionada(Pausar)) { ... return; }   // empilha a pausa
if (pertoDoConsole() && acaoPressionada(Diagnostico)) { ... return; } // empilha o casco
if (pertoDoConsole() && acaoPressionada(Interagir)) { ... return; }   // fecha a cortina
moverComColisao(...);                          // só aqui o jogador anda
camera_.seguir(posicao_, dt, 7.0f);
```

O passo do voo vem **antes das saídas antecipadas**: qualquer `return` acima dele
faria a viagem perder passos. E enquanto a cortina está em cena, nenhuma tecla é
lida — um segundo `E` empilharia uma segunda cabine.

A checagem do casco vem logo depois dele, antes de tudo o mais — inclusive de uma
cortina que já estivesse fechando. Não há o que fazer no convés de uma nave que
acabou de se abrir: a `FlightScene` é empilhada **sem cortina** (o casco não se
rompe com educação) e é ela que mostra o que aconteceu. Uma trava
(`entregouADestruicao_`) guarda que isso já foi pedido: como a pilha só aplica os
comandos no fim do quadro, dois passos fixos no mesmo quadro empilhariam duas
cabines.

Também é aqui que o `Flight` nasce (`aoEntrar`) e morre (`aoSair`): a viagem dura
exatamente o tempo desta cena.

No `desenhar`, o tremor da batida entra em uma **cópia** da câmera:

```cpp
Camera camera = camera_;
if (voo_.batida() > 0.0f) { camera.definirPosicao(/* ...seno... */); }
```

Somado em `camera_`, o deslocamento realimentaria o seguidor, que perseguiria o
alvo a partir da posição já sacudida — e o tremor viraria deriva. Em uma cópia,
ele é só imagem.

A **luz de emergência** também é desenho e nada mais: um retângulo vermelho do
tamanho da tela, com o alfa saindo de `voo_.alarme()` (seção 12). A ordem importa
— ele entra **depois da nave e antes da HUD**: a lâmpada é do convés, e tingir o
convite e a barra de dicas só custaria legibilidade justo quando há pressa para
ler. No vale do ciclo o alfa é zero e a iluminação normal volta inteira; não há
dois estados, só o número indo e voltando. O convite é ancorado no console mas desenhado em coordenadas de
tela, com `medir()` dando o tamanho da tarja — e são **duas linhas de mesma
largura**, `[E] Assumir os controles` e `[Q] Diagnostico do casco`, exatamente
para a tarja sair retangular e as duas opções ficarem centralizadas sobre o
painel sem cálculo à parte.

As duas opções do painel saem por caminhos diferentes, e a diferença não é
capricho: o `E` **fecha a cortina** e troca de mundo (a cabine cobre a tela
inteira); o `Q` empilha um painel que é um *overlay* sobre o próprio convés, que
continua visível atrás — não há para onde transicionar, então não há cortina.

### FlightScene

A cabine. Não é dona do voo: guarda uma **referência** para o `Flight` da cena de
baixo. Isso é seguro porque a pilha só desempilha do topo — o interior sempre
sobrevive à cabine.

O que ela acrescenta é a apresentação: a câmera de terceira pessoa, o campo de
estrelas, a névoa, a HUD, o brilho do escapamento e o clarão da batida.

A câmera persegue um ponto atrás e acima da nave (`{0, 1.4, 5}` no espaço dela) e
**olha 14 unidades à frente** da nave, não para a nave: mirar adiante mantém o
alvo estável quando a nave gira. O atraso da perseguição é o que dá peso às
manobras.

O `aoEntrar` tem uma linha que merece o parágrafo que a acompanha no código:

```cpp
camera_ = pose.posicao + rotacao * Vec3{0, 1.4f, 5}
        - rotacao.frente() * (voo_.velocidade() / kPerseguicaoCamera);
```

A nave já estava voando quando a cabine abriu. Se a câmera nascesse na posição
"desejada", ela estaria colada na nave e recuaria sozinha no primeiro meio
segundo, porque o regime permanente da perseguição é ficar `v/k` atrás (seção 9).
Nascer já nessa distância é começar no regime, não caminhar até ele.

O FOV também nasce 12° mais fechado e abre sozinho, pela mesma suavização que já
existia para o turbo: a vista **abre do painel para o espaço** em vez de aparecer
pronta.

#### A sequência de destruição

Esta é também a cena onde a nave acaba, venha o jogador de onde vier. A condição
é o estado do voo (`voo_.destruida()`), e não um aviso que alguém precise mandar:
quem chega aqui já morto — trazido do convés ou do painel do casco — cai na mesma
linha que quem morreu pilotando. Quando isso acontece:

- `comecarDestruicao()` estilhaça a malha da nave (seção 11.2) na pose, rotação e
  escala em que ela estava sendo desenhada, e passa a velocidade dela aos cacos;
- `morrendo_` desliga o resto da cena: nenhum comando é lido (a tecla que voltava
  ao convés não pode voltar para uma nave que não existe), a mira e a HUD saem, o
  escapamento some junto com o motor, e no lugar da nave vão os destroços;
- a câmera **afrouxa a perseguição** (de 9 para 2,2 por segundo): o que ela segue
  agora é a deriva do que sobrou, e o atraso maior abre distância dela;
- passados 1,9 s, a cortina começa a fechar — devagar, 1,4 s, e não os 0,24 s da
  passagem para o convés. Apagar antes disso seria esconder justamente o que há
  para ver;
- no passo em que a tela fica coberta, `substituir` troca esta cena pela
  `GameOverScene`.

Note que a cortina é a mesma de sempre, com **dois destinos**: `desempilhar` no
fim de uma visita à cabine, `substituir` no fim da viagem. E quando esta cena é
aberta já com a nave perdida, ela **não toca a cortina de entrada**: o corte é
seco de propósito — foi o que aconteceu com a nave.

### StatusScene

A outra opção do painel: `Q`, junto ao console, abre o **diagnóstico do casco** —
uma barra, o número em porcentagem e a palavra do estado (`INTEGRO`, `AVARIADO`,
`CRITICO`), sobre o convés escurecido. `Esc` ou `Q` de novo fecham.

Ela repete a escolha da `FlightScene` por um motivo parecido: guarda uma
**referência** para o `Flight` da cena de baixo, bloqueia o update dela e, por
isso, **passa a ser quem chama `Flight::atualizar`** (com `Comando{}`, o piloto
automático). Se ela apenas bloqueasse, a viagem congelaria enquanto o painel
estivesse aberto; chamando, a nave continua voando — e uma rocha derruba o
mostrador na frente de quem está lendo. Continua valendo o invariante da seção
12: **um passo de voo por passo fixo**, seja qual for a cena no topo.

O único enfeite com lógica é o **ponteiro**. `ponteiro_` persegue `casco()` com a
mesma suavização exponencial de sempre, e a barra desenha três coisas: o trilho,
o casco que restou (na cor da faixa) e, entre ele e o ponteiro que ainda está
descendo, **o pedaço que a última rocha levou**, em vermelho. Sem isso a barra
simplesmente teria outro tamanho no quadro seguinte à batida, e ninguém veria o
que foi perdido. O número segue o ponteiro, e não o casco, para os dois nunca se
contradizerem no meio da queda.

Um detalhe de acabamento que é dívida da perseguição exponencial: ela chega perto
e **nunca encosta**, então o ponteiro é encaixado no casco quando a diferença cai
abaixo de 0,001 — senão sobraria para sempre uma lasca de vermelho de menos de um
pixel na ponta da barra.

Se o mostrador chega a zero, não há diagnóstico a fazer: esta cena **se troca**
pela `FlightScene` (`substituir`, não `empilhar`). Trocar é o que deixa a pilha
idêntica à dos outros caminhos até o fim — `MenuScene ▸ InteriorScene ▸
FlightScene` —, e é por isso que a `GameOverScene` pode desempilhar sempre os
mesmos dois degraus para voltar ao menu.

### PauseScene

O exemplo mais limpo do que a pilha de cenas compra: quatro linhas de lógica,
`bloqueiaRender()` devolvendo `false`, e a partida fica congelada e visível atrás
de um véu. `M` desempilha duas vezes — sai da pausa e da partida, voltando ao
menu que ficou na base da pilha.

### GameOverScene

Uma tela e duas teclas. O que ela tem de interessante é a aritmética da pilha:
como os três caminhos até a morte convergem em `MenuScene ▸ InteriorScene ▸
FlightScene`, e ela **substitui** a `FlightScene`, voltar ao menu é sempre
desempilhar dois degraus — esta tela e a partida congelada embaixo dela. Quem
encerra a viagem de fato é o `aoSair` da `InteriorScene`, que apaga o ambiente.

Ela também **ignora as teclas enquanto a cortina ainda está abrindo**: a tecla
que o jogador estava segurando quando a nave se abriu não pode pular o fim.

### A cortina entre o convés e a cabine

Arquivo: [`src/scenes/Transicao.cpp`](../src/scenes/Transicao.cpp).

Passar do convés para a cabine com um corte seco é desagradável: a tela troca de
mundo em um quadro. A `Transicao` é a **cortina** que costura as duas — um
retângulo de cor que cobre a tela e reabre do outro lado.

Ela **não é uma cena**. É estado que cada uma das duas cenas guarda e desenha por
cima do próprio HUD. Duas razões, e as duas vêm da seção 5:

- uma cena de overlay teria que **sobreviver à troca da cena de baixo**, e a
  pilha só empilha e desempilha no topo;
- ocupando o topo, ela **tomaria de quem está embaixo o passo de simulação do
  voo**.

A coreografia completa, ao apertar `E`:

```
convés:  E → inicia a saída ─────► cortina fecha ─────► empilha a FlightScene
                                   (tela coberta)              │
cabine:                                          aoEntrar ◄────┘
                                   inicia a entrada → cortina abre
```

E na volta, `Esc` fecha a cortina da cabine, o `desempilhar` acontece com a tela
coberta, e a `InteriorScene` recebe `aoRetomar` — onde a cortina dela, **que
ficou fechada desde que a cabine abriu**, começa a subir. É por isso que
`Transicao::avancar` não desliga a saída sozinha: a tela precisa continuar
coberta até a cena de destino aparecer.

`avancar(dt)` devolve `true` **em um único passo**: aquele em que a saída acabou
de cobrir a tela. Esse é o instante da troca.

A duração de cada metade é **argumento**, com o valor de sempre como padrão:
nem toda passagem tem a mesma pressa. A troca entre convés e cabine é curta de
propósito (0,24 s + 0,20 s, curta o bastante para não virar espera); o apagar
depois de a nave se despedaçar é longo de propósito (1,4 s), e a tela de fim abre
em 0,9 s — o fim da nave não volta a ser jogo em 0,2 s.

Duas escolhas de acabamento: a curva é uma **smoothstep** (`t²(3−2t)`, que sai e
chega parada) porque a rampa linear fazia a passagem parecer mais curta do que
é; e a cor `{14, 30, 48}` é o vidro escuro do painel, entre o azul do convés e o
vazio do espaço — é nela que as duas metades emendam sem costura visível.

Enquanto a cortina fecha, a câmera do convés dá zoom no painel — e o zoom anda
pela **cobertura da cortina**, não pelo tempo:

```cpp
const float k = transicao_.cobertura();
camera_.definirZoom(kZoom + (kZoomConsole - kZoom) * k);
```

Assim a volta é o mesmo movimento invertido, sem uma segunda curva para manter
sincronizada com a primeira.

---

## 14. O áudio

Arquivo: [`src/audio/Audio.cpp`](../src/audio/Audio.cpp).

### O básico do som digital

Som digital é **PCM** (*pulse-code modulation*): uma sequência de números, cada
um a amplitude da onda em um instante. A **taxa de amostragem** (*sample rate*)
diz quantos números por segundo — 44100 Hz é o padrão de CD; a **profundidade**
diz o tamanho de cada número — 16 bits com sinal vai de −32768 a 32767. Um
arquivo **WAV** é praticamente isso com um cabeçalho na frente.

**Mixar** é somar várias fontes em uma saída só. Duas notas tocando juntas são,
literalmente, as amostras somadas.

### Sem SDL3_mixer

Quem mixa aqui é o próprio dispositivo de áudio do SDL. Cada reprodução cria um
`SDL_AudioStream` — um tubo que converte formato e taxa de amostragem — e o
prende ao dispositivo com `SDL_BindAudioStream`. O dispositivo soma todos os
tubos ligados nele. É isso; não há nem código de mixagem no projeto.

Cada voz tem um **ganho** (multiplicador de amplitude: 1,0 é o original, 0,0 é
silêncio), aplicado com `SDL_SetAudioStreamGain` na hora em que o dispositivo
puxa os dados — mudar o ganho vale no mesmo instante, o que é o que permite ao
`Flight` ajustar o ambiente a cada passo.

Os handles são opacos: `SomId` para um WAV carregado, `VozId` para uma
reprodução em andamento. `VozId` é um número que só cresce, e não um índice —
assim ele continua válido (ou continua inválido) mesmo quando o vetor de vozes é
remanejado.

### Três detalhes que não são óbvios

**A carência de 250 ms.** Uma voz só é destruída um quarto de segundo depois de o
SDL terminar de ler seus dados. O áudio já lido ainda está tocando no buffer do
dispositivo; destruir o tubo antes cortaria o fim do som.

**O loop não leva flush.** `tocarEmLoop` é a mesma voz reabastecida em
`atualizar()` enquanto a fila do fluxo tiver menos de meio segundo. É o único
caso em que **não** se chama `SDL_FlushAudioStream` — o flush anuncia o fim do
sinal, e o reamostrador (o conversor de taxa de amostragem, aqui de 22050 para
44100 Hz) zeraria o estado interno a cada volta, marcando a emenda com um
estalo. O próprio WAV também precisa emendar o fim no começo; é o que
`gerar_ambiente` garante (seção 15).

**O limite de vozes.** São 16 no máximo. Passando disso, a voz **não-loop** mais
antiga cede lugar, para um efeito novo nunca ser engolido — e os loops são
poupados, porque são ambientes de cena e não podem sumir sozinhos.

O fade de saída (`parar(voz, segundos)`) anda em **tempo real**, dentro de
`Audio::atualizar`, e não no passo fixo: quem pediu o fade em geral já saiu da
pilha de cenas.

---

## 15. Os assets e o gerador

Arquivo: [`tools/gen_assets.py`](../tools/gen_assets.py).

Nenhum asset versionado é um binário opaco que ninguém sabe refazer: todos saem
de um script Python (com Pillow) que qualquer pessoa pode rodar e ajustar.

```sh
python tools/gen_assets.py
```

Ele gera as texturas (tiles do interior, personagem, console), o atlas da fonte
e os WAVs. Dois deles merecem explicação.

**O ambiente (`espaco.wav`) é ruído marrom.** *Ruído branco* tem energia igual em
todas as frequências e soa como chiado de televisão; o **ruído marrom** cai a
−6 **dB por oitava** (a amplitude cai pela metade cada vez que a frequência
dobra), o que soa grave e encorpado — mais próximo de um casco vibrando que de um
chiado. E o filtro que o produz é rodado **em círculo**, ou seja, o fim do sinal
alimenta o começo: é isso que faz o WAV emendar consigo mesmo sem estalo quando o
loop dá a volta.

**A sirene (`sirene.wav`) é só o timbre.** O arquivo é um tom sustentado — a
fundamental de 620 Hz mais a quinta e a oitava, que dão o corte metálico que um
seno puro não tem —, e não a sirene indo e voltando: quem faz o vaivém é o ganho,
no `Flight` (seção 12). Como todo *loop*, ele precisa emendar consigo mesmo, e
aqui isso é uma conta e não um filtro: cada parcial tem que fechar um número
inteiro de ciclos na duração do arquivo. O gerador confere isso e recusa uma
combinação que não feche, em vez de gravar um WAV que estala a cada volta.

**A fonte não depende do sistema.** `assets/fonts/unscii-16.ttf` está em domínio
público e acompanha o repositório justamente para que gerar o atlas não dependa
de nenhuma fonte instalada na máquina de quem roda o script.

Sobre a cópia dos assets para o diretório do build, no `CMakeLists.txt`, há uma
armadilha registrada: a cópia depende dos **arquivos** de assets (um carimbo mais
a lista gravada por `file(CONFIGURE)`), e **não** do relink do executável.
Amarrada a um `POST_BUILD`, mudar só um asset não relinkava nada, a cópia velha
ficava onde estava e o jogo carregava o arquivo antigo. Não volte para
`POST_BUILD`.

---

## 16. O build

```sh
cmake --preset debug && cmake --build build/debug    # o preset release também existe
./build/debug/jogo
```

A única dependência é **SDL3 ≥ 3.4**. A 3.4 trouxe o `SDL_LoadPNG` para o core
(o `SDL_LoadWAV` já estava lá), o texto usa fonte bitmap e o áudio usa
`SDL_AudioStream` — por isso não há `SDL3_image`, `SDL3_ttf` nem `SDL3_mixer`.
Acrescentar um deles precisa de uma necessidade real (OGG, JPG, TrueType
escalável).

**Não há suíte de testes nem linter.** O build é o controle de qualidade, e por
isso ele é rigoroso: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`, hoje sem
nenhum aviso. `-Wconversion` avisa quando um número muda de tipo e pode perder
informação; `-Wshadow` avisa quando uma variável interna esconde outra de fora.
Os dois são chatos de propósito: cada aviso novo é uma pergunta que alguém
precisa responder.

Arquivos `.cpp` novos precisam entrar na lista de fontes do `CMakeLists.txt` —
há glob só para assets, nunca para código, porque um glob de fontes faz o CMake
não perceber que precisa reconfigurar.

---

## 17. Como espiar o programa rodando

Sem testes automatizados, conferir uma mudança é observar o programa. Três
ferramentas dão observação **determinística**, que é melhor que olhar a janela e
achar.

**Rodar sem tela.** Os **drivers dummy** do SDL fingem ter vídeo e áudio, sem
precisar de sessão gráfica nem placa de som:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/release/jogo
```

Se o jogo sobe, roda alguns segundos e sai sem erro, o básico não quebrou — é um
**teste de fumaça**.

**Ver a imagem.** Um patch temporário no laço de `App::rodar()` grava o quadro
com `SDL_RenderReadPixels` + `SDL_SavePNG` e encerra. Com o driver dummy a janela
sai exatos 1280×720, sem nada do desktop em volta; foi assim que os PNGs de
`docs/` foram gerados. Para chegar a uma cena específica, aponte o `main.cpp`
para ela em um build temporário — injetar teclas no compositor é frágil, porque a
janela perde o foco e as teclas vão parar em outra aplicação.

**Ouvir o som.** O SDL tem um driver de áudio **disk**, que em vez de tocar
escreve o PCM cru em um arquivo:

```sh
SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=saida.raw ./build/debug/jogo
```

O arquivo sai no formato do dispositivo (S16LE estéreo a 44100 Hz) e pode ser
medido com qualquer script. É o equivalente sonoro do screenshot: foi assim que
o loop do ambiente, os fades e o abafamento do casco foram conferidos.

Nada de "está suave" ou "está audível": **RMS** por janela de tempo para volume
percebido, diferença amostra a amostra na virada do loop comparada com a média do
sinal para provar que a emenda não estala, deslocamento em pixels entre dois
quadros para provar que a câmera tremeu.

Todo experimento desses entra como patch temporário marcado com `// TEMP`, e
`grep -rn "TEMP" src/` tem que voltar vazio antes de commitar.

---

## Manter este documento

Este é um documento **explicativo**, não um histórico: ele descreve o programa
como ele é hoje. Quando uma mudança altera um mecanismo descrito aqui — o laço,
a pilha, o pipeline 3D, o formato do mapa, o jeito de tocar som —, a seção
correspondente é reescrita no mesmo commit, e não acrescentada como uma nota de
"antes era assim". Termo técnico novo entra no texto e no glossário.

O registro do que mudou e quando é o histórico do git; o "porquê" de cada
mudança vai no corpo da mensagem de commit.

---

## Glossário

| Termo | O que é |
|---|---|
| **Ação** | Comando lógico do jogo (`Interagir`, `Confirmar`), que as cenas consultam no lugar de teclas. |
| **Acumulador** | Onde o tempo real vai se juntando até dar um passo fixo inteiro. |
| **Algoritmo do pintor** | Desenhar de trás para frente para resolver oclusão sem z-buffer. |
| **Alpha (interpolação)** | Fração do passo ainda não simulada, usada para interpolar o desenho entre dois estados. |
| **Âncora** | Ponto do sprite que cai sobre a posição dada: `{0.5, 0.5}` é o centro. |
| **Asset** | Arquivo de conteúdo: imagem, som, fonte, mapa. |
| **Atlas** | Uma imagem com vários desenhos lado a lado, dos quais se recorta um por vez. |
| **Back-face culling** | Descartar as faces que apontam para o lado oposto ao da câmera. |
| **Borda (entrada)** | O instante em que uma tecla passa de solta a pressionada (ou o contrário). |
| **Cena** | Uma tela do jogo (menu, convés, cabine, pausa), que vive na pilha de cenas. |
| **Clipe (animação)** | Uma linha da folha de sprites tocada como sequência: quantos quadros e a que ritmo. |
| **Cortina (transição)** | Retângulo de cor que cobre a tela para emendar duas cenas sem corte. |
| **Culling** | Descartar cedo o que não vai aparecer, para não pagar por desenhá-lo. |
| **dB por oitava** | Quanto a amplitude cai cada vez que a frequência dobra. |
| **Delta time** | Tempo real decorrido entre dois quadros. |
| **Determinístico** | Mesma entrada, mesmo resultado, sempre. |
| **Diretiva** | Linha `chave valor` de um arquivo de dados, lida como um comando por quem carrega. |
| **Diretório de preferências** | Onde cada sistema guarda a configuração de um programa, por usuário (`SDL_GetPrefPath`). |
| **Driver dummy / disk** | Backends do SDL que fingem vídeo/áudio ou gravam o áudio em arquivo. |
| **Esfera de colisão** | Esfera que substitui a geometria real nos testes de colisão. |
| **Espera (debounce)** | Adiar uma ação até que as mudanças parem, para não repeti-la a cada instante. |
| **Espiral da morte** | Quando processar o atraso gera mais atraso do que o quadro consegue absorver. |
| **Folha de sprites** | Atlas em que cada célula é um quadro de animação; aqui, uma linha por clipe. |
| **Fonte bitmap** | Fonte já rasterizada como atlas de glifos, em vez de curvas (TrueType). |
| **FOV** | Abertura angular da câmera. Maior FOV = mais "grande angular". |
| **FPS / quadro** | Uma volta do laço principal; FPS é quantas cabem em um segundo. |
| **Ganho** | Multiplicador de amplitude de um som. |
| **Hotplug** | Conectar ou desconectar um dispositivo com o programa em execução. |
| **Icosaedro** | Poliedro de 20 faces triangulares e 12 vértices. |
| **Legenda (de mapa)** | A tabela que liga cada caractere da grade a um tile. |
| **Leque de triângulos** | Reconstituir um polígono como triângulos que partem todos do mesmo vértice. |
| **Letterbox** | Barras pretas quando a proporção da janela não bate com a do jogo. |
| **Luz difusa (Lambert)** | Brilho proporcional ao ângulo entre a normal da face e a direção da luz. |
| **Malha (mesh)** | Lista de vértices mais a lista de triângulos que os indexam. |
| **Marcador** | Ponto nomeado do cenário (ex.: onde fica o console), em coordenadas de tile. |
| **Mistura aditiva** | Somar a cor ao que já está na tela, em vez de cobri-la; é como luz se comporta. |
| **Mixar** | Somar várias fontes de áudio em uma saída. |
| **Névoa (fog)** | Misturar a cor das faces com a cor do fundo conforme a distância. |
| **Normal** | Vetor perpendicular a uma face; diz para que lado ela aponta. |
| **Overlay** | Cena desenhada por cima de outra, que continua aparecendo atrás. |
| **Passo fixo** | Simular sempre em fatias de tempo iguais, independentemente da taxa de quadros. |
| **PCM** | Som como sequência de amplitudes amostradas. |
| **Pico / RMS** | Maior amplitude / amplitude quadrática média (esta corresponde melhor ao volume percebido). |
| **Produto escalar (dot)** | Mede o quanto dois vetores apontam para o mesmo lado. |
| **Produto vetorial (cross)** | Devolve um vetor perpendicular a outros dois; é como se obtém uma normal. |
| **RAII** | Amarrar o tempo de vida de um recurso ao de um objeto, para que nada vaze. |
| **Rasterizador por software** | Quem transforma triângulos em pixels usando a CPU, sem placa de vídeo. |
| **Reamostrador** | Conversor de taxa de amostragem (ex.: 22050 → 44100 Hz). |
| **Recorte no plano próximo** | Cortar os triângulos contra um plano à frente da câmera antes de projetar. |
| **Regime permanente** | O estado de equilíbrio para o qual um sistema converge; aqui, o atraso `v/k` da câmera. |
| **Resolução lógica** | A resolução fixa em que o jogo desenha (640×360), escalada depois para a janela. |
| **Ruído branco / marrom** | Energia igual em todas as frequências / caindo a −6 dB por oitava. |
| **Scancode** | Código físico da tecla, independente do layout do teclado. |
| **Shader** | Programa que roda na placa de vídeo. Este projeto não usa nenhum. |
| **Smoothstep** | Curva `t²(3−2t)`: vai de 0 a 1 saindo e chegando parada. |
| **Sombreamento flat** | Uma cor por face, sem interpolar entre vértices. |
| **Sutherland–Hodgman** | Algoritmo de recorte de polígono contra um plano. |
| **Taxa de amostragem** | Quantas amostras de som por segundo (44100 Hz é o padrão de CD). |
| **Teste de fumaça** | Verificação mínima de que o programa sobe e roda. |
| **Tile** | Peça quadrada com que um cenário 2D é montado. |
| **Vínculo (binding)** | Ligação entre uma tecla ou botão e uma ação lógica do jogo. |
| **Vsync** | Sincronizar a apresentação do quadro com a atualização do monitor. |
| **Winding** | Sentido (horário/anti-horário) em que os vértices de um triângulo são listados. |
| **Wrap** | Reposicionar por envolvimento: quem sai por um lado entra pelo outro. |
| **Z-buffer** | Memória de profundidade por pixel. Este projeto não usa. |
| **Zona morta** | Faixa central do analógico tratada como zero, para o controle parado não mover nada. |

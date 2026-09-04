# Jogo SDL3

Você anda pelo interior de uma nave em 2D, usa o painel de pilotagem do convés e
a tela vira um voo 3D: um caça low poly atravessando um campo de estrelas gerado
proceduralmente, desviando de asteroides. O mesmo painel também abre o
diagnóstico do casco, que perde um pedaço a cada rocha — inclusive nas batidas
que acontecem enquanto você anda lá dentro. Quando o casco acaba, a nave se
despedaça na vista externa e a viagem termina ali.

![Interior da nave: convés de tiles, janelas para o espaço e o painel de pilotagem com o convite para assumir os controles](docs/interior.png)

![Voo 3D: o caça low poly triangular entre asteroides, com as estrelas riscando a tela ao redor](docs/voo.png)

## Dependências e build

SDL 3.4 ou mais recente, CMake 3.28+, Ninja e um compilador C++20 (GCC, Clang ou
o MSVC do Visual Studio 2022).

Não há dependência de `SDL3_image`, `SDL3_ttf` ou `SDL3_mixer`: o SDL 3.4 já traz
PNG e WAV no core (`SDL_LoadPNG`, `SDL_LoadWAV`), o texto usa uma fonte bitmap e
o áudio mixa vozes com `SDL_AudioStream`. Então a única coisa que precisa ser
instalada é o SDL3 — e é só nisso que os três guias abaixo diferem.

Com as dependências no lugar, o build é o mesmo em qualquer sistema:

```sh
cmake --preset debug
cmake --build build/debug
./build/debug/jogo                  # build\debug\jogo.exe no Windows
```

Há também o preset `release`. Os assets são copiados para junto do executável a
cada build, então o jogo roda de qualquer diretório; eles já vêm gerados em
`assets/`, e `tools/gen_assets.py` (Pillow) os regenera.

### Linux

```sh
sudo pacman -S sdl3 cmake ninja     # Arch; em outras distros, o pacote de dev do SDL3
```

A 3.4 é recente: distribuições com repositório mais conservador ainda empacotam
a 3.2, e aí vale [compilar o SDL](#quando-o-sdl-do-sistema-é-mais-velho-que-34).

### Windows

Instale o **Visual Studio 2022** com a carga de trabalho *Desenvolvimento para
desktop com C++* (traz MSVC, CMake e Ninja) e abra o **x64 Native Tools Command
Prompt for VS 2022** — os presets usam Ninja, que fora desse prompt não acha o
compilador.

O caminho mais curto para o SDL3 é o [vcpkg](https://vcpkg.io):

```bat
git clone https://github.com/microsoft/vcpkg %USERPROFILE%\vcpkg
%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat
%USERPROFILE%\vcpkg\vcpkg install sdl3:x64-windows
cmake --preset debug -DCMAKE_TOOLCHAIN_FILE=%USERPROFILE%/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/debug
```

Alternativa sem vcpkg: baixe o `SDL3-devel-<versão>-VC.zip` das
[releases oficiais](https://github.com/libsdl-org/SDL/releases), extraia e aponte
o CMake para a pasta extraída:

```bat
cmake --preset debug -DCMAKE_PREFIX_PATH=C:/SDL3-3.4.0
```

Nesse caso, copie a `SDL3.dll` de `lib/x64` para junto de `jogo.exe` antes de
rodar — o vcpkg faz isso sozinho, o zip não. Se preferir o gerador do Visual
Studio ao Ninja (para depurar dentro da IDE), troque o preset por
`cmake -B build/vs -G "Visual Studio 17 2022" -A x64` e informe o mesmo
`CMAKE_PREFIX_PATH` ou toolchain.

### macOS

```sh
xcode-select --install              # compilador Clang e ferramentas de linha de comando
brew install cmake ninja sdl3
brew info sdl3                      # confira que é 3.4 ou mais recente
cmake --preset debug -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build/debug
```

O `CMAKE_PREFIX_PATH` é necessário nos Macs com Apple Silicon: o Homebrew instala
em `/opt/homebrew`, que não está na lista de lugares que o CMake procura por
padrão (nos Intel, o `/usr/local` está, e a linha é inofensiva).

### Quando o SDL do sistema é mais velho que 3.4

Compilar o SDL e instalá-lo em um diretório seu funciona nos três sistemas e não
mexe no que está instalado:

```sh
git clone --depth 1 --branch release-3.4.0 https://github.com/libsdl-org/SDL
cmake -S SDL -B SDL/build -DCMAKE_BUILD_TYPE=Release
cmake --build SDL/build --config Release
cmake --install SDL/build --prefix "$HOME/sdl3"
```

Depois é só apontar o build do jogo para esse prefixo:

```sh
cmake --preset debug -DCMAKE_PREFIX_PATH="$HOME/sdl3"
```

## Controles

| Ação | Teclado | Gamepad |
|---|---|---|
| Andar / pilotar / navegar | WASD ou setas | analógico esquerdo / direcional |
| **Usar o painel** (interior) | **E** | X (botão oeste) |
| **Diagnóstico do casco** (no painel) | **Q** | Y (botão norte) |
| Turbo (voo 3D) | Espaço | A (botão sul) |
| Confirmar (menu) | Enter ou Espaço | A (botão sul) |
| Ajustar volume / tela cheia (menu) | ← → | direcional |
| Voltar ao console / fechar o diagnóstico / sair | Esc | B / Back |
| Pausar | Esc ou P | Start |
| Tela cheia | F11 | — |
| Menu (na pausa) | M | — |
| Voltar ao menu (fim de jogo) | Enter ou Esc | A / B / Start |
| **Tela de depuração** (só no build debug) | **F3** | — |

O F3 abre a tela de depuração de qualquer lugar do jogo: quadros por segundo,
onde o jogo está rodando (sistema, drivers de vídeo e áudio, renderer, janela,
caminhos dos arquivos) e o estado da nave, a começar pela integridade do casco.
Ela é uma ferramenta de quem desenvolve, não uma tela do jogo — no build
`release` nem o código nem a tecla existem.

As preferências — volume, tela cheia e os vínculos de cada ação — são gravadas
em `config.ini` no diretório de configuração do sistema (`SDL_GetPrefPath`:
`~/.local/share/jogo-sdl/jogo/` no Linux, `%APPDATA%\jogo-sdl\jogo\` no Windows,
`~/Library/Application Support/jogo-sdl/jogo/` no macOS). O arquivo nasce na
primeira execução e é texto `chave=valor`, com um vínculo por linha:

```ini
volume=0.6
tela-cheia=0

tecla.interagir.1=E
botao.interagir.1=x
```

Como ainda não há tela de remapeamento, editar esse arquivo é o jeito de trocar
um controle; os nomes são os do SDL (`Left`, `Keypad Enter`, `dpleft`). Citar
uma ação substitui os vínculos de fábrica dela, e apagar o arquivo restaura
tudo. Linha estragada é ignorada com um aviso no log.

## O 3D sem shaders

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

O campo de estrelas (`Starfield`) sorteia posições com um xorshift semeado dentro
de um cubo e, a cada quadro, reposiciona cada estrela por *wrap* em torno da
câmera: um campo infinito com memória constante. O rastro sai de projetar a
estrela deslocada de `velocidade * Δt` e desenhar um quadrilátero que desvanece
na cauda, em blending aditivo.

Os asteroides (`AsteroidField`) usam o mesmo cubo com *wrap*, só que com malhas:
icosaedros amassados por um xorshift, normalizados para raio 1 — a escala de
desenho é, portanto, o raio da esfera de colisão, e a colisão é uma comparação de
distância contra a posição da nave. Quem atravessa a borda do cubo não volta na
mesma formação: os eixos que não viraram são sorteados de novo, senão voar em
linha reta traria as mesmas pedras de volta a cada travessia. A névoa do
`Renderer3D` dissolve as faces na cor do vazio antes da borda, o que faz a rocha
emergir em vez de aparecer — e ainda descarta de graça o que já virou fundo.

Bater custa quase toda a velocidade, sacode a câmera, dá um clarão quente e
manda a rocha para outro canto do cubo: a densidade do campo não muda e nada é
alocado durante o voo.

O voo não é a tela do voo: ele é estado da viagem (`sim/Flight.*`) e continua
acontecendo enquanto o jogador anda pelo convés — a nave segue reto no piloto
automático e pode bater, e aí o convés inteiro sacode.

## O convés vem de um arquivo

O interior da nave não é gerado em código: `assets/maps/conves.mapa` é um arquivo
de texto com a legenda de caracteres, marcadores nomeados e a grade desenhada
caractere a caractere. Editar o cenário — mover o painel de pilotagem, abrir uma
janela, mudar o tamanho do convés — é editar esse arquivo e rodar de novo, sem
recompilar.

```
legenda . piso
legenda # parede
marcador console 8.5 2
mapa
####################
#-ooooo------ooooo-#
#.....,..,......,..#
```

Quais nomes de tile existem, o índice de cada um no atlas e quais bloqueiam o
passo continuam no código (`src/world/MapaDeTiles.cpp`): isso é propriedade da
textura e da colisão, não do desenho do cenário.

## Som

Sem `SDL3_mixer`: cada reprodução é um `SDL_AudioStream` ligado ao dispositivo,
que faz a mixagem. Além dos efeitos, a viagem tem um ambiente em *loop* — ruído
marrom cujo ganho acompanha o esforço do motor. Ele toca do convés à cabine,
abafado enquanto o casco está no caminho, e entra e sai em rampa.

O WAV do ambiente é gerado em `tools/gen_assets.py` por um integrador com
vazamento rodado **em círculo**: uma passada só para aquecer o estado e outra
para valer, de modo que a última amostra emenda na primeira como emendaria no
meio do sinal — o loop não tem costura audível e não precisa de crossfade.

## Como o laço funciona

`App::rodar()` mede o tempo real do quadro, acumula e simula em fatias fixas de
1/60 s (`StepTimer::kPassoFixo`), com teto de 0,25 s por quadro para evitar
avalanche de updates depois de um travamento. O que sobra no acumulador vira o
`alpha` passado ao desenho, usado para interpolar posição e orientação. Assim a
física é determinística e o desenho é suave em qualquer taxa de quadros.

Tudo é desenhado em coordenadas lógicas de **640×360**
(`SDL_SetRenderLogicalPresentation` com letterbox); o SDL escala para o tamanho
real da janela e converte as coordenadas do mouse.

## Arquitetura

Mapa do código, como adicionar uma cena e como adicionar assets:
[docs/ARQUITETURA.md](docs/ARQUITETURA.md).

Como cada peça do programa funciona, do laço principal ao rasterizador 3D, com
todos os termos explicados: [docs/FUNCIONAMENTO.md](docs/FUNCIONAMENTO.md).

## Licença

Código sob a licença MIT — veja [LICENSE](LICENSE).

O atlas de texto é rasterizado da [unscii](http://viznut.fi/unscii/), fonte
bitmap de Viznut em domínio público, que acompanha o repositório:
[assets/fonts/README.md](assets/fonts/README.md).

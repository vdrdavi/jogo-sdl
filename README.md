# Jogo SDL3

Você anda pelo interior de uma nave em 2D, usa o painel de pilotagem do convés e
a tela vira um voo 3D: um caça low poly atravessando um campo de estrelas gerado
proceduralmente, desviando de asteroides.

![Interior da nave: convés de tiles, janelas para o espaço e o painel de pilotagem com o convite para assumir os controles](docs/interior.png)

![Voo 3D: o caça low poly triangular entre asteroides, com as estrelas riscando a tela ao redor](docs/voo.png)

## Dependências e build

SDL 3.4 ou mais recente, CMake 3.28+ e um compilador C++20.

```sh
sudo pacman -S sdl3 cmake ninja     # Arch; em outras distros, o pacote de dev do SDL3
cmake --preset debug
cmake --build build/debug
./build/debug/jogo
```

Não há dependência de `SDL3_image`, `SDL3_ttf` ou `SDL3_mixer`: o SDL 3.4 já traz
PNG e WAV no core (`SDL_LoadPNG`, `SDL_LoadWAV`), o texto usa uma fonte bitmap e
o áudio mixa vozes com `SDL_AudioStream`.

Há também o preset `release`. Os assets são copiados para junto do executável a
cada build, então o jogo roda de qualquer diretório; eles já vêm gerados em
`assets/`, e `tools/gen_assets.py` (Pillow) os regenera.

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

Como cada coisa foi feita e por quê, com os termos técnicos explicados e o
diário de cada implementação: [docs/DESENVOLVIMENTO.md](docs/DESENVOLVIMENTO.md).

## Licença

Código sob a licença MIT — veja [LICENSE](LICENSE).

O atlas de texto é rasterizado da [unscii](http://viznut.fi/unscii/), fonte
bitmap de Viznut em domínio público, que acompanha o repositório:
[assets/fonts/README.md](assets/fonts/README.md).

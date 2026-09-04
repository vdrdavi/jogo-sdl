---
name: rodar-release
description: Compila o jogo no preset release e abre a janela do jogo. Use quando o usuario pedir para rodar, abrir ou buildar o jogo em release (otimizado), conferir desempenho, ou validar o binario que vai para instalacao.
allowed-tools: Bash, Read, Glob, Grep
model: haiku
effort: low
---

# Rodar o jogo em release

Compila com o preset `release` (`CMAKE_BUILD_TYPE=Release`, otimizado, sem
simbolos de depuracao) e abre a janela do jogo na sessao grafica do usuario.
E esta a build que representa o que o jogador final recebe.

## Passos

1. Configure e compile, sempre a partir da raiz do projeto:

   ```sh
   cmake --preset release && cmake --build build/release
   ```

   O `cmake --preset` e barato quando o cache ja existe, entao rode os dois
   sempre — e ele que pega `CMakeLists.txt` novo (arquivo `.cpp` adicionado, por
   exemplo).

2. Se o build falhar, **pare aqui**. Release pega coisa que debug deixa passar
   (uso de variavel nao inicializada, warning que so aparece com otimizacao) —
   trate como resultado, nao como ruido.

   Nao conserte o codigo por conta propria: quem mexe no C++ aqui e o modelo
   da sessao, nao o modelo leve que roda esta skill. Mostre a saida do
   compilador como ela veio e devolva a conversa.

3. O build usa `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` e o projeto esta
   hoje sem nenhum warning. Se aparecer warning novo, avise o usuario — o build
   e o unico controle de qualidade que existe aqui (nao ha testes nem linter).

4. Abra o jogo em segundo plano, para nao travar a sessao:

   ```sh
   ./build/release/jogo
   ```

   Use `run_in_background: true` no Bash. A janela abre na sessao grafica do
   usuario; ele fecha quando quiser. Diga a ele que o jogo esta aberto e que
   voce fica esperando o retorno dele.

## Detalhes que evitam retrabalho

- **Nunca** rode com `SDL_VIDEODRIVER=dummy` aqui: o pedido e abrir o jogo de
  verdade. O driver dummy so serve para fumaca sem sessao grafica:

  ```sh
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/release/jogo
  ```

  Use isso apenas se o usuario pedir explicitamente uma checagem sem janela — e
  mate o processo depois de alguns segundos.
- Nao tente injetar teclas (ydotool e afins) para "dirigir" o jogo: a janela
  perde o foco e as teclas vao parar em outra aplicacao do usuario. Quem joga e
  ele.
- Release e a build que se instala. Se o usuario pedir o pacote instalado em vez
  da janela:

  ```sh
  cmake --install build/release --prefix <dir>
  ```

- Se o jogo em segundo plano terminar sozinho logo depois de abrir, leia a saida
  do processo: costuma ser asset faltando ou falha ao criar janela/audio.

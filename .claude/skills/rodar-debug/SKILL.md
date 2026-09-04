---
name: rodar-debug
description: Compila o jogo no preset debug e abre a janela do jogo. Use quando o usuario pedir para rodar, abrir, testar ou buildar o jogo em debug (com assercoes e simbolos), ou disser apenas "roda o jogo" durante um trabalho de depuracao.
allowed-tools: Bash, Read, Glob, Grep
model: haiku
effort: low
---

# Rodar o jogo em debug

Compila com o preset `debug` (`CMAKE_BUILD_TYPE=Debug`, sem otimizacao, com
simbolos) e abre a janela do jogo na sessao grafica do usuario.

## Passos

1. Configure e compile, sempre a partir da raiz do projeto:

   ```sh
   cmake --preset debug && cmake --build build/debug
   ```

   O `cmake --preset` e barato quando o cache ja existe, entao rode os dois
   sempre — e ele que pega `CMakeLists.txt` novo (arquivo `.cpp` adicionado, por
   exemplo).

2. Se o build falhar, **pare aqui**. Nao rode um binario velho fingindo que a
   mudanca foi testada.

   Nao conserte o codigo por conta propria: quem mexe no C++ aqui e o modelo
   da sessao, nao o modelo leve que roda esta skill. Mostre a saida do
   compilador como ela veio e devolva a conversa.

3. O build usa `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` e o projeto esta
   hoje sem nenhum warning. Se aparecer warning novo, avise o usuario — o build
   e o unico controle de qualidade que existe aqui (nao ha testes nem linter).

4. Abra o jogo em segundo plano, para nao travar a sessao:

   ```sh
   ./build/debug/jogo
   ```

   Use `run_in_background: true` no Bash. A janela abre na sessao grafica do
   usuario; ele fecha quando quiser. Diga a ele que o jogo esta aberto e que
   voce fica esperando o retorno dele.

## Detalhes que evitam retrabalho

- **Nunca** rode com `SDL_VIDEODRIVER=dummy` aqui: o pedido e abrir o jogo de
  verdade. O driver dummy so serve para fumaca sem sessao grafica.
- Nao tente injetar teclas (ydotool e afins) para "dirigir" o jogo: a janela
  perde o foco e as teclas vao parar em outra aplicacao do usuario. Quem joga e
  ele.
- Para conferir uma tela especifica sem depender do usuario, o caminho
  deterministico e o patch temporario com `SDL_RenderReadPixels` + `SDL_SavePNG`
  descrito no `CLAUDE.md` — marque com `// TEMP` e confira com
  `grep -rn "TEMP" src/` antes de qualquer commit.
- Se o jogo em segundo plano terminar sozinho logo depois de abrir, leia a saida
  do processo: costuma ser asset faltando ou falha ao criar janela/audio.

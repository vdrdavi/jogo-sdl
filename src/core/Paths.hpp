#pragma once

#include <string>
#include <string_view>

namespace jogo::paths {

/// Diretorio raiz dos assets, resolvido uma unica vez: primeiro ao lado do
/// executavel (build/instalacao), depois o diretorio de fontes definido pelo
/// CMake em JOGO_ASSETS_DIR. Termina com separador.
const std::string& assetsRoot();

/// Caminho absoluto de um asset, ex.: asset("textures/player.png").
std::string asset(std::string_view relativo);

/// Diretorio de preferencias do usuario (SDL_GetPrefPath), criado se preciso.
/// E o unico lugar onde o jogo escreve: o diretorio dos assets pode ser
/// somente leitura (uma instalacao em /usr) e nao acompanha o usuario. Termina
/// com separador; vazio se o SDL nao souber responder.
const std::string& prefRoot();

/// Caminho de um arquivo de preferencias, ex.: pref("config.ini"). Vazio quando
/// prefRoot() e vazio, para quem chama nao acabar escrevendo no cwd.
std::string pref(std::string_view relativo);

}  // namespace jogo::paths

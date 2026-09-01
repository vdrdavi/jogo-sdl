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

}  // namespace jogo::paths

#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace jogo {

class Audio;
class Input;

/// Preferencias que sobrevivem ao fechamento do jogo: volume, tela cheia e os
/// vinculos de cada acao.
///
/// Nao ha uma copia dos valores aqui. Cada preferencia ja tem um dono -- o
/// volume e do Audio, a tela cheia e da janela, os vinculos sao do Input -- e
/// duplica-los criaria dois estados para sincronizar. Este par de funcoes so
/// leva os valores dos donos para o disco e de volta.
namespace config {

/// Caminho do arquivo, dentro de paths::prefRoot(). Vazio se o SDL nao souber
/// dizer onde fica o diretorio de preferencias.
const std::string& caminho();

/// Aplica o arquivo sobre os subsistemas e devolve se havia um arquivo para
/// ler. Chave desconhecida ou valor torto nao sao erro: o que nao for lido fica
/// como estava, que e o padrao de fabrica.
bool carregar(SDL_Window* janela, Audio& audio, Input& input);

/// Reescreve o arquivo inteiro. Devolve false e loga em caso de falha -- nao
/// poder salvar as preferencias nao e motivo para o jogo parar.
bool salvar(SDL_Window* janela, const Audio& audio, const Input& input);

}  // namespace config
}  // namespace jogo

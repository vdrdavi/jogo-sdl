cmake_minimum_required(VERSION 3.28)

project(jogo
    VERSION 0.1.0
    DESCRIPTION "Base para jogo 2D em SDL3"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# SDL 3.4+ traz carregamento de PNG (SDL_LoadPNG) e de WAV (SDL_LoadWAV) no core,
# e o texto e o audio da base usam apenas essas APIs: nenhuma biblioteca satelite
# (SDL3_image / SDL3_ttf / SDL3_mixer) e necessaria.
find_package(SDL3 3.4 CONFIG REQUIRED)

add_executable(jogo
    src/main.cpp
    src/core/App.cpp
    src/core/Config.cpp
    src/core/Paths.cpp
    src/input/Input.cpp
    src/gfx/Animacao.cpp
    src/gfx/Assets.cpp
    src/gfx/Camera.cpp
    src/gfx/Draw.cpp
    src/gfx/BitmapFont.cpp
    src/audio/Audio.cpp
    src/scene/SceneStack.cpp
    src/sim/Flight.cpp
    src/world/MapaDeTiles.cpp
    src/gfx3d/AsteroidField.cpp
    src/gfx3d/Destrocos.cpp
    src/gfx3d/Mesh.cpp
    src/gfx3d/Renderer3D.cpp
    src/gfx3d/Starfield.cpp
    src/scenes/MenuScene.cpp
    src/scenes/InteriorScene.cpp
    src/scenes/FlightScene.cpp
    src/scenes/PauseScene.cpp
    src/scenes/StatusScene.cpp
    src/scenes/GameOverScene.cpp
    src/scenes/Transicao.cpp
)

target_include_directories(jogo PRIVATE src)
target_link_libraries(jogo PRIVATE SDL3::SDL3)

# A tela de depuracao (F3) e uma ferramenta de quem desenvolve, nao uma tela do
# jogo: na build de release ela nao existe. O arquivo so entra na lista de
# fontes na configuracao Debug, e JOGO_DEBUG e o que apaga o gancho do F3 em
# App.cpp -- assim o release nao carrega nem o codigo nem a tecla.
target_sources(jogo PRIVATE $<$<CONFIG:Debug>:src/scenes/DebugScene.cpp>)
target_compile_definitions(jogo PRIVATE $<$<CONFIG:Debug>:JOGO_DEBUG>)

# Caminho de assets usado quando o executavel roda de dentro de build/ (dev).
# Em uma instalacao, os assets ficam ao lado do binario e tem prioridade.
target_compile_definitions(jogo PRIVATE
    JOGO_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(jogo PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
elseif(MSVC)
    target_compile_options(jogo PRIVATE /W4)
endif()

# Copia os assets para junto do binario, para o jogo rodar de qualquer cwd.
#
# A copia depende dos ARQUIVOS de assets, nao do alvo: amarrada a um POST_BUILD
# do "jogo", ela so rodaria quando o executavel fosse religado, e mudar apenas
# um asset (regerar uma textura, editar um WAV) nao mexe em nenhum .cpp. A copia
# em build/<preset>/assets/ ficaria velha e o jogo carregaria o asset antigo,
# porque jogo::paths::assetsRoot() prefere o diretorio do binario.
#
# CONFIGURE_DEPENDS faz o CMake se reconfigurar sozinho quando um arquivo e
# adicionado ou removido de assets/.
file(GLOB_RECURSE ARQUIVOS_DE_ASSETS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")

# A lista de assets tambem vira uma dependencia, em arquivo. Sem ela, REMOVER um
# asset nao dispararia a copia: o ninja compara a data das entradas que restaram
# com a do carimbo, e nenhuma delas ficou mais nova. file(CONFIGURE) so reescreve
# quando o conteudo muda, entao isso nao suja o build incremental.
set(LISTA_DE_ASSETS "${CMAKE_CURRENT_BINARY_DIR}/assets.list")
string(REPLACE ";" "\n" CONTEUDO_DA_LISTA "${ARQUIVOS_DE_ASSETS}")
file(CONFIGURE OUTPUT "${LISTA_DE_ASSETS}" CONTENT "${CONTEUDO_DA_LISTA}\n")

# O destino e apagado antes de copiar porque copy_directory faz merge: sem isso,
# um asset removido de assets/ sobreviveria na copia e o jogo continuaria
# achando um arquivo que nao existe mais nas fontes.
set(CARIMBO_DE_ASSETS "${CMAKE_CURRENT_BINARY_DIR}/assets.stamp")
add_custom_command(
    OUTPUT "${CARIMBO_DE_ASSETS}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:jogo>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:jogo>/assets"
    COMMAND ${CMAKE_COMMAND} -E touch "${CARIMBO_DE_ASSETS}"
    DEPENDS ${ARQUIVOS_DE_ASSETS} "${LISTA_DE_ASSETS}"
    COMMENT "Copiando assets para o diretorio do executavel"
    VERBATIM)

add_custom_target(assets ALL DEPENDS "${CARIMBO_DE_ASSETS}")
add_dependencies(jogo assets)

include(GNUInstallDirs)
install(TARGETS jogo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets DESTINATION ${CMAKE_INSTALL_BINDIR})

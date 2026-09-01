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
    src/core/Paths.cpp
    src/input/Input.cpp
    src/gfx/Assets.cpp
    src/gfx/Camera.cpp
    src/gfx/Draw.cpp
    src/gfx/BitmapFont.cpp
    src/audio/Audio.cpp
    src/scene/SceneStack.cpp
    src/gfx3d/Mesh.cpp
    src/gfx3d/Renderer3D.cpp
    src/gfx3d/Starfield.cpp
    src/scenes/MenuScene.cpp
    src/scenes/InteriorScene.cpp
    src/scenes/FlightScene.cpp
    src/scenes/PauseScene.cpp
)

target_include_directories(jogo PRIVATE src)
target_link_libraries(jogo PRIVATE SDL3::SDL3)

# Caminho de assets usado quando o executavel roda de dentro de build/ (dev).
# Em uma instalacao, os assets ficam ao lado do binario e tem prioridade.
target_compile_definitions(jogo PRIVATE
    JOGO_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(jogo PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
elseif(MSVC)
    target_compile_options(jogo PRIVATE /W4)
endif()

# Copia os assets para junto do binario a cada build, para o jogo rodar de qualquer cwd.
add_custom_command(TARGET jogo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:jogo>/assets"
    COMMENT "Copiando assets para o diretorio do executavel")

include(GNUInstallDirs)
install(TARGETS jogo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY assets DESTINATION ${CMAKE_INSTALL_BINDIR})

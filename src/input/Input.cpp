#include "input/Input.hpp"

#include <algorithm>
#include <cmath>

#include "core/Log.hpp"

namespace jogo {
namespace {

constexpr float kZonaMorta = 0.25f;
constexpr std::size_t kNumAcoes = Input::kNumAcoes;

/// Vinculos de fabrica; a ordem segue o enum Acao. Sao o ponto de partida do
/// mapa que o Input carrega, e para onde restaurarPadroes() volta.
constexpr std::array<Input::Mapeamento, kNumAcoes> kPadrao{{
    // Esquerda
    {{{SDL_SCANCODE_LEFT, SDL_SCANCODE_A, SDL_SCANCODE_UNKNOWN}},
     {{SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_INVALID}}},
    // Direita
    {{{SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, SDL_SCANCODE_UNKNOWN}},
     {{SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_INVALID}}},
    // Cima
    {{{SDL_SCANCODE_UP, SDL_SCANCODE_W, SDL_SCANCODE_UNKNOWN}},
     {{SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_INVALID}}},
    // Baixo
    {{{SDL_SCANCODE_DOWN, SDL_SCANCODE_S, SDL_SCANCODE_UNKNOWN}},
     {{SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_INVALID}}},
    // Confirmar
    {{{SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE, SDL_SCANCODE_KP_ENTER}},
     {{SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_INVALID}}},
    // Voltar
    {{{SDL_SCANCODE_ESCAPE, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_UNKNOWN}},
     {{SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_BACK}}},
    // Pausar
    {{{SDL_SCANCODE_ESCAPE, SDL_SCANCODE_P, SDL_SCANCODE_UNKNOWN}},
     {{SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_INVALID}}},
    // Interagir
    {{{SDL_SCANCODE_E, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN}},
     {{SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_INVALID}}},
    // Diagnostico
    {{{SDL_SCANCODE_Q, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN}},
     {{SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_INVALID}}},
}};

/// Nomes das acoes, tambem na ordem do enum.
constexpr std::array<std::string_view, kNumAcoes> kNomes{
    "esquerda", "direita", "cima",   "baixo",
    "confirmar", "voltar", "pausar", "interagir",
    "diagnostico",
};

float normalizarEixo(Sint16 bruto) {
    const float valor = static_cast<float>(bruto) / 32767.0f;
    return std::clamp(valor, -1.0f, 1.0f);
}

float aplicarZonaMorta(float valor) {
    if (std::fabs(valor) < kZonaMorta) {
        return 0.0f;
    }
    // Reescala para que o movimento comece suave logo apos a zona morta.
    const float sinal = valor < 0.0f ? -1.0f : 1.0f;
    return sinal * (std::fabs(valor) - kZonaMorta) / (1.0f - kZonaMorta);
}

}  // namespace

std::string_view nomeDaAcao(Acao acao) {
    return kNomes[static_cast<std::size_t>(acao)];
}

Acao acaoPorNome(std::string_view nome) {
    for (std::size_t i = 0; i < kNumAcoes; ++i) {
        if (kNomes[i] == nome) {
            return static_cast<Acao>(i);
        }
    }
    return Acao::Contagem;
}

Input::Input() {
    restaurarPadroes();
}

const Input::Mapeamento& Input::mapeamento(Acao acao) const {
    return mapa_[static_cast<std::size_t>(acao)];
}

void Input::definirMapeamento(Acao acao, const Mapeamento& mapeamento) {
    mapa_[static_cast<std::size_t>(acao)] = mapeamento;
}

const Input::Mapeamento& Input::mapeamentoPadrao(Acao acao) {
    return kPadrao[static_cast<std::size_t>(acao)];
}

void Input::restaurarPadroes() {
    mapa_ = kPadrao;
}

void Input::iniciar() {
    int total = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&total);
    if (ids == nullptr) {
        return;
    }
    for (int i = 0; i < total; ++i) {
        abrirGamepad(ids[i]);
    }
    SDL_free(ids);
}

void Input::abrirGamepad(SDL_JoystickID id) {
    const auto ja = std::find_if(gamepads_.begin(), gamepads_.end(),
                                 [id](const GamepadAberto& g) { return g.id == id; });
    if (ja != gamepads_.end()) {
        return;
    }
    GamepadPtr pad{SDL_OpenGamepad(id)};
    if (!pad) {
        JOGO_ERRO_SDL("SDL_OpenGamepad");
        return;
    }
    JOGO_INFO("Gamepad conectado: %s", SDL_GetGamepadName(pad.get()));
    gamepads_.push_back(GamepadAberto{id, std::move(pad)});
}

void Input::fecharGamepad(SDL_JoystickID id) {
    const auto it = std::find_if(gamepads_.begin(), gamepads_.end(),
                                 [id](const GamepadAberto& g) { return g.id == id; });
    if (it != gamepads_.end()) {
        JOGO_INFO("Gamepad desconectado");
        gamepads_.erase(it);
    }
}

void Input::onEvent(const SDL_Event& evento) {
    switch (evento.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            abrirGamepad(evento.gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            fecharGamepad(evento.gdevice.which);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            rodaAcumulada_ += evento.wheel.y;
            break;
        default:
            break;
    }
}

void Input::novoQuadro(SDL_Renderer* renderer) {
    if (consumido_) {
        teclasAntes_ = teclas_;
        botoesAntes_ = botoes_;
        botoesMouseAntes_ = botoesMouse_;
        roda_ = 0.0f;
        consumido_ = false;
    }

    int numTeclas = 0;
    const bool* estado = SDL_GetKeyboardState(&numTeclas);
    const std::size_t limite =
        std::min(static_cast<std::size_t>(numTeclas), teclas_.size());
    for (std::size_t i = 0; i < limite; ++i) {
        teclas_[i] = estado[i];
    }

    float janelaX = 0.0f;
    float janelaY = 0.0f;
    botoesMouse_ = SDL_GetMouseState(&janelaX, &janelaY);
    if (renderer == nullptr ||
        !SDL_RenderCoordinatesFromWindow(renderer, janelaX, janelaY, &mouse_.x, &mouse_.y)) {
        mouse_ = SDL_FPoint{janelaX, janelaY};
    }

    roda_ += rodaAcumulada_;
    rodaAcumulada_ = 0.0f;

    botoes_.fill(false);
    eixos_.fill(0.0f);
    for (const GamepadAberto& g : gamepads_) {
        for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b) {
            if (SDL_GetGamepadButton(g.pad.get(), static_cast<SDL_GamepadButton>(b))) {
                botoes_[static_cast<std::size_t>(b)] = true;
            }
        }
        for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; ++a) {
            const float valor =
                normalizarEixo(SDL_GetGamepadAxis(g.pad.get(), static_cast<SDL_GamepadAxis>(a)));
            const std::size_t idx = static_cast<std::size_t>(a);
            // Com varios controles, vale o eixo de maior deflexao.
            if (std::fabs(valor) > std::fabs(eixos_[idx])) {
                eixos_[idx] = valor;
            }
        }
    }
}

bool Input::teclaAtiva(SDL_Scancode tecla) const {
    return tecla != SDL_SCANCODE_UNKNOWN && teclas_[static_cast<std::size_t>(tecla)];
}

bool Input::teclaPressionada(SDL_Scancode tecla) const {
    if (tecla == SDL_SCANCODE_UNKNOWN) {
        return false;
    }
    const std::size_t i = static_cast<std::size_t>(tecla);
    return teclas_[i] && !teclasAntes_[i];
}

bool Input::botaoGamepadAtivo(SDL_GamepadButton botao) const {
    return botao != SDL_GAMEPAD_BUTTON_INVALID && botoes_[static_cast<std::size_t>(botao)];
}

bool Input::botaoGamepadPressionado(SDL_GamepadButton botao) const {
    if (botao == SDL_GAMEPAD_BUTTON_INVALID) {
        return false;
    }
    const std::size_t i = static_cast<std::size_t>(botao);
    return botoes_[i] && !botoesAntes_[i];
}

bool Input::acaoAtiva(Acao acao) const {
    const Mapeamento& m = mapeamento(acao);
    for (SDL_Scancode tecla : m.teclas) {
        if (teclaAtiva(tecla)) {
            return true;
        }
    }
    for (SDL_GamepadButton botao : m.botoes) {
        if (botaoGamepadAtivo(botao)) {
            return true;
        }
    }
    return false;
}

bool Input::acaoPressionada(Acao acao) const {
    const Mapeamento& m = mapeamento(acao);
    for (SDL_Scancode tecla : m.teclas) {
        if (teclaPressionada(tecla)) {
            return true;
        }
    }
    for (SDL_GamepadButton botao : m.botoes) {
        if (botaoGamepadPressionado(botao)) {
            return true;
        }
    }
    return false;
}

bool Input::acaoSolta(Acao acao) const {
    const Mapeamento& m = mapeamento(acao);
    for (SDL_Scancode tecla : m.teclas) {
        if (tecla == SDL_SCANCODE_UNKNOWN) {
            continue;
        }
        const std::size_t i = static_cast<std::size_t>(tecla);
        if (!teclas_[i] && teclasAntes_[i]) {
            return true;
        }
    }
    for (SDL_GamepadButton botao : m.botoes) {
        if (botao == SDL_GAMEPAD_BUTTON_INVALID) {
            continue;
        }
        const std::size_t i = static_cast<std::size_t>(botao);
        if (!botoes_[i] && botoesAntes_[i]) {
            return true;
        }
    }
    return false;
}

bool Input::botaoMouseAtivo(int botao) const {
    return (botoesMouse_ & SDL_BUTTON_MASK(botao)) != 0;
}

bool Input::botaoMousePressionado(int botao) const {
    const SDL_MouseButtonFlags mascara = SDL_BUTTON_MASK(botao);
    return (botoesMouse_ & mascara) != 0 && (botoesMouseAntes_ & mascara) == 0;
}

SDL_FPoint Input::eixoMovimento() const {
    SDL_FPoint direcao{0.0f, 0.0f};

    if (acaoAtiva(Acao::Esquerda)) {
        direcao.x -= 1.0f;
    }
    if (acaoAtiva(Acao::Direita)) {
        direcao.x += 1.0f;
    }
    if (acaoAtiva(Acao::Cima)) {
        direcao.y -= 1.0f;
    }
    if (acaoAtiva(Acao::Baixo)) {
        direcao.y += 1.0f;
    }

    const float analogicoX = aplicarZonaMorta(eixos_[SDL_GAMEPAD_AXIS_LEFTX]);
    const float analogicoY = aplicarZonaMorta(eixos_[SDL_GAMEPAD_AXIS_LEFTY]);
    if (analogicoX != 0.0f) {
        direcao.x = analogicoX;
    }
    if (analogicoY != 0.0f) {
        direcao.y = analogicoY;
    }

    // Normaliza para a diagonal nao ser mais rapida que a horizontal.
    const float tamanho = std::sqrt(direcao.x * direcao.x + direcao.y * direcao.y);
    if (tamanho > 1.0f) {
        direcao.x /= tamanho;
        direcao.y /= tamanho;
    }
    return direcao;
}

}  // namespace jogo

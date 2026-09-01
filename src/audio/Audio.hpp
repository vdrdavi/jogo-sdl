#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/SdlPtr.hpp"

namespace jogo {

/// Audio sobre a API nativa do SDL3: cada som e um WAV carregado na memoria e
/// cada reproducao usa um SDL_AudioStream preso ao dispositivo, que faz a
/// mixagem. Sem dependencia de SDL3_mixer.
class Audio {
public:
    /// Handle opaco de um som carregado (0 = invalido).
    using SomId = std::size_t;
    static constexpr int kMaxVozes = 16;

    bool iniciar();
    void encerrar();

    /// Carrega (ou reaproveita) um WAV. Ex.: carregar("audio/blip.wav").
    SomId carregar(std::string_view relativo);

    /// Toca um som. `ganho` em 0..1 multiplica o volume geral.
    void tocar(SomId som, float ganho = 1.0f);

    /// Recolhe as vozes que ja terminaram; chamar uma vez por quadro.
    void atualizar();

    float volume() const { return volume_; }
    void definirVolume(float v);

    bool ativo() const { return dispositivo_ != 0; }

private:
    struct Som {
        SDL_AudioSpec spec{};
        std::vector<Uint8> dados;
    };

    /// Uma reproducao em andamento. O fluxo so e destruido depois de uma
    /// carencia: quando o SDL termina de ler os dados ainda ha alguns
    /// milissegundos tocando no buffer do dispositivo.
    struct Voz {
        AudioStreamPtr fluxo;
        Uint64 fimNs{0};
    };

    SDL_AudioDeviceID dispositivo_{0};
    std::vector<Som> sons_;
    std::unordered_map<std::string, SomId> porCaminho_;
    std::vector<Voz> vozes_;
    float volume_{0.6f};
};

}  // namespace jogo

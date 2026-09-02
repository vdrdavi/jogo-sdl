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
    /// Handle de uma reproducao em andamento (0 = invalido). Sobrevive ao
    /// remanejamento do vetor de vozes, ao contrario de um indice.
    using VozId = std::size_t;
    static constexpr int kMaxVozes = 16;

    bool iniciar();
    void encerrar();

    /// Carrega (ou reaproveita) um WAV. Ex.: carregar("audio/blip.wav").
    SomId carregar(std::string_view relativo);

    /// Toca um som. `ganho` em 0..1 multiplica o volume geral.
    void tocar(SomId som, float ganho = 1.0f);

    /// Toca um som repetido sem emenda ate parar(). Devolve o handle da voz, e
    /// o WAV precisa emendar o fim no comeco (veja gerar_ambiente em
    /// tools/gen_assets.py).
    VozId tocarEmLoop(SomId som, float ganho = 1.0f);

    /// Muda o ganho de uma voz em andamento; vale no mesmo instante. Quem quiser
    /// uma rampa suaviza o proprio valor antes de chamar.
    void ajustarGanho(VozId voz, float ganho);

    /// Encerra uma voz, opcionalmente com fade -- cortar um ambiente longo de
    /// uma vez e audivel. Handle invalido ou ja encerrado e no-op.
    void parar(VozId voz, float segundos = 0.0f);

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
        VozId id{0};
        SomId som{0};
        bool loop{false};
        /// Ganho da voz antes do volume geral.
        float ganho{1.0f};
        /// Fade de saida em curso: o ganho cai a essa taxa (por segundo) e a voz
        /// e recolhida ao chegar em zero.
        float taxaFade{0.0f};
        bool encerrando{false};
    };

    VozId iniciarVoz(SomId som, float ganho, bool loop);
    Voz* procurar(VozId voz);
    void aplicarGanho(const Voz& voz) const;
    /// Reenfileira o WAV enquanto a fila do loop estiver curta.
    void reabastecer(Voz& voz);

    SDL_AudioDeviceID dispositivo_{0};
    std::vector<Som> sons_;
    std::unordered_map<std::string, SomId> porCaminho_;
    std::vector<Voz> vozes_;
    VozId proximoId_{1};
    Uint64 ultimaAtualizacaoNs_{0};
    float volume_{0.6f};
};

}  // namespace jogo

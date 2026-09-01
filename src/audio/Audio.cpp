#include "audio/Audio.hpp"

#include <algorithm>

#include "core/Log.hpp"
#include "core/Paths.hpp"

namespace jogo {

bool Audio::iniciar() {
    dispositivo_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (dispositivo_ == 0) {
        // Sem placa de som o jogo continua rodando; so fica mudo.
        JOGO_ERRO_SDL("SDL_OpenAudioDevice (o jogo seguira sem audio)");
        return false;
    }
    // O indice 0 e reservado para "som invalido".
    sons_.emplace_back();
    return true;
}

void Audio::encerrar() {
    vozes_.clear();
    sons_.clear();
    porCaminho_.clear();
    if (dispositivo_ != 0) {
        SDL_CloseAudioDevice(dispositivo_);
        dispositivo_ = 0;
    }
}

Audio::SomId Audio::carregar(std::string_view relativo) {
    const std::string chave{relativo};
    if (const auto it = porCaminho_.find(chave); it != porCaminho_.end()) {
        return it->second;
    }
    if (!ativo()) {
        return 0;
    }

    const std::string caminho = paths::asset(relativo);
    SDL_AudioSpec spec{};
    Uint8* buffer = nullptr;
    Uint32 tamanho = 0;
    if (!SDL_LoadWAV(caminho.c_str(), &spec, &buffer, &tamanho)) {
        JOGO_ERRO_SDL(caminho.c_str());
        return 0;
    }

    Som som;
    som.spec = spec;
    som.dados.assign(buffer, buffer + tamanho);
    SDL_free(buffer);

    if (sons_.empty()) {
        sons_.emplace_back();
    }
    const SomId id = sons_.size();
    sons_.push_back(std::move(som));
    porCaminho_.emplace(chave, id);
    return id;
}

void Audio::tocar(SomId som, float ganho) {
    if (!ativo() || som == 0 || som >= sons_.size()) {
        return;
    }
    atualizar();
    if (vozes_.size() >= static_cast<std::size_t>(kMaxVozes)) {
        // Voz mais antiga cede lugar, para um efeito novo nunca ser engolido.
        vozes_.erase(vozes_.begin());
    }

    const Som& fonte = sons_[som];
    AudioStreamPtr voz{SDL_CreateAudioStream(&fonte.spec, nullptr)};
    if (!voz) {
        JOGO_ERRO_SDL("SDL_CreateAudioStream");
        return;
    }
    if (!SDL_BindAudioStream(dispositivo_, voz.get())) {
        JOGO_ERRO_SDL("SDL_BindAudioStream");
        return;
    }
    SDL_SetAudioStreamGain(voz.get(), std::clamp(ganho, 0.0f, 1.0f) * volume_);
    if (!SDL_PutAudioStreamData(voz.get(), fonte.dados.data(),
                                static_cast<int>(fonte.dados.size()))) {
        JOGO_ERRO_SDL("SDL_PutAudioStreamData");
        return;
    }
    SDL_FlushAudioStream(voz.get());
    vozes_.push_back(Voz{std::move(voz), 0});
}

void Audio::atualizar() {
    constexpr Uint64 kCarenciaNs = 250'000'000;  // 250 ms
    const Uint64 agora = SDL_GetTicksNS();

    for (Voz& voz : vozes_) {
        if (voz.fimNs == 0 && SDL_GetAudioStreamAvailable(voz.fluxo.get()) <= 0) {
            voz.fimNs = agora;
        }
    }
    std::erase_if(vozes_, [&](const Voz& voz) {
        return voz.fimNs != 0 && agora - voz.fimNs > kCarenciaNs;
    });
}

void Audio::definirVolume(float v) {
    volume_ = std::clamp(v, 0.0f, 1.0f);
}

}  // namespace jogo

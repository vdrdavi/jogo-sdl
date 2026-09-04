#include "audio/Audio.hpp"

#include <algorithm>
#include <cmath>

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
    suspenso_ = false;
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
    iniciarVoz(som, ganho, false);
}

Audio::VozId Audio::tocarEmLoop(SomId som, float ganho) {
    return iniciarVoz(som, ganho, true);
}

Audio::VozId Audio::iniciarVoz(SomId som, float ganho, bool loop) {
    if (!ativo() || som == 0 || som >= sons_.size()) {
        return 0;
    }
    atualizar();
    if (vozes_.size() >= static_cast<std::size_t>(kMaxVozes)) {
        // Voz mais antiga cede lugar, para um efeito novo nunca ser engolido --
        // menos os loops, que sao ambientes de cena e nao podem sumir sozinhos.
        const auto vitima =
            std::find_if(vozes_.begin(), vozes_.end(), [](const Voz& v) { return !v.loop; });
        if (vitima == vozes_.end()) {
            return 0;
        }
        vozes_.erase(vitima);
    }

    const Som& fonte = sons_[som];
    AudioStreamPtr fluxo{SDL_CreateAudioStream(&fonte.spec, nullptr)};
    if (!fluxo) {
        JOGO_ERRO_SDL("SDL_CreateAudioStream");
        return 0;
    }
    if (!SDL_BindAudioStream(dispositivo_, fluxo.get())) {
        JOGO_ERRO_SDL("SDL_BindAudioStream");
        return 0;
    }

    Voz voz;
    voz.fluxo = std::move(fluxo);
    voz.id = proximoId_++;
    voz.som = som;
    voz.loop = loop;
    voz.ganho = std::clamp(ganho, 0.0f, 1.0f);
    aplicarGanho(voz);

    if (!SDL_PutAudioStreamData(voz.fluxo.get(), fonte.dados.data(),
                                static_cast<int>(fonte.dados.size()))) {
        JOGO_ERRO_SDL("SDL_PutAudioStreamData");
        return 0;
    }
    if (!loop) {
        // Um loop nunca e descarregado: o flush avisa o fim do sinal e faria o
        // reamostrador zerar o estado a cada volta, marcando a emenda.
        SDL_FlushAudioStream(voz.fluxo.get());
    }

    const VozId id = voz.id;
    vozes_.push_back(std::move(voz));
    return id;
}

Audio::Voz* Audio::procurar(VozId voz) {
    if (voz == 0) {
        return nullptr;
    }
    const auto it = std::find_if(vozes_.begin(), vozes_.end(),
                                 [voz](const Voz& v) { return v.id == voz; });
    return it == vozes_.end() ? nullptr : &*it;
}

void Audio::aplicarGanho(const Voz& voz) const {
    SDL_SetAudioStreamGain(voz.fluxo.get(), voz.ganho * volume_);
}

void Audio::ajustarGanho(VozId voz, float ganho) {
    Voz* alvo = procurar(voz);
    // Uma voz em fade de saida nao ressuscita: quem a parou ja seguiu adiante.
    if (alvo == nullptr || alvo->encerrando) {
        return;
    }
    alvo->ganho = std::clamp(ganho, 0.0f, 1.0f);
    aplicarGanho(*alvo);
}

void Audio::parar(VozId voz, float segundos) {
    Voz* alvo = procurar(voz);
    if (alvo == nullptr) {
        return;
    }
    alvo->encerrando = true;
    alvo->taxaFade = segundos > 0.0f ? alvo->ganho / segundos : 0.0f;
    if (alvo->taxaFade <= 0.0f) {
        alvo->ganho = 0.0f;
        aplicarGanho(*alvo);
    }
}

void Audio::atualizar() {
    constexpr Uint64 kCarenciaNs = 250'000'000;  // 250 ms
    const Uint64 agora = SDL_GetTicksNS();
    // O fade de saida anda em tempo real: ele nao passa pelo passo fixo da
    // simulacao -- a cena que o pediu ja saiu da pilha.
    const float dt = ultimaAtualizacaoNs_ == 0
                         ? 0.0f
                         : static_cast<float>(agora - ultimaAtualizacaoNs_) * 1e-9f;
    ultimaAtualizacaoNs_ = agora;

    // Suspenso, o relogio segue anotado acima (para o dt nao dar um salto do
    // tamanho da pausa ao voltar), mas nada mais anda: fade parado, carencia
    // parada, loop sem reabastecer -- o dispositivo nao esta consumindo fluxo
    // nenhum, entao recolher uma voz aqui jogaria fora um som que ainda nao
    // tocou.
    if (suspenso_) {
        return;
    }

    for (Voz& voz : vozes_) {
        if (voz.encerrando && voz.ganho > 0.0f) {
            voz.ganho = std::max(0.0f, voz.ganho - voz.taxaFade * dt);
            aplicarGanho(voz);
        }
        if (voz.loop) {
            reabastecer(voz);
        } else if (voz.fimNs == 0 && SDL_GetAudioStreamAvailable(voz.fluxo.get()) <= 0) {
            voz.fimNs = agora;
        }
    }
    std::erase_if(vozes_, [&](const Voz& voz) {
        if (voz.encerrando && voz.ganho <= 0.0f) {
            return true;
        }
        return voz.fimNs != 0 && agora - voz.fimNs > kCarenciaNs;
    });
}

void Audio::suspender() {
    if (!ativo() || suspenso_) {
        return;
    }
    suspenso_ = true;
    SDL_PauseAudioDevice(dispositivo_);
}

void Audio::retomar() {
    if (!ativo() || !suspenso_) {
        return;
    }
    suspenso_ = false;
    SDL_ResumeAudioDevice(dispositivo_);
}

void Audio::reabastecer(Voz& voz) {
    const Som& fonte = sons_[voz.som];
    if (fonte.dados.empty()) {
        return;
    }
    // Meio segundo de folga: fundo suficiente para o dispositivo nunca raspar o
    // fim da fila, e raso o bastante para o SDL nao guardar copias demais.
    const int minimo = SDL_AUDIO_FRAMESIZE(fonte.spec) * fonte.spec.freq / 2;
    while (SDL_GetAudioStreamQueued(voz.fluxo.get()) < minimo) {
        if (!SDL_PutAudioStreamData(voz.fluxo.get(), fonte.dados.data(),
                                    static_cast<int>(fonte.dados.size()))) {
            JOGO_ERRO_SDL("SDL_PutAudioStreamData (loop)");
            return;
        }
    }
}

void Audio::definirVolume(float v) {
    volume_ = std::clamp(v, 0.0f, 1.0f);
    // Um efeito curto acaba antes de a mudanca importar, mas um loop de ambiente
    // atravessa a troca de volume e precisa segui-la.
    for (const Voz& voz : vozes_) {
        aplicarGanho(voz);
    }
}

}  // namespace jogo

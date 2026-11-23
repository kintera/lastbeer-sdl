#include "audio.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace lastbeer {
Mixer::Mixer(int output_rate) : output_rate_(output_rate) {
    if (output_rate <= 0) throw std::invalid_argument("Invalid audio output rate");
}
std::uint64_t Mixer::play(std::shared_ptr<const Sound> sound, bool loop, float volume) {
    if (!sound || sound->sample_rate <= 0 || sound->samples.empty() || !std::isfinite(volume) || volume < 0 || volume > 1)
        throw std::invalid_argument("Invalid sound or volume");
    std::lock_guard lock(mutex_);
    auto id = next_id_++;
    if (!id) throw std::overflow_error("Audio voice ID exhausted");
    voices_.push_back({id, std::move(sound), 0, loop, volume});
    return id;
}
void Mixer::stop(std::uint64_t id) {
    std::lock_guard lock(mutex_);
    std::erase_if(voices_, [id](const Voice &v) { return v.id == id; });
}
void Mixer::stop_all() {
    std::lock_guard lock(mutex_);
    voices_.clear();
}
bool Mixer::active(std::uint64_t id) const {
    std::lock_guard lock(mutex_);
    return std::any_of(voices_.begin(), voices_.end(), [id](const Voice &v) { return v.id == id; });
}
void Mixer::mix(std::span<float> output) {
    std::fill(output.begin(), output.end(), 0.0f);
    std::lock_guard lock(mutex_);
    for (auto &voice : voices_) {
        auto &samples = voice.sound->samples;
        double advance = double(voice.sound->sample_rate) / output_rate_;
        for (auto &out : output) {
            if (voice.position >= samples.size()) {
                if (!voice.loop) break;
                voice.position = std::fmod(voice.position, double(samples.size()));
            }
            out += samples[std::size_t(voice.position)] * voice.volume;
            voice.position += advance;
        }
    }
    std::erase_if(voices_, [](const Voice &v) { return !v.loop && v.position >= v.sound->samples.size(); });
    for (auto &sample : output) sample = std::clamp(sample, -1.0f, 1.0f);
}
AudioDevice::AudioDevice() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        throw std::runtime_error(std::string("SDL audio initialization failed: ") + SDL_GetError());
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq = 48000;
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, callback, this);
    if (!stream_ || !SDL_ResumeAudioStreamDevice(stream_)) {
        std::string error = SDL_GetError();
        if (stream_) SDL_DestroyAudioStream(stream_);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        throw std::runtime_error("Cannot open SDL audio output: " + error);
    }
}
AudioDevice::~AudioDevice() {
    SDL_DestroyAudioStream(stream_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}
void SDLCALL AudioDevice::callback(void *userdata, SDL_AudioStream *stream, int additional, int) {
    auto &mixer = static_cast<AudioDevice *>(userdata)->mixer_;
    std::array<float, 1024> buffer{};
    while (additional > 0) {
        auto samples = std::min<std::size_t>((std::size_t(additional) + sizeof(float) - 1) / sizeof(float), buffer.size());
        mixer.mix(std::span<float>(buffer.data(), samples));
        int bytes = int(samples * sizeof(float));
        if (!SDL_PutAudioStreamData(stream, buffer.data(), bytes)) break;
        additional -= bytes;
    }
}
}

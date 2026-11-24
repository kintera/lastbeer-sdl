#pragma once
#include "sound.h"
#include <SDL3/SDL.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace lastbeer {
// Mono software mixer. Samples are owned by their voices, so they remain
// valid even if a caller unloads an archive immediately after playback starts.
class Mixer {
public:
    explicit Mixer(int output_rate = 48000);
    std::uint64_t play(std::shared_ptr<const Sound> sound, bool loop = false, float volume = 1.0f);
    void stop(std::uint64_t id);
    void stop_all();
    bool active(std::uint64_t id) const;
    void mix(std::span<float> output);
private:
    struct Voice {
        std::uint64_t id;
        std::shared_ptr<const Sound> sound;
        double position;
        bool loop;
        float volume;
    };
    int output_rate_;
    float limiter_gain_ = 1.0f;
    float limiter_release_;
    std::uint64_t next_id_ = 1;
    mutable std::mutex mutex_;
    std::vector<Voice> voices_;
};

// SDL owns the playback callback thread. AudioDevice must outlive callbacks;
// destruction closes the stream before releasing the mixer.
class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();
    AudioDevice(const AudioDevice &) = delete;
    AudioDevice &operator=(const AudioDevice &) = delete;
    Mixer &mixer() { return mixer_; }
private:
    static void SDLCALL callback(void *userdata, SDL_AudioStream *stream, int additional, int total);
    Mixer mixer_;
    SDL_AudioStream *stream_ = nullptr;
};
}

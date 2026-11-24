#include "sound.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lastbeer {
namespace {
unsigned le16(std::span<const std::uint8_t> b, std::size_t p) {
    return b[p] | (unsigned(b[p + 1]) << 8);
}
std::uint32_t le32(std::span<const std::uint8_t> b, std::size_t p) {
    return std::uint32_t(b[p]) | (std::uint32_t(b[p + 1]) << 8) |
           (std::uint32_t(b[p + 2]) << 16) | (std::uint32_t(b[p + 3]) << 24);
}
float normalized(int value) { return (value - 128) / 128.0f; }
}
Sound make_menu_sound(MenuSound effect) {
    constexpr int rate = 24000;
    const float duration = effect == MenuSound::Move ? 0.045f : 0.12f;
    const int length = int(rate * duration);
    Sound result{rate, 0, std::vector<float>(length)};
    float phase = 0;
    for (int i = 0; i < length; ++i) {
        const float t = float(i) / rate;
        const float frequency = effect == MenuSound::Move ? 620.0f :
            effect == MenuSound::Select ? (t < 0.055f ? 520.0f : 780.0f) :
                                        (t < 0.055f ? 620.0f : 400.0f);
        phase += 6.28318530718f * frequency / rate;
        // Short fades keep the notes soft and prevent clicks at either end.
        const float envelope = std::min({1.0f, t / 0.004f, (duration - t) / 0.025f});
        result.samples[i] = 0.35f * envelope * std::sin(phase);
    }
    result.samples.back() = 0.0f;
    return result;
}
Sound decode_sound(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 10) throw std::runtime_error("Truncated sound header");
    unsigned rate = le16(bytes, 2), flags = le16(bytes, 4);
    auto length = le32(bytes, 6);
    if (!rate || rate > 192 || (flags & ~1u) || length == 0 || length > bytes.size() - 10)
        throw std::runtime_error("Invalid sound record");
    Sound result{int(rate * 1000), int(le16(bytes, 0)), {}};
    auto data = bytes.subspan(10, length);
    if (!(flags & 1)) {
        result.samples.reserve(length);
        for (auto byte : data) result.samples.push_back(normalized(byte));
        return result;
    }
    result.samples.reserve(1 + 2 * (std::size_t(length) - 1));
    int value = data[0], step = 0;
    result.samples.push_back(normalized(value));
    for (std::size_t i = 1; i < data.size(); ++i) {
        for (int shift : {4, 0}) {
            int nibble = (data[i] >> shift) & 15;
            int delta = nibble & 7;
            value = std::clamp(value + ((nibble & 8) ? -1 : 1) * (delta << step), 0, 255);
            result.samples.push_back(normalized(value));
            if (delta >= 5) ++step;
            else if (delta == 0) --step;
            step = std::clamp(step, 0, 3);
        }
    }
    return result;
}
std::vector<Sound> decode_sound_library(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 2) throw std::runtime_error("Truncated sound library");
    const auto count = le16(bytes, 0);
    if (count > (bytes.size() - 2) / 4) throw std::runtime_error("Truncated sound library directory");
    const std::size_t directory_end = 2 + std::size_t(count) * 4;
    std::vector<Sound> sounds;
    sounds.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        auto entry = 2 + std::size_t(i) * 4;
        // The old DOS directory stores a segment:offset relative to byte 2.
        std::size_t offset = 2 + le16(bytes, entry) + 16 * std::size_t(le16(bytes, entry + 2));
        if (offset < directory_end || offset > bytes.size())
            throw std::runtime_error("Invalid sound library offset");
        sounds.push_back(decode_sound(bytes.subspan(offset)));
    }
    return sounds;
}

}

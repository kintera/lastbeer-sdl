#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace lastbeer {
struct Sound {
    int sample_rate;
    int priority;
    std::vector<float> samples; // mono, normalized to approximately [-1, 1]
};
// Decode an individual original 10-byte-header SND record, including packed4.
// Level*.SND entries are libraries containing multiple such records.
Sound decode_sound(std::span<const std::uint8_t> bytes);
std::vector<Sound> decode_sound_library(std::span<const std::uint8_t> bytes);
}

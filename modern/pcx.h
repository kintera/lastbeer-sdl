#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace lastbeer {
struct Color { std::uint8_t r, g, b; };
struct IndexedImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
    std::array<Color, 256> palette{};
};

// Decode the 8-bit, single-plane PCX files used by the game's data archive.
// Throws std::runtime_error if the file is invalid or uses another format.
IndexedImage decode_pcx(std::span<const std::uint8_t> bytes);
}

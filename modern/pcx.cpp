#include "pcx.h"

#include <stdexcept>

namespace lastbeer {
namespace {
unsigned le16(std::span<const std::uint8_t> b, std::size_t offset) {
    return b[offset] | (unsigned(b[offset + 1]) << 8);
}
}

IndexedImage decode_pcx(std::span<const std::uint8_t> bytes) {
    constexpr std::size_t header = 128, palette_size = 769;
    if (bytes.size() < header + palette_size || bytes[0] != 10 || bytes[2] != 1 ||
        bytes[3] != 8 || bytes[65] != 1 || bytes[bytes.size() - palette_size] != 12)
        throw std::runtime_error("Unsupported or truncated 8-bit PCX image");

    const auto xmin = le16(bytes, 4), ymin = le16(bytes, 6);
    const auto xmax = le16(bytes, 8), ymax = le16(bytes, 10);
    if (xmax < xmin || ymax < ymin) throw std::runtime_error("Invalid PCX dimensions");
    const unsigned width = xmax - xmin + 1, height = ymax - ymin + 1;
    const unsigned stride = le16(bytes, 66);
    // Avoid unbounded allocations when reading damaged or untrusted data.
    if (width > 4096 || height > 4096 || stride < width || stride > 8192)
        throw std::runtime_error("Invalid PCX row size");
    IndexedImage image;
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.pixels.resize(std::size_t(width) * height);
    std::size_t pos = header;
    const auto end = bytes.size() - palette_size;
    for (unsigned y = 0; y < height; ++y) {
        unsigned x = 0;
        while (x < stride) {
            if (pos >= end) throw std::runtime_error("Truncated PCX pixels");
            auto value = bytes[pos++];
            unsigned count = 1;
            if ((value & 0xc0) == 0xc0) {
                count = value & 0x3f;
                if (!count || pos >= end) throw std::runtime_error("Invalid PCX run");
                value = bytes[pos++];
            }
            if (count > stride - x) throw std::runtime_error("PCX run exceeds row");
            for (unsigned i = 0; i < count; ++i, ++x)
                if (x < width) image.pixels[std::size_t(y) * width + x] = value;
        }
    }
    for (std::size_t i = 0; i < image.palette.size(); ++i) {
        auto p = bytes.size() - 768 + i * 3;
        image.palette[i] = {bytes[p], bytes[p + 1], bytes[p + 2]};
    }
    return image;
}
}

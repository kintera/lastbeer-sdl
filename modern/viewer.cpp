#include "fileman.h"
#include "pcx.h"
#include "video.h"
#include "audio.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

namespace {
void archive_error(char *where, int code, ...) {
    std::fprintf(stderr, "%s: archive error %d\n", where, code);
}
struct DatabaseCloser { void operator()(void *p) const { if (p) closedatabase(p); } };
struct DataCloser { void operator()(unsigned char *p) const { unloadfile(p); } };
}
int main(int argc, char **argv) {
    if (argc > 1 && std::strcmp(argv[1], "--help") == 0) {
        std::puts("Usage: beer-viewer [BEER.DAT path] [asset.PCX] [--sound asset.SND] [--frames N]\n"
                  "View original game artwork; optionally loop a standalone sound.\n"
                  "Escape closes; Alt+Enter toggles fullscreen.");
        return 0;
    }
    const char *path = "BEER.DAT", *name = "TITLE.PCX", *sound_name = nullptr;
    unsigned long frames = 0;
    int arg = 1;
    if (arg < argc && std::strncmp(argv[arg], "--", 2) != 0) path = argv[arg++];
    if (arg < argc && std::strncmp(argv[arg], "--", 2) != 0) name = argv[arg++];
    while (arg < argc) {
        if (std::strcmp(argv[arg], "--sound") == 0 && arg + 1 < argc) {
            sound_name = argv[arg + 1];
            arg += 2;
        } else if (std::strcmp(argv[arg], "--frames") == 0 && arg + 1 < argc) {
            char *end = nullptr;
            errno = 0;
            const char *value = argv[arg + 1];
            frames = std::strtoul(value, &end, 10);
            if (errno || !*value || *value == '-' || *end || !frames) {
                std::fprintf(stderr, "Frame count must be a positive integer.\n");
                return 2;
            }
            arg += 2;
        } else {
            std::fprintf(stderr, "Invalid arguments; use --help for usage.\n");
            return 2;
        }
    }
    if (initfilemanager(40, 512, 8192, archive_error) != 1) {
        std::fprintf(stderr, "Cannot initialize archive manager\n");
        return 1;
    }
    int result = 0;
    {
        std::unique_ptr<void, DatabaseCloser> db(opendatabase(const_cast<char *>(path)));
        std::size_t size = 0;
        if (!db || !database_file_size(db.get(), name, &size)) {
            std::fprintf(stderr, "Cannot find %s in %s\n", name, path);
            result = 1;
        } else {
            std::unique_ptr<unsigned char, DataCloser> data(loadfile(db.get(), const_cast<char *>(name)));
            if (!data) result = 1;
            else try {
                auto image = lastbeer::decode_pcx(std::span<const std::uint8_t>(data.get(), size));
                lastbeer::Video video("The Last Eichhof - SDL asset viewer", image.width, image.height);
                std::unique_ptr<lastbeer::AudioDevice> audio;
                if (sound_name) {
                    std::size_t sound_size = 0;
                    if (!database_file_size(db.get(), sound_name, &sound_size))
                        throw std::runtime_error(std::string("Cannot find sound: ") + sound_name);
                    std::unique_ptr<unsigned char, DataCloser> sound_data(loadfile(db.get(), const_cast<char *>(sound_name)));
                    if (!sound_data) throw std::runtime_error("Cannot load sound");
                    auto sound = std::make_shared<lastbeer::Sound>(
                        lastbeer::decode_sound(std::span<const std::uint8_t>(sound_data.get(), sound_size)));
                    audio = std::make_unique<lastbeer::AudioDevice>();
                    audio->mixer().play(sound, true);
                }
                unsigned long shown = 0;
                while (video.poll() && !video.input().pressed(SDL_SCANCODE_ESCAPE)) {
                    video.present(image.pixels, image.palette);
                    if (frames && ++shown >= frames) break;
                    SDL_Delay(16);
                }
            } catch (const std::exception &e) {
                std::fprintf(stderr, "%s\n", e.what());
                result = 1;
            }
        }
    }
    shutfilemanager();
    return result;
}

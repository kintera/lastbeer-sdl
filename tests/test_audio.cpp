#include "audio.h"
#include "fileman.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>

static void check(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
static void archive_error(char *, int, ...) {}
static bool equal(float a, float b) { return std::abs(a - b) < 0.00001f; }
int main(int argc, char **argv) {
    // Packed nibble values: seed=128; +5 (step rises), -2, +0, +1.
    const std::array<std::uint8_t, 13> packed = {5,0,15,0,1,0,3,0,0,0,128,0x59,0x01};
    auto decoded = lastbeer::decode_sound(packed);
    const std::array expected = {0.0f, 5/128.0f, 3/128.0f, 3/128.0f, 4/128.0f};
    check(decoded.sample_rate == 15000 && decoded.priority == 5 && decoded.samples.size() == expected.size(), "invalid packed sound metadata");
    for (std::size_t i = 0; i < expected.size(); ++i) check(equal(decoded.samples[i], expected[i]), "invalid packed sound data");
    bool rejected = false;
    try { lastbeer::decode_sound(std::span(packed).first(12)); } catch (const std::exception &) { rejected = true; }
    check(rejected, "truncated sound accepted");

    std::array<lastbeer::Sound, 3> menu_sounds = {
        lastbeer::make_menu_sound(lastbeer::MenuSound::Move),
        lastbeer::make_menu_sound(lastbeer::MenuSound::Select),
        lastbeer::make_menu_sound(lastbeer::MenuSound::Back)};
    for (const auto &effect : menu_sounds) {
        check(!effect.samples.empty() && effect.samples.size() < std::size_t(effect.sample_rate / 4), "menu sound duration invalid");
        check(equal(effect.samples.front(), 0) && equal(effect.samples.back(), 0), "menu sound does not fade to silence");
        bool audible = false;
        for (float sample : effect.samples) {
            check(std::isfinite(sample) && std::abs(sample) <= 1, "invalid menu sound sample");
            audible |= std::abs(sample) > 0.01f;
        }
        check(audible, "menu sound is silent");
    }
    check(menu_sounds[1].samples != menu_sounds[2].samples, "select and back sounds should differ");

    lastbeer::Mixer mixer(4);
    auto sound = std::make_shared<lastbeer::Sound>(lastbeer::Sound{2, 0, {0.25f, -0.5f}});
    auto id = mixer.play(sound);
    sound.reset(); // playback owns its sample data
    std::array<float, 5> output;
    mixer.mix(output);
    check(equal(output[0], .25f) && equal(output[1], .25f) && equal(output[2], -.5f) &&
          equal(output[3], -.5f) && equal(output[4], 0) && !mixer.active(id), "incorrect resampling or voice lifetime");
    auto loop = std::make_shared<lastbeer::Sound>(lastbeer::Sound{4, 0, {0.8f}});
    id = mixer.play(loop, true);
    mixer.play(loop, true);
    mixer.mix(output);
    check(equal(output[0], .98f) && equal(output[4], .98f), "overlapping sounds were not limited");
    mixer.stop(id); mixer.stop_all(); mixer.mix(output);
    check(equal(output[0], 0) && !mixer.active(id), "voice stop failed");

    lastbeer::Mixer repeated(4);
    auto burst = std::make_shared<lastbeer::Sound>(lastbeer::Sound{4, 0, {0.1f, 0.2f, 0.3f, 0.4f}});
    const auto earlier = repeated.play(burst);
    std::array<float, 2> pair;
    repeated.mix(pair);
    const auto later = repeated.play(burst);
    repeated.mix(pair);
    check(equal(pair[0], .4f) && equal(pair[1], .6f) && !repeated.active(earlier) && repeated.active(later),
          "repeated sounds did not play at their own start times");

    lastbeer::Mixer limiter(1000);
    auto loud = std::make_shared<lastbeer::Sound>(lastbeer::Sound{1000, 0, {0.8f}});
    auto first = limiter.play(loud, true);
    auto second = limiter.play(loud, true);
    std::array<float, 1> peak;
    limiter.mix(peak);
    check(equal(peak[0], .98f), "limiter did not prevent a clipped peak");
    limiter.stop(second);
    std::array<float, 1000> recovery;
    limiter.mix(recovery);
    check(recovery.front() < .6f && recovery.back() > .79f, "limiter did not recover smoothly");
    limiter.stop_all();
    first = limiter.play(loud, true);
    limiter.mix(peak);
    check(equal(peak[0], .8f) && limiter.active(first), "ordinary sound volume changed");

    check(argc == 2 && initfilemanager(1, 0, 8192, archive_error) == 1, "archive init failed");
    void *db = opendatabase(argv[1]); check(db != nullptr, "archive open failed");
    for (auto name : {"CLOSE.SND", "BLICK.SND", "GO.SND", "MENU.SND", "SELL.SND", "BUY.SND",
                      "TOD.SND", "LONGTIME.SND", "HS.SND", "TITLE.SND",
                      "LEVEL0.SND", "LEVEL1.SND", "LEVEL2.SND", "LEVEL3.SND", "LEVEL4.SND"}) {
        std::size_t size = 0;
        check(database_file_size(db, name, &size), "sound size missing");
        auto *data = loadfile(db, const_cast<char *>(name)); check(data != nullptr, "sound missing");
        try {
            auto bytes = std::span<const std::uint8_t>(data, size);
            if (name[0] == 'L' && name[1] == 'E') {
                auto library = lastbeer::decode_sound_library(bytes);
                check(!library.empty(), "empty sound library");
            } else check(!lastbeer::decode_sound(bytes).samples.empty(), "empty decoded sound");
        } catch (const std::exception &e) { std::fprintf(stderr, "%s: %s\n", name, e.what()); std::exit(1); }
        unloadfile(data);
    }
    closedatabase(db); shutfilemanager();
    {
        lastbeer::AudioDevice device;
        auto voice = device.mixer().play(loop, true);
        auto one_shot = device.mixer().play(loop);
        for (int i = 0; i < 100 && device.mixer().active(one_shot); ++i) SDL_Delay(10);
        check(!device.mixer().active(one_shot), "SDL audio callback did not consume samples");
        check(device.mixer().active(voice), "SDL audio loop not active");
        device.mixer().stop_all();
    }
}

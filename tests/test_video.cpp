#include "video.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <vector>

static void check(bool condition, const char *message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
static bool near(float a, float b) { return std::abs(a - b) < 0.01f; }
static void key_down(SDL_Scancode key, SDL_Keymod mod = SDL_KMOD_NONE) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = key;
    event.key.mod = mod;
    check(SDL_PushEvent(&event), "could not enqueue shortcut");
}
int main() {
    for (const auto &[width, height, expected_w, expected_h] : {
             std::array{1920, 1080, 1440, 1080}, std::array{3840, 2160, 2880, 2160},
             std::array{1366, 768, 1024, 768}, std::array{640, 480, 640, 480}}) {
        const auto sizes = lastbeer::window_sizes_for_display(width, height);
        check(sizes.size() >= 2 && sizes.front() == std::pair{0, 0} &&
              sizes.back() == std::pair{expected_w, expected_h}, "incorrect monitor-aware size limit");
        for (std::size_t i = 1; i < sizes.size(); ++i)
            check(sizes[i].first <= width && sizes[i].second <= height &&
                  sizes[i].first * 3 == sizes[i].second * 4 && sizes[i - 1].first < sizes[i].first,
                  "window sizes are invalid or out of order");
    }
    const std::array<std::uint32_t, 9> pattern = {0, 1, 0, 1, 0, 0, 0, 0, 0};
    std::array<std::uint32_t, 36> scaled{};
    lastbeer::scale2x(pattern, 3, 3, scaled);
    check(scaled[2 * 6 + 2] == 1 && scaled[2 * 6 + 3] == 0 &&
          scaled[3 * 6 + 2] == 0 && scaled[3 * 6 + 3] == 0,
          "Scale2x did not smooth the corner as expected");
    lastbeer::Video video("display test", 640, 480, 2);
    const auto window = video.window_id();
    check(window != 0, "missing window");
    std::array<lastbeer::Color, 256> palette{};
    palette[1] = {255, 0, 0};
    video.set_frame_size(640, 350);
    auto rect = video.frame_destination();
    check(near(rect.x, 0) && near(rect.y, 65) && near(rect.w, 640) && near(rect.h, 350),
          "intro should be centered with its aspect preserved");
    video.present(std::vector<std::uint8_t>(640 * 350, 1), palette);
    video.set_frame_size(320, 240);
    rect = video.frame_destination();
    check(near(rect.x, 0) && near(rect.y, 0) && near(rect.w, 640) && near(rect.h, 480),
          "game should fill the 4:3 canvas");
    video.present(std::vector<std::uint8_t>(320 * 240, 1), palette);
    video.set_smooth(true);
    check(video.smooth(), "smooth filtering not enabled");
    video.set_frame_size(640, 350);
    check(video.smooth(), "filter lost on framebuffer change");
    video.set_smooth(false);
    check(!video.smooth(), "pixel filtering not restored");
    video.set_filter(2);
    video.set_frame_size(320, 240);
    video.present(std::vector<std::uint8_t>(320 * 240, 1), palette);
    check(video.filter() == 2, "Scale2x filtering lost on framebuffer change");
    video.set_integer_scaling(true);
    video.set_scanlines(true);
    video.present(std::vector<std::uint8_t>(320 * 240, 1), palette);
    check(video.integer_scaling() && video.scanlines(), "display effects not enabled");
    video.set_integer_scaling(false);
    video.set_scaling(2);
    check(video.scaling() == 2 && !video.integer_scaling(), "stretch scaling not enabled");
    video.present(std::vector<std::uint8_t>(320 * 240, 1), palette);
    video.set_scaling(0);
    video.set_scanlines(false);
    video.set_filter(0);
    video.present(std::vector<std::uint8_t>(320 * 240, 1), palette);
    char *base = SDL_GetPrefPath("", "lastbeer");
    check(base != nullptr, "missing screenshot preference directory");
    std::filesystem::path screenshots = std::filesystem::path(base) / "screenshots";
    SDL_free(base);
    auto list_screenshots = [&]() {
        std::set<std::filesystem::path> paths;
        if (std::filesystem::exists(screenshots))
            for (const auto &entry : std::filesystem::directory_iterator(screenshots))
                if (entry.path().extension() == ".bmp") paths.insert(entry.path());
        return paths;
    };
    const auto previous_screenshots = list_screenshots();
    key_down(SDL_SCANCODE_S, SDL_KMOD_LALT);
    check(video.poll() && !video.input().pressed(SDL_SCANCODE_S), "screenshot shortcut leaked into game input");
    const auto current_screenshots = list_screenshots();
    check(current_screenshots.size() == previous_screenshots.size() + 1, "screenshot shortcut did not save an image");
    SDL_Surface *captured = nullptr;
    for (const auto &path : current_screenshots)
        if (!previous_screenshots.contains(path)) { captured = SDL_LoadBMP(path.string().c_str()); break; }
    check(captured && captured->w > 0 && captured->h > 0, "screenshot is unreadable");
    Uint8 red = 0, green = 0, blue = 0;
    check(SDL_ReadSurfacePixel(captured, captured->w / 2, captured->h / 2, &red, &green, &blue, nullptr) &&
          red > 200 && green < 10 && blue < 10, "screenshot did not contain the displayed game image");
    SDL_DestroySurface(captured);
    palette[2] = {100, 100, 100};
    video.set_brightness(120);
    video.present(std::vector<std::uint8_t>(320 * 240, 2), palette);
    const auto brightness_image = screenshots / "brightness-test.bmp";
    check(video.save_screenshot(brightness_image.string().c_str()), "brightness screenshot failed");
    captured = SDL_LoadBMP(brightness_image.string().c_str());
    check(captured && SDL_ReadSurfacePixel(captured, captured->w / 2, captured->h / 2, &red, &green, &blue, nullptr) &&
          red == 120 && green == 120 && blue == 120, "brightness did not affect displayed pixels");
    SDL_DestroySurface(captured);
    video.set_brightness(100);
    video.set_contrast(120);
    video.present(std::vector<std::uint8_t>(320 * 240, 2), palette);
    check(video.save_screenshot(brightness_image.string().c_str()), "contrast screenshot failed");
    captured = SDL_LoadBMP(brightness_image.string().c_str());
    check(captured && SDL_ReadSurfacePixel(captured, captured->w / 2, captured->h / 2, &red, &green, &blue, nullptr) &&
          red == 95 && green == 95 && blue == 95, "contrast did not affect displayed pixels");
    SDL_DestroySurface(captured);
    video.set_contrast(100);
    video.set_brightness(0);
    video.present(std::vector<std::uint8_t>(320 * 240, 2), palette);
    check(video.save_screenshot(brightness_image.string().c_str()), "zero brightness screenshot failed");
    captured = SDL_LoadBMP(brightness_image.string().c_str());
    check(captured && SDL_ReadSurfacePixel(captured, captured->w / 2, captured->h / 2, &red, nullptr, nullptr, nullptr) && red == 0,
          "zero brightness did not produce black");
    SDL_DestroySurface(captured);
    palette[15] = {255, 255, 255};
    std::vector<std::uint8_t> controls(320 * 240, 2);
    for (int y = 0; y < 240; ++y) std::fill_n(controls.data() + y * 320, 160, 15);
    video.set_clarity_controls(true);
    video.present(controls, palette);
    check(video.save_screenshot(brightness_image.string().c_str()), "settings visibility screenshot failed");
    captured = SDL_LoadBMP(brightness_image.string().c_str());
    check(captured && SDL_ReadSurfacePixel(captured, captured->w / 4, captured->h / 2, &red, nullptr, nullptr, nullptr) && red == 255,
          "settings text was hidden at zero brightness");
    check(SDL_ReadSurfacePixel(captured, captured->w * 3 / 4, captured->h / 2, &red, nullptr, nullptr, nullptr) && red == 0,
          "zero brightness did not preview on artwork");
    SDL_DestroySurface(captured);
    video.set_clarity_controls(false);
    video.set_brightness(150);
    video.present(std::vector<std::uint8_t>(320 * 240, 2), palette);
    check(video.save_screenshot(brightness_image.string().c_str()), "maximum brightness screenshot failed");
    captured = SDL_LoadBMP(brightness_image.string().c_str());
    check(captured && SDL_ReadSurfacePixel(captured, captured->w / 2, captured->h / 2, &red, nullptr, nullptr, nullptr) && red == 150,
          "maximum brightness did not reach 150 percent");
    SDL_DestroySurface(captured);
    video.set_brightness(100);
    video.set_contrast(0);
    video.present(std::vector<std::uint8_t>(320 * 240, 2), palette);
    check(video.save_screenshot(brightness_image.string().c_str()), "zero contrast screenshot failed");
    captured = SDL_LoadBMP(brightness_image.string().c_str());
    check(captured && SDL_ReadSurfacePixel(captured, captured->w / 2, captured->h / 2, &red, nullptr, nullptr, nullptr) && red == 128,
          "zero contrast did not produce midpoint gray");
    SDL_DestroySurface(captured);
    video.set_contrast(150);
    video.present(std::vector<std::uint8_t>(320 * 240, 2), palette);
    check(video.save_screenshot(brightness_image.string().c_str()), "maximum contrast screenshot failed");
    captured = SDL_LoadBMP(brightness_image.string().c_str());
    check(captured && SDL_ReadSurfacePixel(captured, captured->w / 2, captured->h / 2, &red, nullptr, nullptr, nullptr) && red == 86,
          "maximum contrast did not reach 150 percent");
    SDL_DestroySurface(captured);
    video.set_contrast(100);
    video.set_scanline_strength(2);
    video.present(std::vector<std::uint8_t>(320 * 240, 2), palette);
    check(video.save_screenshot(brightness_image.string().c_str()), "scanline screenshot failed");
    captured = SDL_LoadBMP(brightness_image.string().c_str());
    int low = 255, high = 0;
    check(captured != nullptr, "scanline screenshot is unreadable");
    for (int y = captured->h / 2; y < captured->h / 2 + 20; ++y) {
        check(SDL_ReadSurfacePixel(captured, captured->w / 2, y, &red, &green, &blue, nullptr), "scanline pixel unreadable");
        low = std::min(low, int(red)); high = std::max(high, int(red));
    }
    check(low < high && high == 100, "scanline strength did not affect displayed pixels");
    SDL_DestroySurface(captured);
    video.set_scanline_strength(0);
    std::filesystem::remove(brightness_image);
    video.set_brightness(100);
    if (video.set_vsync(true)) check(video.vsync(), "vertical sync state incorrect");
    video.set_vsync(false);
    check(video.window_id() == window, "mode change replaced the window");
    auto *native_window = SDL_GetWindowFromID(window);
    check(native_window && SDL_SetWindowSize(native_window, 1200, 700), "resize failed");
    check(video.poll() && video.window_id() == window, "resize replaced the window");
    key_down(SDL_SCANCODE_F11);
    check(video.poll() && video.fullscreen() && video.window_id() == window, "F11 fullscreen failed");
    key_down(SDL_SCANCODE_RETURN, SDL_KMOD_LALT);
    check(video.poll() && !video.fullscreen() && video.window_id() == window, "Alt+Enter windowed mode failed");
    check(!video.input().pressed(SDL_SCANCODE_RETURN), "fullscreen shortcut leaked into game input");

    // Attach an SDL virtual gamepad to exercise discovery and the real event path.
    SDL_VirtualJoystickDesc desc{};
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    desc.name = "Lastbeer test controller";
    auto id = SDL_AttachVirtualJoystick(&desc);
    check(id != 0, "could not attach virtual gamepad");
    check(video.poll(), "gamepad connection failed");
    check(video.controller_ids().size() == 1, "controller list did not update");
    video.select_controller(id);
    check(video.selected_controller_id() == id && !video.selected_controller_guid().empty(), "controller selection failed");
    auto *joystick = SDL_OpenJoystick(id);
    check(joystick != nullptr, "could not open virtual joystick");
    check(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true), "could not press gamepad button");
    check(video.poll() && video.input().pad_down(lastbeer::PadAction::Confirm), "connected gamepad button not detected");
    check(SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTX, 22000), "could not move gamepad stick");
    check(video.poll() && video.input().pad_down(lastbeer::PadAction::Right), "connected gamepad stick not detected");
    auto second = SDL_AttachVirtualJoystick(&desc);
    check(second != 0 && video.poll(), "second controller was not detected");
    auto *other = SDL_OpenJoystick(second);
    check(other != nullptr, "could not open second joystick");
    check(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, false), "could not release selected controller");
    check(SDL_SetJoystickVirtualButton(other, SDL_GAMEPAD_BUTTON_SOUTH, true), "could not press second controller");
    check(video.poll() && !video.input().pad_down(lastbeer::PadAction::Confirm), "unselected controller affected input");
    SDL_CloseJoystick(joystick);
    check(SDL_DetachVirtualJoystick(id), "could not detach virtual gamepad");
    check(video.poll() && !video.input().pad_down(lastbeer::PadAction::Confirm) &&
          !video.input().pad_down(lastbeer::PadAction::Right), "disconnected gamepad remained pressed");
    check(video.selected_controller_id() == 0 && !video.selected_controller_guid().empty(), "disconnected selection not retained");
    auto replacement = SDL_AttachVirtualJoystick(&desc);
    check(replacement != 0 && video.poll() && video.selected_controller_id() == replacement,
          "selected controller did not reconnect");
    SDL_CloseJoystick(other);
    check(SDL_DetachVirtualJoystick(second) && SDL_DetachVirtualJoystick(replacement), "could not detach controllers");
    video.poll();
    video.select_controller(0);
    check(video.selected_controller_guid().empty(), "auto controller selection failed");
}

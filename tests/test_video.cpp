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
    auto *icon = lastbeer::make_window_icon_surface();
    Uint8 ir = 0, ig = 0, ib = 0, ia = 0;
    check(icon && icon->w == 128 && icon->h == 128 &&
          SDL_ReadSurfacePixel(icon, 0, 0, &ir, &ig, &ib, &ia) && ia == 0 &&
          SDL_ReadSurfacePixel(icon, 64, 64, &ir, &ig, &ib, &ia) &&
          ir == 151 && ig == 17 && ib == 4 && ia == 253,
          "window icon pixels or transparency are incorrect");
    SDL_DestroySurface(icon);
    video.show_loading();
    auto *startup = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_RGBA32);
    auto *startup_renderer = startup ? SDL_CreateSoftwareRenderer(startup) : nullptr;
    check(startup_renderer && lastbeer::draw_startup_screen(startup_renderer, 640) &&
          SDL_ReadSurfacePixel(startup, 320, 174, &ir, &ig, &ib, &ia) &&
          ir > 100 && ig < 50 && ib < 50,
          "startup screen did not draw the game icon");
    SDL_DestroyRenderer(startup_renderer);
    SDL_DestroySurface(startup);
    // Draw the bars on a 16:9 target and make sure the 4:3 game area and
    // logical coordinate mapping remain untouched.
    auto *side_output = SDL_CreateSurface(960, 540, SDL_PIXELFORMAT_RGBA32);
    auto *side_renderer = side_output ? SDL_CreateSoftwareRenderer(side_output) : nullptr;
    auto *side_art = lastbeer::make_beer_sidebar_surface();
    auto *side_texture = side_renderer && side_art ? SDL_CreateTextureFromSurface(side_renderer, side_art) : nullptr;
    check(side_texture && SDL_SetTextureScaleMode(side_texture, SDL_SCALEMODE_NEAREST) &&
          SDL_SetRenderLogicalPresentation(side_renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX) &&
          SDL_SetRenderDrawColor(side_renderer, 0, 0, 0, 255) && SDL_RenderClear(side_renderer) &&
          lastbeer::draw_beer_sidebars(side_renderer, side_texture), "could not draw fullscreen beer sidebars");
    SDL_FRect side_rect{};
    SDL_RendererLogicalPresentation side_mode{};
    int logical_w = 0, logical_h = 0;
    check(SDL_GetRenderLogicalPresentationRect(side_renderer, &side_rect) &&
          SDL_GetRenderLogicalPresentation(side_renderer, &logical_w, &logical_h, &side_mode) &&
          near(side_rect.x, 120) && near(side_rect.w, 720) && logical_w == 640 && logical_h == 480 &&
          side_mode == SDL_LOGICAL_PRESENTATION_LETTERBOX, "sidebar rendering changed the game viewport");
    check(SDL_SetRenderDrawColor(side_renderer, 200, 0, 0, 255) && SDL_RenderFillRect(side_renderer, nullptr) &&
          SDL_RenderPresent(side_renderer), "could not render game over sidebars");
    Uint8 sr = 0, sg = 0, sb = 0;
    check(SDL_ReadSurfacePixel(side_output, 70, 270, &sr, &sg, &sb, nullptr) && sr > 120 && sr > sg && sg > sb,
          "left sidebar does not show amber beer");
    check(SDL_ReadSurfacePixel(side_output, 890, 270, &sr, &sg, &sb, nullptr) && sr > 120 && sr > sg && sg > sb,
          "right sidebar does not show amber beer");
    check(SDL_ReadSurfacePixel(side_output, 480, 270, &sr, &sg, &sb, nullptr) && sr == 200 && sg == 0 && sb == 0,
          "sidebar covered the game view");
    auto *full_capture = lastbeer::read_render_output(side_renderer);
    check(full_capture && full_capture->w == 960 && full_capture->h == 540 &&
          SDL_ReadSurfacePixel(full_capture, 70, 270, &sr, &sg, &sb, nullptr) && sr > 120 && sr > sg && sg > sb &&
          SDL_GetRenderLogicalPresentation(side_renderer, &logical_w, &logical_h, &side_mode) &&
          logical_w == 640 && logical_h == 480 && side_mode == SDL_LOGICAL_PRESENTATION_LETTERBOX,
          "screenshot omitted the beer sidebars or changed the game viewport");
    SDL_DestroySurface(full_capture);
    for (int y : {0, 539}) {
        for (int x : {60, 900})
            check(SDL_ReadSurfacePixel(side_output, x, y, &sr, &sg, &sb, nullptr) && sr == 0 && sg == 0 && sb == 0,
                  "sidebar background is not black");
        for (int x : {1, 118, 841, 958})
            check(SDL_ReadSurfacePixel(side_output, x, y, &sr, &sg, &sb, nullptr) && sr == 81 && sg == 42 && sb == 14,
                  "sidebar trim does not reach the screen edge");
    }
    check(SDL_SetRenderLogicalPresentation(side_renderer, 640, 480, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE) &&
          SDL_SetRenderDrawColor(side_renderer, 0, 0, 0, 255) && SDL_RenderClear(side_renderer) &&
          lastbeer::draw_beer_sidebars(side_renderer, side_texture) &&
          SDL_GetRenderLogicalPresentationRect(side_renderer, &side_rect) && near(side_rect.x, 160) &&
          SDL_SetRenderDrawColor(side_renderer, 200, 0, 0, 255) && SDL_RenderFillRect(side_renderer, nullptr) &&
          SDL_RenderPresent(side_renderer) &&
          SDL_ReadSurfacePixel(side_output, 70, 270, &sr, &sg, &sb, nullptr) && sr > 120 && sr > sg && sg > sb &&
          SDL_ReadSurfacePixel(side_output, 480, 270, &sr, &sg, &sb, nullptr) && sr == 200 && sg == 0 && sb == 0,
          "sidebars failed with integer scaling");
    check(SDL_SetRenderLogicalPresentation(side_renderer, 640, 480, SDL_LOGICAL_PRESENTATION_STRETCH) &&
          SDL_SetRenderDrawColor(side_renderer, 0, 0, 0, 255) && SDL_RenderClear(side_renderer) &&
          lastbeer::draw_beer_sidebars(side_renderer, side_texture) && SDL_RenderPresent(side_renderer) &&
          SDL_ReadSurfacePixel(side_output, 70, 270, &sr, &sg, &sb, nullptr) && sr == 0 && sg == 0 && sb == 0,
          "sidebars appeared when the game fills the screen");
    SDL_DestroyTexture(side_texture);
    SDL_DestroySurface(side_art);
    SDL_DestroyRenderer(side_renderer);
    SDL_DestroySurface(side_output);
    auto *cursor = lastbeer::make_beer_cursor_surface();
    Uint8 cr = 0, cg = 0, cb = 0, ca = 0;
    check(cursor && cursor->w == 32 && cursor->h == 40 &&
          SDL_ReadSurfacePixel(cursor, 8, 10, &cr, &cg, &cb, &ca) &&
          cr > 200 && cg > 100 && cb < 80 && ca == 255,
          "beer cursor was not rendered at its larger size");
    check(SDL_ReadSurfacePixel(cursor, 0, 0, &cr, &cg, &cb, &ca) && ca == 0,
          "beer cursor background was not transparent");
    SDL_DestroySurface(cursor);
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
    int initial_width = 0, initial_height = 0;
    check(SDL_GetWindowSize(SDL_GetWindowFromID(window), &initial_width, &initial_height), "cannot get window size");
    const auto mouse_center = video.window_to_frame(initial_width / 2.0f, initial_height / 2.0f);
    check(near(mouse_center.first, 160) && near(mouse_center.second, 120), "mouse position did not map to game canvas");
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
    video.set_fullscreen(true);
    check(video.fullscreen() && (SDL_GetWindowFlags(native_window) & SDL_WINDOW_FULLSCREEN),
          "fullscreen was not applied before returning");
    video.set_fullscreen(false);
    check(!video.fullscreen() && !(SDL_GetWindowFlags(native_window) & SDL_WINDOW_FULLSCREEN),
          "windowed mode was not applied before returning");
    check(video.poll(), "window events failed");
    check(SDL_SetWindowFullscreen(native_window, true) && SDL_SyncWindow(native_window) && video.poll() && video.fullscreen(),
          "fullscreen entered outside the game was not detected");
    check(SDL_SetWindowFullscreen(native_window, false) && SDL_SyncWindow(native_window) && video.poll() && !video.fullscreen(),
          "fullscreen exit outside the game was not detected");
    video.set_window_size(800, 600);
    int window_width = 0, window_height = 0;
    check(SDL_GetWindowSize(native_window, &window_width, &window_height) &&
          window_width == 800 && window_height == 600,
          "window size was not applied before returning");
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
    video.select_controller(second);
    const auto selected_guid = video.selected_controller_guid();
    const auto selected_identity = video.selected_controller_identity();
    video.select_controller_saved(selected_guid, selected_identity);
    check(video.selected_controller_id() == second, "identical controller selection did not survive reload");
    video.select_controller(id);
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

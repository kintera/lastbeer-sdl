#include "video.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include "window_icon_rgba.h"

namespace lastbeer {
namespace {
std::runtime_error sdl_error(const char *what) {
    return std::runtime_error(std::string(what) + ": " + SDL_GetError());
}
}
SDL_Surface *make_window_icon_surface() {
    auto *surface = SDL_CreateSurface(128, 128, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return nullptr;
    for (int y = 0; y < 128; ++y)
        std::memcpy(static_cast<unsigned char *>(surface->pixels) + y * surface->pitch,
                    lastbeer_window_icon_rgba + y * 128 * 4, 128 * 4);
    return surface;
}
void scale2x(std::span<const std::uint32_t> input, int width, int height,
             std::span<std::uint32_t> output) {
    if (width <= 0 || height <= 0 || input.size() != std::size_t(width) * height ||
        output.size() != input.size() * 4) throw std::invalid_argument("Invalid Scale2x buffer");
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto e = input[std::size_t(y) * width + x];
        const auto b = input[std::size_t(std::max(0, y-1)) * width + x];
        const auto d = input[std::size_t(y) * width + std::max(0, x-1)];
        const auto f = input[std::size_t(y) * width + std::min(width-1, x+1)];
        const auto h = input[std::size_t(std::min(height-1, y+1)) * width + x];
        auto *top = output.data() + std::size_t(y*2) * width*2 + x*2;
        auto *bottom = top + width*2;
        top[0] = d == b && d != h && b != f ? d : e;
        top[1] = b == f && b != d && f != h ? f : e;
        bottom[0] = d == h && d != b && h != f ? d : e;
        bottom[1] = h == f && d != h && b != f ? f : e;
    }
}
std::vector<std::pair<int, int>> window_sizes_for_display(int width, int height) {
    std::vector<std::pair<int, int>> sizes{{0, 0}}; // Automatic window size.
    if (width <= 0 || height <= 0) return sizes;
    const int units = std::min(width / 4, height / 3);
    if (units <= 0) return sizes;
    const int max_width = units * 4;
    constexpr int widths[] = {640, 800, 960, 1024, 1152, 1280, 1440, 1600,
                              1920, 2048, 2560, 2880, 3200, 3840, 4096, 5120, 5760, 7680, 8192};
    for (int w : widths) if (w <= max_width) sizes.emplace_back(w, w / 4 * 3);
    if (sizes.size() == 1 || sizes.back().first != max_width) sizes.emplace_back(max_width, units * 3);
    return sizes;
}
Video::Video(const char *title, int width, int height, int scale, bool automatic_fullscreen)
    : width_(width), height_(height), automatic_fullscreen_(automatic_fullscreen) {
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || scale < 1 || scale > 8)
        throw std::invalid_argument("Invalid video dimensions");
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) throw sdl_error("SDL video initialization failed");
    try {
        gamepad_initialized_ = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if (gamepad_initialized_) {
            int count = 0;
            auto *ids = SDL_GetGamepads(&count);
            for (int i = 0; ids && i < count; ++i) open_gamepad(ids[i]);
            SDL_free(ids);
        }
        int window_width = width * scale, window_height = height * scale;
        SDL_Rect usable{};
        auto display = SDL_GetPrimaryDisplay();
        if (display && SDL_GetDisplayUsableBounds(display, &usable) && usable.w > 0 && usable.h > 0) {
            // Leave room for desktop panels and window decorations on smaller screens.
            const double fit = std::min({1.0, usable.w * 0.90 / window_width,
                                         usable.h * 0.90 / window_height});
            window_width = std::max(1, int(window_width * fit));
            window_height = std::max(1, int(window_height * fit));
        }
        initial_window_width_ = window_width;
        initial_window_height_ = window_height;
        window_ = SDL_CreateWindow(title, window_width, window_height, SDL_WINDOW_RESIZABLE);
        if (!window_) throw sdl_error("Cannot create window");
        if (auto *icon = make_window_icon_surface()) {
            SDL_SetWindowIcon(window_, icon);
            SDL_DestroySurface(icon);
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) throw sdl_error("Cannot create renderer");
        if (!SDL_SetRenderLogicalPresentation(renderer_, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX))
            throw sdl_error("Cannot set logical resolution");
        set_frame_size(width, height);
    } catch (...) {
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        for (const auto &[id, gamepad] : gamepads_) { (void)id; SDL_CloseGamepad(gamepad); }
        if (gamepad_initialized_) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        throw;
    }
}
Video::~Video() {
    if (beer_cursor_) { SDL_SetCursor(SDL_GetDefaultCursor()); SDL_DestroyCursor(beer_cursor_); }
    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    for (const auto &[id, gamepad] : gamepads_) { (void)id; SDL_CloseGamepad(gamepad); }
    if (gamepad_initialized_) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
void Video::set_frame_size(int width, int height) {
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        throw std::invalid_argument("Invalid framebuffer dimensions");
    if (texture_ && frame_width_ == width && frame_height_ == height) return;
    rebuild_texture(width, height, filter_);
}
void Video::rebuild_texture(int width, int height, int filter) {
    std::vector<std::uint32_t> pixels(std::size_t(width) * height);
    const int scale = filter == 2 ? 2 : 1;
    std::vector<std::uint32_t> scaled(scale == 2 ? std::size_t(width) * height * 4 : 0);
    auto *texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, width * scale, height * scale);
    if (!texture) throw sdl_error("Cannot create framebuffer texture");
    if (!SDL_SetTextureScaleMode(texture, filter == 1 ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST)) {
        SDL_DestroyTexture(texture);
        throw sdl_error("Cannot set framebuffer scaling");
    }
    SDL_DestroyTexture(texture_);
    texture_ = texture;
    frame_width_ = width;
    frame_height_ = height;
    frame_ready_ = false;
    rgba_ = std::move(pixels);
    scaled_ = std::move(scaled);
    filter_ = filter;
}
SDL_WindowID Video::window_id() const { return SDL_GetWindowID(window_); }
SDL_FRect Video::frame_destination() const {
    const float scale = std::min(float(width_) / frame_width_, float(height_) / frame_height_);
    const float w = frame_width_ * scale, h = frame_height_ * scale;
    return {(width_ - w) / 2, (height_ - h) / 2, w, h};
}
std::pair<float, float> Video::window_to_frame(float x, float y) const {
    float logical_x = 0, logical_y = 0;
    if (!SDL_RenderCoordinatesFromWindow(renderer_, x, y, &logical_x, &logical_y))
        throw sdl_error("Cannot map mouse position");
    const auto rect = frame_destination();
    return {std::clamp((logical_x - rect.x) * frame_width_ / rect.w, 0.0f, float(frame_width_ - 1)),
            std::clamp((logical_y - rect.y) * frame_height_ / rect.h, 0.0f, float(frame_height_ - 1))};
}
void Video::warp_mouse_to_frame(float x, float y) {
    const auto rect = frame_destination();
    float window_x = 0, window_y = 0;
    if (!SDL_RenderCoordinatesToWindow(renderer_, rect.x + x * rect.w / frame_width_,
                                        rect.y + y * rect.h / frame_height_, &window_x, &window_y))
        throw sdl_error("Cannot map bottle position");
    SDL_WarpMouseInWindow(window_, window_x, window_y);
}
SDL_Surface *make_beer_cursor_surface() {
    static constexpr const char *pixels[] = {
        "..KKKKKKKK......", ".KWWWWWWWWK.....", "KWWWWWWWWWWK....", "KWWWWWWWWWWK....",
        ".KYYYYYYYYK.....", ".KYYYYYYYYKKKK..", ".KYLYLYLYYK...K.", ".KYLYLYLYYK...K.",
        ".KYLYLYLYYK...K.", ".KYLYLYLYYK...K.", ".KYLYLYLYYKKKK..", ".KYLYLYLYYK.....",
        ".KYYYYYYYYK.....", ".KYYYYYYYYK.....", ".KYYYYYYYYK.....", ".KYYYYYYYYK.....",
        ".KYYYYYYYYK.....", ".KKKKKKKKKK.....", "................", "................"
    };
    auto *surface = SDL_CreateSurface(32, 40, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return nullptr;
    for (int y = 0; y < 20; ++y) for (int x = 0; x < 16; ++x) {
        const char pixel = pixels[y][x];
        const Uint8 r = pixel == 'K' ? 80 : pixel == 'Y' ? 236 : 255;
        const Uint8 g = pixel == 'K' ? 43 : pixel == 'Y' ? 164 : pixel == 'L' ? 221 : 250;
        const Uint8 b = pixel == 'K' ? 12 : pixel == 'Y' ? 20 : pixel == 'L' ? 69 : 226;
        for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx)
            SDL_WriteSurfacePixel(surface, x * 2 + dx, y * 2 + dy, r, g, b, pixel == '.' ? 0 : 255);
    }
    return surface;
}
void Video::set_beer_cursor() {
    auto *surface = make_beer_cursor_surface();
    if (!surface) return;
    auto *cursor = SDL_CreateColorCursor(surface, 4, 4);
    SDL_DestroySurface(surface);
    if (cursor && SDL_SetCursor(cursor)) beer_cursor_ = cursor;
    else if (cursor) SDL_DestroyCursor(cursor);
}
void Video::show_loading() {
    if (!SDL_SetRenderDrawColor(renderer_, 15, 10, 5, 255) || !SDL_RenderClear(renderer_) ||
        !SDL_SetRenderDrawColor(renderer_, 255, 210, 125, 255) ||
        !SDL_RenderDebugText(renderer_, 220, 225, "THE LAST EICHHOF") ||
        !SDL_RenderDebugText(renderer_, 220, 245, "STARTING...") || !SDL_RenderPresent(renderer_))
        throw sdl_error("Cannot show startup screen");
}
void Video::present(std::span<const std::uint8_t> pixels, std::span<const Color, 256> palette) {
    if (pixels.size() != rgba_.size()) throw std::invalid_argument("Incorrect framebuffer size");
    std::uint32_t colors[256];
    for (std::size_t i = 0; i < 256; ++i) {
        const auto c = palette[i];
        // Keep the settings panel and its fonts readable at extreme values,
        // while the original artwork continues to preview the adjustment.
        if (clarity_controls_ && ((i >= 15 && i <= 30) || (i >= 240 && i <= 245) || clarity_ui_colors_[i])) {
            colors[i] = 0xff000000u | (std::uint32_t(c.r) << 16) | (std::uint32_t(c.g) << 8) | c.b;
            continue;
        }
        const int brightness = brightness_, contrast = contrast_;
        const auto adjust = [brightness, contrast](int value) {
            return std::clamp(((value - 128) * contrast / 100 + 128) * brightness / 100, 0, 255);
        };
        const auto r = adjust(c.r), g = adjust(c.g), b = adjust(c.b);
        colors[i] = 0xff000000u | (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | b;
    }
    for (std::size_t i = 0; i < pixels.size(); ++i) rgba_[i] = colors[pixels[i]];
    const std::uint32_t *upload = rgba_.data();
    int pitch = frame_width_ * int(sizeof(std::uint32_t));
    if (filter_ == 2) {
        scale2x(rgba_, frame_width_, frame_height_, scaled_);
        upload = scaled_.data(); pitch *= 2;
    }
    if (!SDL_UpdateTexture(texture_, nullptr, upload, pitch)) throw sdl_error("Cannot update framebuffer");
    frame_ready_ = true;
    if (!draw_frame()) throw sdl_error("Cannot present framebuffer");
}
bool Video::draw_frame(const char *screenshot_path) {
    const SDL_FRect destination = frame_destination();
    if (!SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255) || !SDL_RenderClear(renderer_) ||
        !SDL_RenderTexture(renderer_, texture_, nullptr, &destination))
        throw sdl_error("Cannot present framebuffer");
    if (scanline_strength_) {
        if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND) ||
            !SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 64 * scanline_strength_)) throw sdl_error("Cannot draw scanlines");
        const float step = std::max(2.0f, destination.h / frame_height_);
        for (float y = destination.y + step - 1; y < destination.y + destination.h; y += step)
            if (!SDL_RenderLine(renderer_, destination.x, y, destination.x + destination.w, y))
                throw sdl_error("Cannot draw scanlines");
        if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE)) throw sdl_error("Cannot restore blend mode");
    }
    bool saved = true;
    if (screenshot_path) {
        SDL_Surface *surface = SDL_RenderReadPixels(renderer_, nullptr);
        saved = surface && SDL_SaveBMP(surface, screenshot_path);
        SDL_DestroySurface(surface);
    }
    if (!SDL_RenderPresent(renderer_)) throw sdl_error("Cannot present framebuffer");
    return saved;
}
bool Video::save_screenshot(const char *path) {
    if (!frame_ready_ || !path || !*path) { SDL_SetError("No game image is available"); return false; }
    return draw_frame(path);
}
void Video::screenshot_shortcut() {
    char *base = SDL_GetPrefPath("", "lastbeer");
    if (!base) { std::fprintf(stderr, "Screenshot failed: %s\n", SDL_GetError()); return; }
    const std::string directory = std::string(base) + "screenshots/";
    SDL_free(base);
    if (!SDL_CreateDirectory(directory.c_str())) { std::fprintf(stderr, "Screenshot failed: %s\n", SDL_GetError()); return; }
    SDL_Time now{};
    SDL_DateTime date{};
    if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &date, true)) {
        std::fprintf(stderr, "Screenshot failed: %s\n", SDL_GetError()); return;
    }
    char name[96];
    for (int number = 0; number < 10000; ++number) {
        std::snprintf(name, sizeof(name), "lastbeer-%04d%02d%02d-%02d%02d%02d-%04d.bmp",
                      date.year, date.month, date.day, date.hour, date.minute, date.second, number);
        const std::string path = directory + name;
        SDL_PathInfo info{};
        if (SDL_GetPathInfo(path.c_str(), &info)) continue;
        if (save_screenshot(path.c_str())) std::fprintf(stderr, "Screenshot saved: %s\n", path.c_str());
        else std::fprintf(stderr, "Screenshot failed: %s\n", SDL_GetError());
        return;
    }
    std::fprintf(stderr, "Screenshot failed: too many screenshots with the same timestamp\n");
}
void Video::set_fullscreen(bool enabled) {
    if (!SDL_SetWindowFullscreen(window_, enabled)) throw sdl_error("Cannot toggle fullscreen");
    const bool synced = SDL_SyncWindow(window_);
    fullscreen_ = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
    if (!synced) throw sdl_error("Cannot complete fullscreen change");
    if (fullscreen_ != enabled) throw std::runtime_error("Fullscreen change was denied");
}
void Video::set_window_size(int width, int height) {
    if (width < 1 || height < 1 || width > 32768 || height > 32768)
        throw std::invalid_argument("Invalid window size");
    if (!SDL_SetWindowSize(window_, width, height)) throw sdl_error("Cannot resize window");
    if (!SDL_SyncWindow(window_)) throw sdl_error("Cannot complete window resize");
}
void Video::reset_window_size() { set_window_size(initial_window_width_, initial_window_height_); }
std::vector<std::pair<int, int>> Video::window_sizes() const {
    SDL_Rect bounds{};
    auto display = SDL_GetDisplayForWindow(window_);
    if (!display) display = SDL_GetPrimaryDisplay();
    if (!display || !SDL_GetDisplayBounds(display, &bounds)) return window_sizes_for_display(1280, 960);
    return window_sizes_for_display(bounds.w, bounds.h);
}
void Video::set_smooth(bool enabled) {
    set_filter(enabled ? 1 : 0);
}
void Video::set_filter(int filter) {
    if (filter < 0 || filter > 2) throw std::invalid_argument("Invalid image filter");
    if (filter != filter_) rebuild_texture(frame_width_, frame_height_, filter);
}
void Video::set_integer_scaling(bool enabled) {
    set_scaling(enabled ? 1 : 0);
}
void Video::set_scaling(int mode) {
    if (mode < 0 || mode > 2) throw std::invalid_argument("Invalid display scaling");
    const auto presentation = mode == 1 ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE :
                              mode == 2 ? SDL_LOGICAL_PRESENTATION_STRETCH : SDL_LOGICAL_PRESENTATION_LETTERBOX;
    if (!SDL_SetRenderLogicalPresentation(renderer_, width_, height_, presentation))
        throw sdl_error("Cannot change display scaling");
    scaling_ = mode;
}
void Video::set_brightness(int value) {
    if (value < 0 || value > 150) throw std::invalid_argument("Invalid brightness");
    brightness_ = value;
}
void Video::set_contrast(int value) {
    if (value < 0 || value > 150) throw std::invalid_argument("Invalid contrast");
    contrast_ = value;
}
void Video::set_scanline_strength(int value) {
    if (value < 0 || value > 3) throw std::invalid_argument("Invalid scanline strength");
    scanline_strength_ = value;
}
bool Video::set_vsync(bool enabled) {
    if (!SDL_SetRenderVSync(renderer_, enabled ? 1 : 0)) return false;
    vsync_ = enabled;
    return true;
}
void Video::set_text_input(bool enabled) {
    if (!(enabled ? SDL_StartTextInput(window_) : SDL_StopTextInput(window_)))
        throw sdl_error("Cannot change text input mode");
}
std::vector<SDL_JoystickID> Video::controller_ids() const {
    std::vector<SDL_JoystickID> ids;
    for (const auto &[id, pad] : gamepads_) { (void)pad; ids.push_back(id); }
    std::sort(ids.begin(), ids.end());
    return ids;
}
const char *Video::controller_name(SDL_JoystickID id) const {
    auto it = gamepads_.find(id);
    if (it == gamepads_.end()) return "Unknown controller";
    const char *name = SDL_GetGamepadName(it->second);
    return name && *name ? name : "Unknown controller";
}
std::string Video::controller_guid(SDL_JoystickID id) const {
    char result[33]{};
    SDL_GUIDToString(SDL_GetGamepadGUIDForID(id), result, sizeof(result));
    return result;
}
std::string Video::controller_identity(SDL_JoystickID id) const {
    const auto it = gamepads_.find(id);
    if (it == gamepads_.end()) return {};
    // Encode identifiers as a single portable settings-file token.
    auto encode = [](char kind, const char *value) {
        constexpr char hex[] = "0123456789abcdef";
        std::string out(1, kind);
        for (const unsigned char *p = reinterpret_cast<const unsigned char *>(value); *p; ++p) {
            out += hex[*p >> 4]; out += hex[*p & 15];
        }
        return out;
    };
    const char *serial = SDL_GetGamepadSerial(it->second);
    if (serial && *serial) {
        bool unique = true;
        for (const auto &[other_id, pad] : gamepads_) {
            if (other_id == id) continue;
            const char *other_serial = SDL_GetGamepadSerial(pad);
            if (other_serial && std::string(other_serial) == serial) { unique = false; break; }
        }
        if (unique) return encode('s', serial);
    }
    // Many pads have no serial number; preserve their order among identical
    // models instead of a device path that can change after reconnecting.
    int ordinal = 0;
    for (auto other : controller_ids()) {
        if (other == id) break;
        if (controller_guid(other) == controller_guid(id)) ++ordinal;
    }
    return "n" + std::to_string(ordinal);
}
void Video::refresh_controller_selection() {
    if (selected_guid_.empty()) {
        selected_id_ = 0; waiting_for_reconnect_ = false; input_.set_pad_filter(false, 0); return;
    }
    if (!selected_id_ || !gamepads_.contains(selected_id_) || controller_guid(selected_id_) != selected_guid_) {
        selected_id_ = 0;
        if (!waiting_for_reconnect_)
            for (auto id : controller_ids())
                if (controller_guid(id) == selected_guid_ &&
                    (selected_identity_.empty() || controller_identity(id) == selected_identity_)) { selected_id_ = id; break; }
    }
    input_.set_pad_filter(true, selected_id_);
}
void Video::select_controller(SDL_JoystickID id) {
    if (id && !gamepads_.contains(id)) throw std::invalid_argument("Controller is not connected");
    selected_id_ = id;
    waiting_for_reconnect_ = false;
    selected_guid_ = id ? controller_guid(id) : "";
    selected_identity_ = id ? controller_identity(id) : "";
    refresh_controller_selection();
}
void Video::select_controller_guid(const std::string &guid) {
    select_controller_saved(guid, "");
}
void Video::select_controller_saved(const std::string &guid, const std::string &identity) {
    selected_guid_ = guid;
    selected_identity_ = identity;
    selected_id_ = 0;
    waiting_for_reconnect_ = false;
    refresh_controller_selection();
}
void Video::open_gamepad(SDL_JoystickID id) {
    if (gamepads_.contains(id)) return;
    if (auto *pad = SDL_OpenGamepad(id)) {
        gamepads_.emplace(id, pad);
        if (waiting_for_reconnect_ && controller_guid(id) == selected_guid_ &&
            (selected_identity_.empty() || selected_identity_[0] == 'n' || controller_identity(id) == selected_identity_)) {
            selected_id_ = id;
            selected_identity_ = controller_identity(id);
            waiting_for_reconnect_ = false;
        }
        refresh_controller_selection();
    }
}
bool Video::poll() {
    input_.begin_frame();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) open_gamepad(event.gdevice.which);
        if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            auto it = gamepads_.find(event.gdevice.which);
            if (it != gamepads_.end()) {
                if (it->first == selected_id_ && !selected_guid_.empty()) {
                    selected_id_ = 0;
                    waiting_for_reconnect_ = true;
                }
                SDL_CloseGamepad(it->second); gamepads_.erase(it); refresh_controller_selection();
            }
        }
        if (automatic_fullscreen_ && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            (event.key.scancode == SDL_SCANCODE_F11 ||
             (event.key.scancode == SDL_SCANCODE_RETURN && (event.key.mod & SDL_KMOD_ALT)))) {
            set_fullscreen(!fullscreen_);
            continue;
        }
        if (automatic_fullscreen_ && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            event.key.scancode == SDL_SCANCODE_S && (event.key.mod & SDL_KMOD_ALT)) {
            screenshot_shortcut();
            continue;
        }
        input_.handle(event);
    }
    return !input_.quit_requested();
}
}

#pragma once

#include "pcx.h"
#include "input.h"
#include <SDL3/SDL.h>
#include <span>
#include <utility>
#include <vector>
#include <unordered_map>

namespace lastbeer {
void scale2x(std::span<const std::uint32_t> input, int width, int height,
             std::span<std::uint32_t> output);
std::vector<std::pair<int, int>> window_sizes_for_display(int width, int height);
SDL_Surface *make_beer_cursor_surface(); // Caller owns the returned surface.
SDL_Surface *make_beer_sidebar_surface(); // Caller owns the returned surface.
bool draw_beer_sidebars(SDL_Renderer *renderer, SDL_Texture *art);
SDL_Surface *make_window_icon_surface(); // Caller owns the returned surface.
bool draw_startup_icon(SDL_Renderer *renderer, int width);
// Owns one SDL window and displays indexed pixels with a mutable palette.
// All calls, including construction/destruction, must be on the SDL main thread.
class Video {
public:
    Video(const char *title, int width, int height, int scale = 2, bool automatic_fullscreen = true);
    ~Video();
    Video(const Video &) = delete;
    Video &operator=(const Video &) = delete;
    // Change the source framebuffer without recreating the window or changing
    // the logical canvas. The image is centered and aspect-correct.
    void set_frame_size(int width, int height);
    SDL_WindowID window_id() const;
    SDL_FRect frame_destination() const;
    std::pair<float, float> window_to_frame(float x, float y) const;
    void warp_mouse_to_frame(float x, float y);
    void set_beer_cursor();
    bool fullscreen() const { return fullscreen_; }
    void show_loading(bool show_icon = false);
    void present(std::span<const std::uint8_t> pixels, std::span<const Color, 256> palette);
    bool save_screenshot(const char *path);
    bool poll(); // Pumps events and returns false after a close request.
    const Input &input() const { return input_; }
    Input &input() { return input_; }
    std::vector<SDL_JoystickID> controller_ids() const;
    const char *controller_name(SDL_JoystickID id) const;
    std::string controller_guid(SDL_JoystickID id) const;
    std::string controller_identity(SDL_JoystickID id) const;
    const std::string &selected_controller_guid() const { return selected_guid_; }
    const std::string &selected_controller_identity() const { return selected_identity_; }
    SDL_JoystickID selected_controller_id() const { return selected_id_; }
    void select_controller(SDL_JoystickID id);
    void select_controller_guid(const std::string &guid);
    void select_controller_saved(const std::string &guid, const std::string &identity);
    void set_window_size(int width, int height);
    void reset_window_size();
    std::vector<std::pair<int, int>> window_sizes() const;
    void set_smooth(bool enabled);
    bool smooth() const { return filter_ == 1; }
    void set_filter(int filter); // 0: nearest, 1: linear, 2: Scale2x
    int filter() const { return filter_; }
    void set_integer_scaling(bool enabled);
    bool integer_scaling() const { return scaling_ == 1; }
    void set_scaling(int mode); // 0: fit, 1: integer, 2: stretch
    int scaling() const { return scaling_; }
    void set_scanlines(bool enabled) { set_scanline_strength(enabled ? 1 : 0); }
    bool scanlines() const { return scanline_strength_ != 0; }
    void set_scanline_strength(int value);
    int scanline_strength() const { return scanline_strength_; }
    void set_brightness(int value);
    int brightness() const { return brightness_; }
    void set_contrast(int value);
    int contrast() const { return contrast_; }
    void set_clarity_controls(bool visible) { clarity_controls_ = visible; }
    void mark_clarity_ui_color(int index) { if (index >= 0 && index < 256) clarity_ui_colors_[index] = true; }
    bool set_vsync(bool enabled);
    bool vsync() const { return vsync_; }
    void set_text_input(bool enabled);
    void set_fullscreen(bool enabled);
private:
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    SDL_Texture *sidebar_ = nullptr;
    SDL_Cursor *beer_cursor_ = nullptr;
    int width_, height_; // Fixed logical canvas.
    int frame_width_ = 0, frame_height_ = 0;
    int initial_window_width_ = 0, initial_window_height_ = 0;
    std::vector<std::uint32_t> rgba_;
    std::vector<std::uint32_t> scaled_;
    void rebuild_texture(int width, int height, int filter);
    Input input_;
    std::unordered_map<SDL_JoystickID, SDL_Gamepad *> gamepads_;
    bool gamepad_initialized_ = false;
    void open_gamepad(SDL_JoystickID id);
    void refresh_controller_selection();
    std::string selected_guid_;
    std::string selected_identity_;
    SDL_JoystickID selected_id_ = 0;
    bool waiting_for_reconnect_ = false;
    int filter_ = 0;
    int scaling_ = 0;
    int brightness_ = 100, contrast_ = 100, scanline_strength_ = 0;
    bool clarity_controls_ = false;
    std::array<bool, 256> clarity_ui_colors_{};
    bool frame_ready_ = false;
    bool draw_frame(const char *screenshot_path = nullptr);
    void screenshot_shortcut();
    bool vsync_ = false;
    bool fullscreen_ = false;
    bool automatic_fullscreen_;
};
}

#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lastbeer {
enum class PadAction { Up, Down, Left, Right, Confirm, Back, Pause, Count };
constexpr std::size_t pad_action_count = static_cast<std::size_t>(PadAction::Count);
// Buttons are SDL button indices; axes use 100 + axis * 2 + (positive ? 1 : 0).
constexpr int pad_axis(int axis, bool positive) { return 100 + axis * 2 + (positive ? 1 : 0); }
struct PadBinding { int primary = -1; int secondary = -1; };
using PadBindings = std::array<PadBinding, pad_action_count>;
struct KeyPress { SDL_Scancode scancode; SDL_Keymod modifiers; };
PadBindings default_pad_bindings();
bool valid_pad_trigger(int trigger);

class Input {
public:
    Input();
    void begin_frame();
    void handle(const SDL_Event &event);
    bool down(SDL_Scancode key) const;
    bool pressed(SDL_Scancode key) const;
    bool released(SDL_Scancode key) const;
    float mouse_x() const { return mouse_x_; }
    float mouse_y() const { return mouse_y_; }
    std::uint64_t mouse_motion_serial() const { return mouse_motion_serial_; }
    bool mouse_position_valid() const { return mouse_position_valid_; }
    bool mouse_left_down() const { return mouse_left_down_; }
    std::uint64_t mouse_click_serial() const { return mouse_click_serial_; }
    float mouse_click_x() const { return mouse_click_x_; }
    float mouse_click_y() const { return mouse_click_y_; }
    bool pad_down(PadAction action) const;
    bool pad_pressed(PadAction action) const;
    void set_pad_filter(bool specific, SDL_JoystickID id);
    void set_pad_bindings(const PadBindings &bindings);
    const PadBindings &pad_bindings() const { return bindings_; }
    void begin_pad_capture();
    void cancel_pad_capture();
    std::optional<int> take_pad_capture();
    bool quit_requested() const { return quit_; }
    const std::string &text() const { return text_; }
    const std::vector<KeyPress> &key_presses() const { return key_presses_; }
private:
    struct PadState {
        std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
        std::array<int, SDL_GAMEPAD_AXIS_COUNT> axes{};
    };
    bool accepts_pad(SDL_JoystickID id) const;
    void update_pad_actions();
    std::array<bool, SDL_SCANCODE_COUNT> down_{}, pressed_{}, released_{};
    std::unordered_map<SDL_JoystickID, PadState> pads_;
    PadBindings bindings_;
    std::array<bool, pad_action_count> pad_down_{}, pad_pressed_{};
    bool filter_specific_ = false;
    SDL_JoystickID selected_pad_ = 0;
    bool capturing_ = false;
    std::optional<int> captured_;
    bool east_pending_ = false;
    float mouse_x_ = 0, mouse_y_ = 0;
    float mouse_click_x_ = 0, mouse_click_y_ = 0;
    std::uint64_t mouse_motion_serial_ = 0, mouse_click_serial_ = 0;
    bool mouse_left_down_ = false, mouse_position_valid_ = false;
    SDL_JoystickID east_pad_ = 0;
    std::uint64_t east_started_ = 0;
    std::string text_;
    std::vector<KeyPress> key_presses_;
    bool quit_ = false;
};
}

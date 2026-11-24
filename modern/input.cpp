#include "input.h"
#include <cstdlib>

namespace lastbeer {
namespace {
bool valid(SDL_Scancode key) { return key > SDL_SCANCODE_UNKNOWN && key < SDL_SCANCODE_COUNT; }
constexpr std::size_t index(PadAction action) { return static_cast<std::size_t>(action); }
constexpr int deadzone = 12000, capture_threshold = 20000;
}
PadBindings default_pad_bindings() {
    return {{{SDL_GAMEPAD_BUTTON_DPAD_UP, pad_axis(SDL_GAMEPAD_AXIS_LEFTY, false)},
             {SDL_GAMEPAD_BUTTON_DPAD_DOWN, pad_axis(SDL_GAMEPAD_AXIS_LEFTY, true)},
             {SDL_GAMEPAD_BUTTON_DPAD_LEFT, pad_axis(SDL_GAMEPAD_AXIS_LEFTX, false)},
             {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, pad_axis(SDL_GAMEPAD_AXIS_LEFTX, true)},
             {SDL_GAMEPAD_BUTTON_SOUTH, -1}, {SDL_GAMEPAD_BUTTON_EAST, -1},
             {SDL_GAMEPAD_BUTTON_START, -1}}};
}
bool valid_pad_trigger(int trigger) {
    return trigger == -1 || (trigger >= 0 && trigger < SDL_GAMEPAD_BUTTON_COUNT) ||
           (trigger >= 100 && trigger < 100 + SDL_GAMEPAD_AXIS_COUNT * 2);
}
Input::Input() : bindings_(default_pad_bindings()) {}
void Input::begin_frame() {
    pressed_.fill(false); released_.fill(false); pad_pressed_.fill(false); text_.clear(); key_presses_.clear();
}
bool Input::accepts_pad(SDL_JoystickID id) const { return !filter_specific_ || (selected_pad_ && id == selected_pad_); }
void Input::set_pad_filter(bool specific, SDL_JoystickID id) {
    filter_specific_ = specific; selected_pad_ = id; update_pad_actions();
}
void Input::set_pad_bindings(const PadBindings &bindings) { bindings_ = bindings; update_pad_actions(); }
void Input::begin_pad_capture() { captured_.reset(); east_pending_ = false; capturing_ = true; }
void Input::cancel_pad_capture() { captured_.reset(); east_pending_ = false; capturing_ = false; }
std::optional<int> Input::take_pad_capture() {
    if (capturing_ && east_pending_ && SDL_GetTicks() - east_started_ >= 800) {
        captured_ = -2; // Hold the right face button to cancel, tap to bind it.
        capturing_ = false; east_pending_ = false;
    }
    auto result = captured_; captured_.reset(); return result;
}
void Input::update_pad_actions() {
    std::array<bool, pad_action_count> next{};
    for (const auto &[id, pad] : pads_) {
        if (!accepts_pad(id)) continue;
        auto active = [&](int trigger) {
            if (trigger >= 0 && trigger < SDL_GAMEPAD_BUTTON_COUNT) return pad.buttons[trigger];
            if (trigger >= 100 && trigger < 100 + SDL_GAMEPAD_AXIS_COUNT * 2) {
                int n = trigger - 100, value = pad.axes[n / 2];
                return (n & 1) ? value > deadzone : value < -deadzone;
            }
            return false;
        };
        for (std::size_t i = 0; i < next.size(); ++i)
            next[i] = next[i] || active(bindings_[i].primary) || active(bindings_[i].secondary);
    }
    for (std::size_t i = 0; i < next.size(); ++i) {
        if (next[i] && !pad_down_[i]) pad_pressed_[i] = true;
        pad_down_[i] = next[i];
    }
}
void Input::handle(const SDL_Event &event) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) quit_ = true;
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        for (std::size_t i = 0; i < down_.size(); ++i) {
            if (down_[i]) released_[i] = true;
            down_[i] = false;
        }
        mouse_left_down_ = false;
        mouse_position_valid_ = false;
        pads_.clear(); update_pad_actions();
    }
    if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) mouse_position_valid_ = false;
    if (event.type == SDL_EVENT_MOUSE_MOTION && event.motion.which != SDL_TOUCH_MOUSEID) {
        mouse_x_ = event.motion.x; mouse_y_ = event.motion.y;
        mouse_position_valid_ = true;
        ++mouse_motion_serial_;
    }
    if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
        event.button.which != SDL_TOUCH_MOUSEID && event.button.button == SDL_BUTTON_LEFT) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !mouse_left_down_) {
            mouse_click_x_ = event.button.x; mouse_click_y_ = event.button.y;
            ++mouse_click_serial_;
        }
        mouse_left_down_ = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    }
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        pads_.erase(event.gdevice.which);
        if (east_pending_ && east_pad_ == event.gdevice.which) east_pending_ = false;
        update_pad_actions();
    }
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        if (event.gbutton.button < SDL_GAMEPAD_BUTTON_COUNT) {
            pads_[event.gbutton.which].buttons[event.gbutton.button] = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            if (capturing_ && !captured_ && accepts_pad(event.gbutton.which)) {
                if (event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST) {
                    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && !east_pending_) {
                        east_pending_ = true; east_pad_ = event.gbutton.which; east_started_ = SDL_GetTicks();
                    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP && east_pending_ && east_pad_ == event.gbutton.which) {
                        captured_ = SDL_GetTicks() - east_started_ >= 800 ? -2 : SDL_GAMEPAD_BUTTON_EAST;
                        east_pending_ = false; capturing_ = false;
                    }
                } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                    captured_ = event.gbutton.button; capturing_ = false; east_pending_ = false;
                }
            }
            update_pad_actions();
        }
    }
    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.axis < SDL_GAMEPAD_AXIS_COUNT) {
        int &axis = pads_[event.gaxis.which].axes[event.gaxis.axis];
        const int old = axis; axis = event.gaxis.value;
        if (capturing_ && !captured_ && accepts_pad(event.gaxis.which) &&
            std::abs(old) < capture_threshold && std::abs(axis) >= capture_threshold) {
            captured_ = pad_axis(event.gaxis.axis, axis > 0); capturing_ = false;
        }
        update_pad_actions();
    }
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        auto key = event.key.scancode;
        if (!valid(key)) return;
        const auto i = static_cast<std::size_t>(key);
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (!down_[i] && !event.key.repeat) {
                pressed_[i] = true;
                key_presses_.push_back({key, event.key.mod});
            }
            down_[i] = true;
        } else { if (down_[i]) released_[i] = true; down_[i] = false; }
    }
    if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text) text_ += event.text.text;
}
bool Input::down(SDL_Scancode key) const { return valid(key) && down_[key]; }
bool Input::pressed(SDL_Scancode key) const { return valid(key) && pressed_[key]; }
bool Input::released(SDL_Scancode key) const { return valid(key) && released_[key]; }
bool Input::pad_down(PadAction action) const { return index(action) < pad_down_.size() && pad_down_[index(action)]; }
bool Input::pad_pressed(PadAction action) const { return index(action) < pad_pressed_.size() && pad_pressed_[index(action)]; }
}

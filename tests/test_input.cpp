#include "input.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
int main() {
    lastbeer::Input input;
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_LEFT;
    input.begin_frame(); input.handle(event);
    check(input.down(SDL_SCANCODE_LEFT) && input.pressed(SDL_SCANCODE_LEFT), "key press lost");
    input.begin_frame();
    event.key.repeat = true; input.handle(event);
    check(input.down(SDL_SCANCODE_LEFT) && !input.pressed(SDL_SCANCODE_LEFT), "repeat treated as new press");
    event.type = SDL_EVENT_KEY_UP; input.handle(event);
    check(!input.down(SDL_SCANCODE_LEFT) && input.released(SDL_SCANCODE_LEFT), "key release lost");
    input.begin_frame();
    event = {}; event.type = SDL_EVENT_KEY_DOWN; event.key.scancode = SDL_SCANCODE_B; input.handle(event);
    event.type = SDL_EVENT_KEY_UP; input.handle(event);
    event.type = SDL_EVENT_KEY_DOWN; event.key.scancode = SDL_SCANCODE_A; input.handle(event);
    check(input.key_presses().size() == 2 && input.key_presses()[0].scancode == SDL_SCANCODE_B &&
          input.key_presses()[1].scancode == SDL_SCANCODE_A, "keyboard events lost their order");
    event.type = SDL_EVENT_KEY_UP; input.handle(event);
    input.begin_frame(); event.type = SDL_EVENT_KEY_DOWN; event.key.scancode = SDL_SCANCODE_LEFT; event.key.repeat = false; input.handle(event);
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST; input.handle(event);
    check(!input.down(SDL_SCANCODE_LEFT) && input.released(SDL_SCANCODE_LEFT), "focus loss left key stuck");
    input.begin_frame();
    event = {}; event.type = SDL_EVENT_TEXT_INPUT; event.text.text = "Eichhof"; input.handle(event);
    check(input.text() == "Eichhof", "text input lost");
    input.begin_frame(); check(input.text().empty(), "text not cleared per frame");
    event = {}; event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 123; event.motion.y = 45; input.handle(event);
    check(input.mouse_x() == 123 && input.mouse_y() == 45 && input.mouse_motion_serial() == 1,
          "mouse motion lost");
    event = {}; event.type = SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.button = SDL_BUTTON_LEFT; input.handle(event);
    check(input.mouse_left_down(), "mouse button press lost");
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST; input.handle(event);
    check(!input.mouse_left_down(), "focus loss left mouse button held");
    event = {}; event.type = SDL_EVENT_MOUSE_BUTTON_DOWN; event.button.button = SDL_BUTTON_LEFT; input.handle(event);
    event.type = SDL_EVENT_MOUSE_BUTTON_UP; input.handle(event);
    check(!input.mouse_left_down(), "mouse button release lost");
    // Controller axes, D-pad and buttons combine correctly across devices.
    input.begin_frame(); event = {}; event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    event.gaxis.which = 1; event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX; event.gaxis.value = 20000; input.handle(event);
    check(input.pad_down(lastbeer::PadAction::Right) && input.pad_pressed(lastbeer::PadAction::Right), "stick direction lost");
    input.begin_frame(); event.gaxis.value = 1000; input.handle(event);
    check(!input.pad_down(lastbeer::PadAction::Right), "stick deadzone failed");
    event = {}; event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.which = 1;
    event.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN; input.handle(event);
    event.gbutton.which = 2; input.handle(event);
    input.begin_frame(); event.type = SDL_EVENT_GAMEPAD_REMOVED; event.gdevice.which = 1; input.handle(event);
    check(input.pad_down(lastbeer::PadAction::Down), "disconnect cleared another controller");
    event.gdevice.which = 2; input.handle(event);
    check(!input.pad_down(lastbeer::PadAction::Down), "disconnect left controller held");
    event = {}; event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.which = 3;
    event.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH; input.handle(event);
    check(input.pad_down(lastbeer::PadAction::Confirm) && input.pad_pressed(lastbeer::PadAction::Confirm), "controller confirm lost");
    input.begin_frame(); event.type = SDL_EVENT_WINDOW_FOCUS_LOST; input.handle(event);
    check(!input.pad_down(lastbeer::PadAction::Confirm), "focus loss left controller held");
    input.set_pad_filter(true, 7);
    input.begin_pad_capture();
    event = {}; event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.which = 8;
    event.gbutton.button = SDL_GAMEPAD_BUTTON_WEST; input.handle(event);
    check(!input.take_pad_capture().has_value(), "captured unselected controller");
    event = {}; event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION; event.gaxis.which = 7;
    event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX; event.gaxis.value = 15000; input.handle(event);
    event.gaxis.value = 23000; input.handle(event);
    auto captured = input.take_pad_capture();
    check(captured && *captured == lastbeer::pad_axis(SDL_GAMEPAD_AXIS_LEFTX, true), "axis binding capture failed");
    auto bindings = lastbeer::default_pad_bindings();
    bindings[static_cast<std::size_t>(lastbeer::PadAction::Confirm)] = {SDL_GAMEPAD_BUTTON_WEST, -1};
    input.set_pad_bindings(bindings);
    event = {}; event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.which = 7;
    event.gbutton.button = SDL_GAMEPAD_BUTTON_WEST; input.begin_frame(); input.handle(event);
    check(input.pad_down(lastbeer::PadAction::Confirm), "rebound controller button not applied");
    input.set_pad_filter(true, 9);
    check(!input.pad_down(lastbeer::PadAction::Confirm), "controller selection did not filter inputs");
    input.set_pad_filter(false, 0);
    input.begin_pad_capture();
    event = {}; event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.which = 7;
    event.gbutton.button = SDL_GAMEPAD_BUTTON_EAST; input.handle(event);
    check(!input.take_pad_capture().has_value(), "east button was not deferred");
    event.type = SDL_EVENT_GAMEPAD_BUTTON_UP; input.handle(event);
    check(input.take_pad_capture() == SDL_GAMEPAD_BUTTON_EAST, "tapping east did not bind it");
    input.begin_pad_capture(); event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN; input.handle(event);
    SDL_Delay(810);
    check(input.take_pad_capture() == -2, "holding east did not cancel binding");
    event.type = SDL_EVENT_QUIT; input.handle(event);
    check(input.quit_requested(), "quit event lost");
}

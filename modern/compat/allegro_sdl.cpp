#include "allegro.h"
// Avoid Allegro-style macros colliding with the C++ standard library.
#undef min
#undef max
#include "video.h"
#include "audio.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <deque>
#include <fstream>
#include <iterator>
#include <sstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
BITMAP *screen = nullptr;
char allegro_error[256] = {};
extern volatile char key[128];
extern volatile int pressedkeys;
extern int key_up, key_down, key_left, key_right, key_fire, key_pause;
extern int soundon;
extern int game_difficulty;
extern int speaker(int state);
#ifdef LASTBEER_AUTOMATION
extern long money;
extern int nweapons;
#endif
}
namespace {
constexpr int padding = 1024;
std::unique_ptr<lastbeer::Video> video;
std::unique_ptr<lastbeer::AudioDevice> audio;
bool skip_present = false;
std::uint64_t last_presented_at = 0;
struct AudioAttempt {
    std::mutex mutex;
    std::unique_ptr<lastbeer::AudioDevice> device;
    std::string error;
    bool finished = false;
    bool abandoned = false;
};
std::shared_ptr<AudioAttempt> audio_attempt;
std::array<lastbeer::Color, 256> colors{};
std::map<const SAMPLE *, std::vector<std::uint64_t>> sample_voices;
// Multiple voices can share one decoded sample without copying its data.
std::map<const SAMPLE *, std::map<int, std::shared_ptr<const lastbeer::Sound>>> sample_cache;
struct QueuedKey { int value; bool gamepad; };
std::deque<QueuedKey> key_queue;
bool last_key_gamepad = false;
int video_repeat_key = 0;
std::uint64_t video_repeat_at = 0;
struct Preferences {
    lastbeer::PadBindings bindings = lastbeer::default_pad_bindings();
    std::string guid;
    std::string controller_identity;
    int window_size = 0;
    int window_width = 0, window_height = 0;
    int filter = 0;
    int scaling = 0;
    int brightness = 100, contrast = 100, difficulty = 1, scanlines = 0;
    int font_style = 2, text_color = 1;
    bool fullscreen = false, vsync = false;
    bool mouse_control = false;
    int input_mode = 0; // Keyboard, keyboard + mouse, or controller.
};
Preferences preferences;
bool gameplay_input = false;
std::uint64_t mouse_motion_seen = 0;
std::uint64_t menu_motion_seen = 0, menu_click_seen = 0;
bool mouse_target_active = false;
bool preferences_loaded = false;
bool pending_auto_size = false;
std::vector<std::uint8_t> frame_pixels;
std::string executable_path;
#ifdef LASTBEER_AUTOMATION
int test_audio_probe_attempts = 0;
std::array<int, 3> test_menu_sounds{};
int test_menu_sounds_muted = 0;
#endif
void (*timer_callback)() = nullptr;
std::uint64_t interval_ns = 0, next_tick = 0;
std::array<unsigned char, 4096> invalid_row{};
#ifdef LASTBEER_AUTOMATION
std::string test_stage;
std::uint64_t test_stage_at = 0, test_release_at = 0;
SDL_Scancode test_release_key = SDL_SCANCODE_UNKNOWN;
SDL_GamepadButton test_release_button = SDL_GAMEPAD_BUTTON_INVALID;
bool test_sent = false, test_release_is_pad = false;
bool test_start_motion_sent = false, test_start_click_sent = false;
int test_options = 0, test_input_settings = 0, test_controls = 0, test_bindings = 0;
int test_controller_select = 0, test_pad_controls = 0, test_pad_bind = 0, test_video_settings = 0;
int test_menu_styles = 0;
int test_shop_buy = 0, test_shop_card = 0, test_shop_place = 0, test_shop_sell = 0;
long test_shop_balance_before_selection = 0;
int test_shop_weapons_before_selection = 0;
int test_shop_x1 = 0, test_shop_y1 = 0, test_shop_x2 = 0, test_shop_y2 = 0;
bool test_shop_hover_sent = false, test_shop_hover_checked = false;
bool test_shop_leave_sent = false, test_shop_redraw_checked = false;
SDL_JoystickID test_pad_id = 987;
int test_frames = 0, test_start_x = -1, test_max_x = -1, test_menus = 0;
int test_skipped_presentations = 0;
int test_menu_last_frame = -1, test_menu_animation_changes = 0;
int test_hard_spawns = 0;
int test_peak_shots = 0, test_shot_failures = 0;
int test_hard_start_x = 0, test_hard_direction = 0, test_hard_motion = 0;
int test_score_views = 0;
int test_score_name_keys = 0, test_score_winner_keys = 0;
std::array<int, 2> test_score_hover_events{}, test_score_hover_phase{};
int test_play_difficulty_steps = 0;
bool test_score_entry_done = false;
bool test_saw_shot = false, test_fullscreen_entered = false, test_fullscreen_exited = false;
SDL_WindowID test_window = 0;
void test_key(SDL_Scancode code, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = code;
    SDL_PushEvent(&event);
}
void test_pad(SDL_GamepadButton button, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    event.gbutton.which = test_pad_id;
    event.gbutton.button = button;
    SDL_PushEvent(&event);
}
void test_mouse_click(int x, int y) {
    int w = 0, h = 0;
    SDL_GetWindowSize(SDL_GetWindowFromID(video->window_id()), &w, &h);
    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = float(x * w) / 320; motion.motion.y = float(y * h) / 240;
    SDL_PushEvent(&motion);
    SDL_Event button{}; button.type = SDL_EVENT_MOUSE_BUTTON_DOWN; button.button.button = SDL_BUTTON_LEFT;
    button.button.x = motion.motion.x; button.button.y = motion.motion.y;
    SDL_PushEvent(&button);
    button.type = SDL_EVENT_MOUSE_BUTTON_UP; SDL_PushEvent(&button);
}
void test_mouse_motion(int x, int y) {
    int w = 0, h = 0;
    SDL_GetWindowSize(SDL_GetWindowFromID(video->window_id()), &w, &h);
    SDL_Event event{}; event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = float(x * w) / 320; event.motion.y = float(y * h) / 240;
    SDL_PushEvent(&event);
}
void test_pump() {
    auto now = SDL_GetTicks();
    if (test_release_at && now >= test_release_at) {
        if (test_release_is_pad) test_pad(test_release_button, false);
        else test_key(test_release_key, false);
        test_release_at = 0;
    }
    if (!test_stage.empty() && test_stage != "playing" && !test_sent && now - test_stage_at >= 250) {
        const bool controls_test = std::getenv("LASTBEER_TEST_CONTROLS") != nullptr;
        const bool pad_test = std::getenv("LASTBEER_TEST_PAD") != nullptr;
        const bool platform_test = std::getenv("LASTBEER_TEST_PLATFORM") != nullptr;
        const bool score_test = std::getenv("LASTBEER_TEST_SCORE_VIEW") != nullptr;
        const bool difficulty_test = std::getenv("LASTBEER_TEST_DIFFICULTY_MENU") != nullptr;
        const bool adjustment_test = std::getenv("LASTBEER_TEST_VIDEO_ADJUST") != nullptr;
        const bool style_test = std::getenv("LASTBEER_TEST_MENU_STYLE") != nullptr;
        const bool entry_test = std::getenv("LASTBEER_TEST_SCORE_ENTRY") != nullptr;
        const bool pad_entry = std::getenv("LASTBEER_TEST_SCORE_PAD") != nullptr;
        const bool keyboard_entry = std::getenv("LASTBEER_TEST_SCORE_ARROWS") != nullptr;
        const bool mouse_entry = std::getenv("LASTBEER_TEST_SCORE_MOUSE") != nullptr;
        const bool mixed_entry = std::getenv("LASTBEER_TEST_SCORE_MIXED") != nullptr;
        const bool mouse_score_view = std::getenv("LASTBEER_TEST_SCORE_MOUSE_VIEW") != nullptr;
        const bool menu_audio = std::getenv("LASTBEER_TEST_MENU_AUDIO") != nullptr;
        const bool menu_sfx = std::getenv("LASTBEER_TEST_MENU_SFX") != nullptr;
        const bool sound_overlap = std::getenv("LASTBEER_TEST_SOUND_OVERLAP") != nullptr;
        const bool mouse_test = std::getenv("LASTBEER_TEST_MOUSE") != nullptr;
        const bool mouse_menu_test = std::getenv("LASTBEER_TEST_MOUSE_MENU") != nullptr;
        const bool shop_cancel = std::getenv("LASTBEER_TEST_SHOP_CANCEL") != nullptr;
        const bool shop_balance = std::getenv("LASTBEER_TEST_SHOP_BALANCE") != nullptr;
        const bool shop_test = std::getenv("LASTBEER_TEST_SHOP_MOUSE") != nullptr || shop_cancel || shop_balance;
        const char *closing_test = std::getenv("LASTBEER_TEST_CLOSING");
        const char *play_test = std::getenv("LASTBEER_TEST_PLAY_DIFFICULTY");
        SDL_Scancode code = test_stage == "menu" ? (test_menus == 1 ? SDL_SCANCODE_1 : SDL_SCANCODE_6) : SDL_SCANCODE_SPACE;
        SDL_GamepadButton pad = SDL_GAMEPAD_BUTTON_INVALID;
        if (std::getenv("LASTBEER_TEST_START_CLICK") && test_stage == "start") {
            if (!test_start_motion_sent) {
                SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION; motion.motion.x = 10; motion.motion.y = 10;
                SDL_PushEvent(&motion);
                test_start_motion_sent = true;
            } else if (now - test_stage_at >= 500) {
                test_mouse_click(160, 120);
                test_start_click_sent = true;
                test_sent = true;
            }
            return;
        }
        if (closing_test && test_stage == "closing") {
            if (strcmp(closing_test, "click") == 0) test_mouse_click(160, 120);
            else {
                SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION; motion.motion.x = 10; motion.motion.y = 10;
                SDL_PushEvent(&motion);
            }
            test_sent = true;
            return;
        }
        if (closing_test && test_stage == "menu") code = SDL_SCANCODE_6;
        if (shop_test && (test_stage == "shop-buy" || test_stage == "shop-card" ||
                          test_stage == "shop-place" || test_stage == "shop-sell")) {
            if (shop_balance) {
                if (test_stage == "shop-buy" && test_shop_buy == 1) test_mouse_click(20, 120);
                else {
                    const bool exit = (test_stage == "shop-buy" && test_shop_buy >= 2) ||
                                      (test_stage == "shop-card" && test_shop_card >= 2);
                    test_mouse_click(exit ? test_shop_x2 : test_shop_x1, exit ? test_shop_y2 : test_shop_y1);
                }
                test_sent = true;
                return;
            }
            if (shop_cancel) {
                if (test_stage == "shop-place" && (test_shop_place == 1 || test_shop_place == 3)) {
                    test_key(SDL_SCANCODE_ESCAPE, true);
                    test_release_key = SDL_SCANCODE_ESCAPE;
                    test_release_is_pad = false;
                    test_release_at = now + 80;
                } else if (test_stage == "shop-buy" && test_shop_buy == 3) {
                    test_mouse_click(20, 120);
                } else {
                    const bool second = (test_stage == "shop-buy" && test_shop_buy >= 4) ||
                                        (test_stage == "shop-card" && test_shop_card >= 3) ||
                                        (test_stage == "shop-place" && test_shop_place >= 4);
                    test_mouse_click(second ? test_shop_x2 : test_shop_x1, second ? test_shop_y2 : test_shop_y1);
                }
                test_sent = true;
                return;
            }
            if (std::getenv("LASTBEER_TEST_SHOP_HOVER") && test_stage != "shop-place") {
                if (!test_shop_hover_sent) {
                    int w = 0, h = 0;
                    SDL_GetWindowSize(SDL_GetWindowFromID(video->window_id()), &w, &h);
                    SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
                    motion.motion.x = float(test_shop_x2 * w) / 320;
                    motion.motion.y = float(test_shop_y2 * h) / 240;
                    SDL_PushEvent(&motion);
                    test_shop_hover_sent = true;
                    return;
                }
                if (now - test_stage_at < 500) return;
                if (!test_shop_hover_checked) {
                    std::fprintf(stderr, "SMOKE: shop hover did not select the target\n"); std::exit(1);
                }
                if (test_stage == "shop-buy" && test_shop_buy == 1) {
                    if (!test_shop_leave_sent) {
                        int w = 0, h = 0;
                        SDL_GetWindowSize(SDL_GetWindowFromID(video->window_id()), &w, &h);
                        SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION;
                        motion.motion.x = float(test_shop_x1 * w) / 320;
                        motion.motion.y = float(test_shop_y1 * h) / 240;
                        SDL_PushEvent(&motion);
                        test_shop_leave_sent = true;
                        return;
                    }
                    if (now - test_stage_at < 750) return;
                    if (!test_shop_redraw_checked) {
                        std::fprintf(stderr, "SMOKE: exit sign was not checked after mouse movement\n"); std::exit(1);
                    }
                }
            }
            if (test_stage == "shop-buy" && test_shop_buy == 2) test_mouse_click(20, 120);
            else {
                const bool second = (test_stage == "shop-buy" && test_shop_buy >= 3) ||
                                    (test_stage == "shop-card" && test_shop_card >= 2) ||
                                    (test_stage == "shop-place" && test_shop_place >= 2) ||
                                    (test_stage == "shop-sell" && test_shop_sell >= 2);
                test_mouse_click(second ? test_shop_x2 : test_shop_x1, second ? test_shop_y2 : test_shop_y1);
            }
            test_sent = true;
            return;
        }
        if (mouse_menu_test && (test_stage == "menu" || test_stage == "options" || test_stage == "input-settings")) {
            if (test_stage == "menu") test_mouse_click(220, test_menus == 1 ? 133 : 153);
            else if (test_stage == "options") test_mouse_click(100, test_options == 1 ? 73 : 153);
            else test_mouse_click(100, test_input_settings == 1 ? 53 : 133);
            test_sent = true;
            return;
        }
        if (menu_sfx && test_stage == "options" && (test_options == 1 || test_options == 4)) {
            test_mouse_click(100, 53);
            test_sent = true;
            return;
        }
        if (mouse_score_view && (test_stage == "menu" || test_stage == "score")) {
            if (test_stage == "menu") test_mouse_click(220, test_menus == 1 ? 73 : 153);
            else test_mouse_click(test_score_views < 3 ? 140 : 240, 185);
            test_sent = true;
            return;
        }
        if (entry_test && std::getenv("LASTBEER_TEST_SCORE_HOVER") &&
            (test_stage == "score-name" || test_stage == "score-winner")) {
            const int which = test_stage == "score-name" ? 0 : 1;
            const int step = which == 0 ? test_score_name_keys : test_score_winner_keys;
            if (step == 1) {
                auto &phase = test_score_hover_phase[which];
                if (phase == 0) { test_mouse_motion(28, 232); ++phase; return; }
                if (phase == 1 && now - test_stage_at >= 400) { test_mouse_motion(36, 232); ++phase; return; }
                if (phase == 2 && now - test_stage_at >= 550) { test_mouse_motion(319, 210); ++phase; return; }
                if (phase < 3 || now - test_stage_at < 700) return;
                if (test_score_hover_events[which] != 2) {
                    std::fprintf(stderr, "SMOKE: score hover did not follow the pointer\n"); std::exit(1);
                }
            }
        }
        if (entry_test && mixed_entry && (test_stage == "score-name" || test_stage == "score-winner")) {
            const int step = test_stage == "score-name" ? test_score_name_keys : test_score_winner_keys;
            if ((test_stage == "score-name" && step <= 2) || (test_stage == "score-winner" && step == 1)) {
                test_mouse_click(test_stage == "score-name" && step == 1 ? 12 : 20, 232);
                test_sent = true;
                return;
            }
            code = test_stage == "score-name" && (step == 3 || step == 4) ? SDL_SCANCODE_LEFT :
                   test_stage == "score-name" && step == 5 ? SDL_SCANCODE_C : SDL_SCANCODE_RETURN;
        }
        else if (entry_test && mouse_entry && (test_stage == "score-name" || test_stage == "score-winner")) {
            const int step = test_stage == "score-name" ? test_score_name_keys : test_score_winner_keys;
            if (test_stage == "score-name" && step == 3) test_mouse_click(58, 64);
            else if ((test_stage == "score-name" && step == 5) || (test_stage == "score-winner" && step == 2))
                test_mouse_click(250, 175);
            else test_mouse_click(test_stage == "score-name" && step == 1 ? 12 : 20, 232);
            test_sent = true;
            return;
        }
        if (entry_test && mixed_entry && (test_stage == "score-name" || test_stage == "score-winner")) { /* use code above */ }
        else if (mouse_test && test_stage == "menu") code = test_menus == 1 ? SDL_SCANCODE_5 : test_menus == 2 ? SDL_SCANCODE_1 : SDL_SCANCODE_6;
        else if (mouse_test && test_stage == "options") code = test_options == 1 ? SDL_SCANCODE_DOWN : test_options == 2 ? SDL_SCANCODE_RETURN : SDL_SCANCODE_ESCAPE;
        else if (mouse_test && test_stage == "input-settings") code = test_input_settings == 1 ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_ESCAPE;
        else if (play_test && test_stage == "menu") {
            const bool retry = (strcmp(play_test, "cancel") == 0 || strcmp(play_test, "back") == 0) && test_menus == 2;
            code = (test_menus == 1 || retry) ? SDL_SCANCODE_1 : SDL_SCANCODE_6;
        }
        else if (play_test && test_stage == "play-difficulty") {
            if (strcmp(play_test, "pad") == 0)
                pad = test_play_difficulty_steps == 1 ? SDL_GAMEPAD_BUTTON_DPAD_DOWN : SDL_GAMEPAD_BUTTON_SOUTH;
            else if (strcmp(play_test, "easy") == 0)
                code = test_play_difficulty_steps == 1 ? SDL_SCANCODE_UP : SDL_SCANCODE_RETURN;
            else if (strcmp(play_test, "hard") == 0)
                code = test_play_difficulty_steps == 1 ? SDL_SCANCODE_DOWN : SDL_SCANCODE_RETURN;
            else if (strcmp(play_test, "classic") == 0)
                code = test_play_difficulty_steps == 1 ? SDL_SCANCODE_DOWN : test_play_difficulty_steps == 2 ? SDL_SCANCODE_UP : SDL_SCANCODE_RETURN;
            else if (strcmp(play_test, "cancel") == 0)
                code = test_play_difficulty_steps == 1 ? SDL_SCANCODE_UP : test_play_difficulty_steps == 2 ? SDL_SCANCODE_ESCAPE : SDL_SCANCODE_RETURN;
            else if (strcmp(play_test, "back") == 0)
                code = test_play_difficulty_steps <= 2 ? SDL_SCANCODE_DOWN : SDL_SCANCODE_RETURN;
        }
        else if (entry_test && (test_stage == "score-name" || test_stage == "score-winner") && keyboard_entry) {
            const int step = test_stage == "score-name" ? test_score_name_keys : test_score_winner_keys;
            if (test_stage == "score-name") {
                const SDL_Scancode sequence[] = {SDL_SCANCODE_UP, SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT,
                    SDL_SCANCODE_UP, SDL_SCANCODE_LEFT, SDL_SCANCODE_DOWN, SDL_SCANCODE_RIGHT,
                    SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_RETURN};
                code = step >= 1 && step <= 11 ? sequence[step - 1] : SDL_SCANCODE_RETURN;
            } else {
                const SDL_Scancode sequence[] = {SDL_SCANCODE_UP, SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT,
                    SDL_SCANCODE_RIGHT, SDL_SCANCODE_RETURN};
                code = step >= 1 && step <= 5 ? sequence[step - 1] : SDL_SCANCODE_RETURN;
            }
        }
        else if (entry_test && (test_stage == "score-name" || test_stage == "score-winner") && pad_entry) {
            const int step = test_stage == "score-name" ? test_score_name_keys : test_score_winner_keys;
            if (test_stage == "score-name") {
                const SDL_GamepadButton sequence[] = {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_UP,
                    SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
                    SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_UP,
                    SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_SOUTH};
                pad = step >= 1 && step <= 11 ? sequence[step - 1] : SDL_GAMEPAD_BUTTON_EAST;
            } else {
                const SDL_GamepadButton sequence[] = {SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_UP,
                    SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_SOUTH};
                pad = step >= 1 && step <= 5 ? sequence[step - 1] : SDL_GAMEPAD_BUTTON_EAST;
            }
        }
        else if (entry_test && test_stage == "score-name") code = !std::getenv("LASTBEER_TEST_SCORE_EMPTY") && test_score_name_keys == 1 ? SDL_SCANCODE_A : SDL_SCANCODE_RETURN;
        else if (entry_test && test_stage == "score-winner") code = !std::getenv("LASTBEER_TEST_SCORE_EMPTY") && test_score_winner_keys == 1 ? SDL_SCANCODE_B : SDL_SCANCODE_RETURN;
        else if (sound_overlap && test_stage == "menu") code = SDL_SCANCODE_6;
        else if (menu_sfx && test_stage == "menu") code = test_menus == 1 ? SDL_SCANCODE_DOWN : test_menus == 2 ? SDL_SCANCODE_5 : SDL_SCANCODE_6;
        else if (menu_sfx && test_stage == "options") code = test_options == 2 ? SDL_SCANCODE_DOWN : test_options == 3 ? SDL_SCANCODE_UP : SDL_SCANCODE_ESCAPE;
        else if (menu_audio && test_stage == "menu") code = test_menus == 1 ? SDL_SCANCODE_5 : SDL_SCANCODE_6;
        else if (menu_audio && test_stage == "options") code = test_options <= 2 ? SDL_SCANCODE_RETURN : SDL_SCANCODE_ESCAPE;
        else if (style_test && test_stage == "menu") code = test_menus == 1 ? SDL_SCANCODE_5 : SDL_SCANCODE_6;
        else if (style_test && test_stage == "options") code = test_options <= 4 ? SDL_SCANCODE_DOWN : test_options == 5 ? SDL_SCANCODE_RETURN : SDL_SCANCODE_ESCAPE;
        else if (style_test && test_stage == "menu-style") {
            if (test_menu_styles <= 3 || test_menu_styles == 5 || test_menu_styles == 6) code = SDL_SCANCODE_RIGHT;
            else if (test_menu_styles == 4 || test_menu_styles == 7) code = SDL_SCANCODE_DOWN;
            else code = SDL_SCANCODE_RETURN;
        }
        else if (adjustment_test && test_stage == "menu") code = test_menus == 1 ? SDL_SCANCODE_5 : SDL_SCANCODE_6;
        else if (adjustment_test && test_stage == "options") code = test_options <= 2 ? SDL_SCANCODE_DOWN : test_options == 3 ? SDL_SCANCODE_RETURN : SDL_SCANCODE_ESCAPE;
        else if (adjustment_test && test_stage == "video-settings") {
            if (test_video_settings <= 6 || test_video_settings == 9) code = SDL_SCANCODE_DOWN;
            else if (test_video_settings == 7 || test_video_settings == 11) code = SDL_SCANCODE_LEFT;
            else if (test_video_settings == 8 || test_video_settings == 10) code = SDL_SCANCODE_RIGHT;
            else code = SDL_SCANCODE_ESCAPE;
        }
        else if (difficulty_test && test_stage == "menu") code = test_menus == 1 ? SDL_SCANCODE_5 : test_menus == 2 ? SDL_SCANCODE_1 : SDL_SCANCODE_6;
        else if (difficulty_test && test_stage == "options") code = test_options <= 3 ? SDL_SCANCODE_DOWN : test_options == 4 ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_ESCAPE;
        else if (score_test && test_stage == "menu") code = test_menus == 1 ? SDL_SCANCODE_2 : SDL_SCANCODE_6;
        else if (score_test && test_stage == "score") code = test_score_views < 3 ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_SPACE;
        else if (platform_test) {
            if (test_stage == "menu") code = test_menus == 1 ? SDL_SCANCODE_5 : SDL_SCANCODE_6;
            else if (test_stage == "options") {
                if (test_options == 1 || test_options == 3) code = SDL_SCANCODE_DOWN;
                else if (test_options == 2 || test_options == 4) code = SDL_SCANCODE_RETURN;
                else code = SDL_SCANCODE_ESCAPE;
            } else if (test_stage == "input-settings") {
                if (test_input_settings <= 2) code = SDL_SCANCODE_RIGHT;
                else if (test_input_settings == 3 || test_input_settings == 4) code = SDL_SCANCODE_DOWN;
                else if (test_input_settings == 5 || test_input_settings == 7) code = SDL_SCANCODE_RETURN;
                else if (test_input_settings == 6) code = SDL_SCANCODE_UP;
                else code = SDL_SCANCODE_ESCAPE;
            } else if (test_stage == "controller-select") {
                code = test_controller_select == 1 ? SDL_SCANCODE_DOWN : test_controller_select == 2 ? SDL_SCANCODE_RETURN : SDL_SCANCODE_ESCAPE;
            } else if (test_stage == "pad-controls") {
                code = (test_pad_controls == 1 || test_pad_controls == 3 || test_pad_controls == 10) ? SDL_SCANCODE_RETURN :
                       (test_pad_controls == 2 || (test_pad_controls >= 4 && test_pad_controls <= 9)) ? SDL_SCANCODE_DOWN : SDL_SCANCODE_ESCAPE;
            } else if (test_stage == "pad-bind") pad = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
            else if (test_stage == "video-settings") {
                if (test_video_settings == 1 || test_video_settings == 5 || test_video_settings == 6 ||
                    test_video_settings == 8 || test_video_settings == 9 || test_video_settings == 11 ||
                    test_video_settings == 18) code = SDL_SCANCODE_RETURN;
                else if (test_video_settings == 3 || test_video_settings == 14 || test_video_settings == 16) code = SDL_SCANCODE_RIGHT;
                else code = SDL_SCANCODE_DOWN;
            }
        } else if (controls_test) {
            if (test_stage == "menu") code = test_menus == 1 ? SDL_SCANCODE_5 : SDL_SCANCODE_6;
            else if (test_stage == "options") pad = test_options == 1 ? SDL_GAMEPAD_BUTTON_DPAD_DOWN :
                                                    test_options == 2 ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
            else if (test_stage == "input-settings") pad = test_input_settings == 1 ? SDL_GAMEPAD_BUTTON_DPAD_DOWN :
                                                            test_input_settings == 2 ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
            else if (test_stage == "controls") pad = (test_controls == 1 || test_controls == 3 || test_controls == 4 || test_controls == 10) ? SDL_GAMEPAD_BUTTON_SOUTH :
                                                     (test_controls == 2 || (test_controls >= 5 && test_controls <= 9)) ? SDL_GAMEPAD_BUTTON_DPAD_DOWN : SDL_GAMEPAD_BUTTON_EAST;
            else if (test_stage == "bind") {
                if (test_bindings == 3) pad = SDL_GAMEPAD_BUTTON_EAST;
                else code = SDL_SCANCODE_W;
            }
        } else if (pad_test && (test_stage == "intro" || test_stage == "background" || test_stage == "play-difficulty" ||
                                test_stage == "title" || test_stage == "start" ||
                                test_stage == "end" || test_stage == "score" ||
                                (test_stage == "menu" && test_menus == 1))) {
            pad = SDL_GAMEPAD_BUTTON_SOUTH;
        }
        test_release_is_pad = pad != SDL_GAMEPAD_BUTTON_INVALID;
        if (test_release_is_pad) { test_pad(pad, true); test_release_button = pad; }
        else {
            test_key(code, true);
            if (std::getenv("LASTBEER_TEST_SCORE_BATCH") && test_stage == "score-name" && test_score_name_keys == 1) {
                test_key(SDL_SCANCODE_B, true); test_key(SDL_SCANCODE_B, false);
            }
            test_release_key = code;
        }
        test_release_at = now + 80;
        test_sent = true;
    }
    if (test_stage == "playing" && test_frames >= (std::getenv("LASTBEER_TEST_FULL_FIRE") ? 400 : 80) && !test_sent) {
        if (test_start_x < 0 || test_max_x <= test_start_x || !test_saw_shot ||
            (std::getenv("LASTBEER_TEST_SLOW_RENDER") && test_skipped_presentations == 0) ||
            ((std::getenv("LASTBEER_TEST_HARD_SPAWN") || std::getenv("LASTBEER_TEST_DIFFICULTY_MENU")) &&
             (test_hard_spawns == 0 || !test_hard_motion)) ||
            (std::getenv("LASTBEER_TEST_EASY_MODE") && test_max_x - test_start_x <= 30) ||
            (std::getenv("LASTBEER_TEST_HARD_SPAWN") && test_max_x - test_start_x >= 30) ||
            !test_fullscreen_entered || !test_fullscreen_exited || video->window_id() != test_window) {
            std::fprintf(stderr, "SMOKE: gameplay controls/display failed (start=%d max=%d shot=%d fullscreen=%d/%d)\n", test_start_x, test_max_x, test_saw_shot, test_fullscreen_entered, test_fullscreen_exited);
            std::exit(1);
        }
        if (std::getenv("LASTBEER_TEST_FULL_FIRE")) {
            if (test_shot_failures || test_peak_shots <= 30) {
                std::fprintf(stderr, "SMOKE: full ship fire failed (peak=%d failures=%d)\n", test_peak_shots, test_shot_failures);
                std::exit(1);
            }
            std::fprintf(stderr, "SMOKE: full ship fire peak=%d with no dropped shots\n", test_peak_shots);
        }
        std::fprintf(stderr, "SMOKE: gameplay frames=%d movement=%d shot=%d single-window fullscreen=ok\n", test_frames, test_max_x-test_start_x, test_saw_shot);
        test_key(SDL_SCANCODE_ESCAPE, true);
        test_release_key = SDL_SCANCODE_ESCAPE;
        test_release_is_pad = false;
        test_release_at = now + 80;
        test_sent = true;
    }
}
#endif

struct Mapping { SDL_Scancode sdl; int legacy; };
constexpr Mapping mappings[] = {
 {SDL_SCANCODE_ESCAPE,KEY_ESC}, {SDL_SCANCODE_1,2}, {SDL_SCANCODE_2,3}, {SDL_SCANCODE_3,4},
 {SDL_SCANCODE_4,5}, {SDL_SCANCODE_5,6}, {SDL_SCANCODE_6,7}, {SDL_SCANCODE_7,8},
 {SDL_SCANCODE_8,9}, {SDL_SCANCODE_9,10}, {SDL_SCANCODE_0,11}, {SDL_SCANCODE_MINUS,12},
 {SDL_SCANCODE_EQUALS,13}, {SDL_SCANCODE_BACKSPACE,KEY_BACKSPACE}, {SDL_SCANCODE_TAB,KEY_TAB},
 {SDL_SCANCODE_Q,16},{SDL_SCANCODE_W,17},{SDL_SCANCODE_E,18},{SDL_SCANCODE_R,19},
 {SDL_SCANCODE_T,20},{SDL_SCANCODE_Y,21},{SDL_SCANCODE_U,22},{SDL_SCANCODE_I,23},
 {SDL_SCANCODE_O,24},{SDL_SCANCODE_P,KEY_P},{SDL_SCANCODE_LEFTBRACKET,26}, {SDL_SCANCODE_RIGHTBRACKET,27},
 {SDL_SCANCODE_RETURN,KEY_ENTER},{SDL_SCANCODE_LCTRL,KEY_LCONTROL},{SDL_SCANCODE_A,30},
 {SDL_SCANCODE_S,31},{SDL_SCANCODE_D,32},{SDL_SCANCODE_F,33},{SDL_SCANCODE_G,34},
 {SDL_SCANCODE_H,35},{SDL_SCANCODE_J,36},{SDL_SCANCODE_K,37},{SDL_SCANCODE_L,38},
 {SDL_SCANCODE_SEMICOLON,39},{SDL_SCANCODE_APOSTROPHE,40},{SDL_SCANCODE_GRAVE,41},
 {SDL_SCANCODE_LSHIFT,KEY_LSHIFT},{SDL_SCANCODE_BACKSLASH,43},{SDL_SCANCODE_Z,44},
 {SDL_SCANCODE_X,45},{SDL_SCANCODE_C,46},{SDL_SCANCODE_V,47},{SDL_SCANCODE_B,48},
 {SDL_SCANCODE_N,49},{SDL_SCANCODE_M,50},{SDL_SCANCODE_COMMA,51},{SDL_SCANCODE_PERIOD,52},
 {SDL_SCANCODE_SLASH,53},{SDL_SCANCODE_RSHIFT,KEY_RSHIFT},{SDL_SCANCODE_KP_MULTIPLY,KEY_ASTERISK},
 {SDL_SCANCODE_LALT,KEY_ALT},{SDL_SCANCODE_SPACE,KEY_SPACE},{SDL_SCANCODE_CAPSLOCK,KEY_CAPSLOCK},
 {SDL_SCANCODE_F1,KEY_F1},{SDL_SCANCODE_F2,KEY_F2},{SDL_SCANCODE_F3,KEY_F3},{SDL_SCANCODE_F4,KEY_F4},
 {SDL_SCANCODE_F5,KEY_F5},{SDL_SCANCODE_F6,KEY_F6},{SDL_SCANCODE_F7,KEY_F7},{SDL_SCANCODE_F8,KEY_F8},
 {SDL_SCANCODE_F9,KEY_F9},{SDL_SCANCODE_F10,KEY_F10},{SDL_SCANCODE_F11,KEY_F11},{SDL_SCANCODE_F12,KEY_F12},
 {SDL_SCANCODE_NUMLOCKCLEAR,KEY_NUMLOCK},{SDL_SCANCODE_SCROLLLOCK,KEY_SCRLOCK},
 {SDL_SCANCODE_UP,KEY_UP},{SDL_SCANCODE_PAGEUP,KEY_PGUP},{SDL_SCANCODE_KP_MINUS,KEY_MINUS_PAD},
 {SDL_SCANCODE_LEFT,KEY_LEFT},{SDL_SCANCODE_KP_5,KEY_5_PAD},{SDL_SCANCODE_RIGHT,KEY_RIGHT},
 {SDL_SCANCODE_KP_PLUS,KEY_PLUS_PAD},{SDL_SCANCODE_END,KEY_END},{SDL_SCANCODE_DOWN,KEY_DOWN},
 {SDL_SCANCODE_PAGEDOWN,KEY_PGDN},{SDL_SCANCODE_INSERT,KEY_INSERT},{SDL_SCANCODE_DELETE,KEY_DEL},
 {SDL_SCANCODE_RCTRL,KEY_RCONTROL}
};
std::string preferences_path() {
    char *dir = SDL_GetPrefPath("", "lastbeer");
    if (!dir) return {};
    std::string path = std::string(dir) + "PLATFORM.CFG";
    SDL_free(dir);
    return path;
}
void save_preferences() {
    if (!preferences_loaded) return;
    auto path = preferences_path();
    if (path.empty()) { std::fprintf(stderr, "Could not locate platform settings directory.\n"); return; }
    auto tmp = path + ".tmp";
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) { std::fprintf(stderr, "Could not save platform settings.\n"); return; }
    out << "LASTBEER_PLATFORM_8\n" << (preferences.guid.empty() ? "auto" : preferences.guid) << '\n'
        << (preferences.controller_identity.empty() ? "-" : preferences.controller_identity) << '\n'
        << preferences.window_size << ' ' << preferences.fullscreen << ' ' << preferences.filter << ' ' << preferences.vsync
        << ' ' << preferences.scaling << ' ' << preferences.scanlines << ' ' << preferences.brightness << ' ' << preferences.difficulty << ' ' << preferences.contrast
        << ' ' << preferences.window_width << ' ' << preferences.window_height << ' ' << preferences.font_style << ' ' << preferences.text_color << '\n';
    for (const auto &b : preferences.bindings) out << b.primary << ' ' << b.secondary << '\n';
    out << "MOUSE " << preferences.mouse_control << '\n'
        << "INPUT_MODE " << preferences.input_mode << '\n';
    out.close();
    if (!out || !SDL_RenamePath(tmp.c_str(), path.c_str()))
        std::fprintf(stderr, "Could not save platform settings.\n");
}
void apply_window_size() {
    if (preferences.window_width > 0) {
        auto sizes = video->window_sizes();
        auto size = std::pair{preferences.window_width, preferences.window_height};
        if (sizes.size() > 1 && size.first > sizes.back().first) size = sizes.back();
        video->set_window_size(size.first, size.second);
    }
}
int selected_window_size() {
    if (!preferences.window_width) return 0;
    auto sizes = video->window_sizes();
    const auto wanted = std::pair{preferences.window_width, preferences.window_height};
    auto it = std::find(sizes.begin(), sizes.end(), wanted);
    return it == sizes.end() ? int(sizes.size() - 1) : int(it - sizes.begin());
}
void load_preferences() {
    preferences = Preferences{};
    const auto path = preferences_path();
#ifdef LASTBEER_AUTOMATION
    if (!preferences_loaded && std::getenv("LASTBEER_TEST_PLATFORM") && !path.empty()) std::remove(path.c_str());
    if (!preferences_loaded && std::getenv("LASTBEER_TEST_DIFFICULTY_MENU") && !path.empty()) std::remove(path.c_str());
    if (!preferences_loaded && std::getenv("LASTBEER_TEST_PLAY_DIFFICULTY") && !path.empty()) std::remove(path.c_str());
    if (!preferences_loaded && std::getenv("LASTBEER_TEST_VIDEO_ADJUST") && !path.empty()) std::remove(path.c_str());
    if (!preferences_loaded && std::getenv("LASTBEER_TEST_MENU_STYLE") && !path.empty()) std::remove(path.c_str());
    if (!preferences_loaded && std::getenv("LASTBEER_TEST_MOUSE") && !path.empty()) std::remove(path.c_str());
    if (!preferences_loaded && std::getenv("LASTBEER_TEST_MOUSE_MENU") && !path.empty()) std::remove(path.c_str());
    if (!preferences_loaded && std::getenv("LASTBEER_TEST_PREVIOUS_VIDEO_CONFIG") && !path.empty()) {
        const int version = std::getenv("LASTBEER_TEST_PREVIOUS_VIDEO_CONFIG")[0] - '0';
        std::ofstream fixture(path, std::ios::trunc);
        fixture << "LASTBEER_PLATFORM_" << version << "\nauto\n2 0 1 0";
        if (version >= 2) fixture << (version == 2 ? " 1 1" : " 2 1");
        if (version >= 4) fixture << (version == 4 ? " 3 2" : version == 5 ? " 1 2" : " 123 2");
        if (version >= 5) fixture << (version == 5 ? " 4" : " 145");
        if (version >= 7) fixture << " 960 720 2 1";
        fixture << '\n';
        for (const auto &b : lastbeer::default_pad_bindings()) fixture << b.primary << ' ' << b.secondary << '\n';
    }
#endif
    if (!path.empty()) {
        std::ifstream in(path);
        Preferences candidate;
        std::string magic, guid, controller_identity;
        int fullscreen = -1, filter = -1, vsync = -1, scaling = 0, scanlines = 0, brightness = 2, difficulty = 1, contrast = 2;
        int window_width = 0, window_height = 0, font_style = 2, text_color = 1;
        if (in >> magic >> guid &&
            (magic != "LASTBEER_PLATFORM_8" || bool(in >> controller_identity)) &&
            bool(in >> candidate.window_size >> fullscreen >> filter >> vsync) &&
            (magic == "LASTBEER_PLATFORM_1" || magic == "LASTBEER_PLATFORM_2" || magic == "LASTBEER_PLATFORM_3" || magic == "LASTBEER_PLATFORM_4" || magic == "LASTBEER_PLATFORM_5" || magic == "LASTBEER_PLATFORM_6" || magic == "LASTBEER_PLATFORM_7" || magic == "LASTBEER_PLATFORM_8") &&
            (magic == "LASTBEER_PLATFORM_1" || bool(in >> scaling >> scanlines)) &&
            ((magic != "LASTBEER_PLATFORM_4" && magic != "LASTBEER_PLATFORM_5" && magic != "LASTBEER_PLATFORM_6" && magic != "LASTBEER_PLATFORM_7" && magic != "LASTBEER_PLATFORM_8") || bool(in >> brightness >> difficulty)) &&
            ((magic != "LASTBEER_PLATFORM_5" && magic != "LASTBEER_PLATFORM_6" && magic != "LASTBEER_PLATFORM_7" && magic != "LASTBEER_PLATFORM_8") || bool(in >> contrast)) &&
            ((magic != "LASTBEER_PLATFORM_7" && magic != "LASTBEER_PLATFORM_8") || bool(in >> window_width >> window_height >> font_style >> text_color)) &&
            candidate.window_size >= 0 && candidate.window_size <= ((magic == "LASTBEER_PLATFORM_7" || magic == "LASTBEER_PLATFORM_8") ? 128 : 3) &&
            fullscreen >= 0 && fullscreen <= 1 && filter >= 0 && filter <= (magic == "LASTBEER_PLATFORM_1" ? 1 : 2) &&
            vsync >= 0 && vsync <= 1 && scaling >= 0 && scaling <= ((magic != "LASTBEER_PLATFORM_1" && magic != "LASTBEER_PLATFORM_2") ? 2 : 1) &&
            scanlines >= 0 && scanlines <= ((magic == "LASTBEER_PLATFORM_5" || magic == "LASTBEER_PLATFORM_6" || magic == "LASTBEER_PLATFORM_7" || magic == "LASTBEER_PLATFORM_8") ? 3 : 1) &&
            brightness >= 0 && brightness <= ((magic == "LASTBEER_PLATFORM_6" || magic == "LASTBEER_PLATFORM_7" || magic == "LASTBEER_PLATFORM_8") ? 150 : 4) &&
            contrast >= 0 && contrast <= ((magic == "LASTBEER_PLATFORM_6" || magic == "LASTBEER_PLATFORM_7" || magic == "LASTBEER_PLATFORM_8") ? 150 : 4) && difficulty >= 0 && difficulty <= 2 &&
            font_style >= 0 && font_style <= 2 && text_color >= 0 && text_color <= 1 &&
            ((window_width == 0 && window_height == 0) || (window_width >= 4 && window_width <= 32768 && window_width % 4 == 0 && window_height == window_width / 4 * 3)) &&
            (guid == "auto" || (guid.size() == 32 && std::all_of(guid.begin(), guid.end(), [](unsigned char c) { return std::isxdigit(c); }))) &&
            (controller_identity.empty() || controller_identity == "-" ||
             (controller_identity.size() <= 2048 && (controller_identity[0] == 's' || controller_identity[0] == 'p' || controller_identity[0] == 'n') &&
              std::all_of(controller_identity.begin() + 1, controller_identity.end(), [](unsigned char c) { return std::isxdigit(c); })))) {
            bool valid = true;
            for (auto &b : candidate.bindings) {
                if (!(in >> b.primary >> b.secondary) || b.primary < 0 || !lastbeer::valid_pad_trigger(b.primary) || !lastbeer::valid_pad_trigger(b.secondary)) { valid = false; break; }
            }
            if (valid) {
                for (std::size_t i = 0; i < candidate.bindings.size(); ++i) {
                    const auto &a = candidate.bindings[i];
                    if (a.secondary >= 0 && a.primary == a.secondary) valid = false;
                    for (std::size_t j = 0; j < i; ++j) {
                        const auto &b = candidate.bindings[j];
                        if (a.primary == b.primary || (b.secondary >= 0 && a.primary == b.secondary) ||
                            (a.secondary >= 0 && (a.secondary == b.primary || a.secondary == b.secondary))) valid = false;
                    }
                }
            }
            if (valid) {
                std::string extra;
                int mouse = 0;
                if (in >> extra >> mouse && extra == "MOUSE" && (mouse == 0 || mouse == 1))
                    { candidate.mouse_control = mouse != 0; candidate.input_mode = mouse ? 1 : 0; }
                int mode = 0;
                if (in >> extra >> mode && extra == "INPUT_MODE" && mode >= 0 && mode <= 2) {
                    candidate.input_mode = mode;
                    candidate.mouse_control = mode == 1;
                }
                if (guid != "auto") std::transform(guid.begin(), guid.end(), guid.begin(), [](unsigned char c) { return char(std::tolower(c)); });
                candidate.guid = guid == "auto" ? "" : guid;
                candidate.controller_identity = controller_identity == "-" ? "" : controller_identity;
                candidate.fullscreen = fullscreen; candidate.filter = filter; candidate.vsync = vsync;
                candidate.scaling = scaling; candidate.scanlines = scanlines;
                candidate.brightness = (magic == "LASTBEER_PLATFORM_6" || magic == "LASTBEER_PLATFORM_7" || magic == "LASTBEER_PLATFORM_8") ? brightness : 80 + brightness * 10;
                candidate.contrast = (magic == "LASTBEER_PLATFORM_6" || magic == "LASTBEER_PLATFORM_7" || magic == "LASTBEER_PLATFORM_8") ? contrast : 80 + contrast * 10;
                candidate.difficulty = difficulty;
                if (magic == "LASTBEER_PLATFORM_7" || magic == "LASTBEER_PLATFORM_8") { candidate.window_width = window_width; candidate.window_height = window_height; }
                else {
                    constexpr int old_sizes[][2] = {{0, 0}, {640, 480}, {960, 720}, {1280, 960}};
                    candidate.window_width = old_sizes[candidate.window_size][0];
                    candidate.window_height = old_sizes[candidate.window_size][1];
                }
                candidate.font_style = font_style; candidate.text_color = text_color;
                preferences = candidate;
            }
        }
    }
#ifdef LASTBEER_AUTOMATION
    if (std::getenv("LASTBEER_TEST_HARD_SPAWN")) preferences.difficulty = 2;
    if (std::getenv("LASTBEER_TEST_EASY_MODE")) preferences.difficulty = 0;
    if (std::getenv("LASTBEER_TEST_PAD")) { preferences.input_mode = 2; preferences.mouse_control = false; }
    if (const char *shop_mode = std::getenv("LASTBEER_TEST_SHOP_MODE")) {
        int mode = std::atoi(shop_mode);
        if (mode >= 0 && mode <= 2) { preferences.input_mode = mode; preferences.mouse_control = mode == 1; }
    }
    if (!preferences_loaded) if (const char *menu_mode = std::getenv("LASTBEER_TEST_MENU_MODE")) {
        int mode = std::atoi(menu_mode);
        if (mode >= 0 && mode <= 2) { preferences.input_mode = mode; preferences.mouse_control = mode == 1; }
    }
    if (const char *entry = std::getenv("LASTBEER_TEST_SCORE_ENTRY"))
        preferences.difficulty = strcmp(entry, "easy") == 0 ? 0 : strcmp(entry, "hard") == 0 ? 2 : 1;
#endif
    preferences_loaded = true;
    video->input().set_pad_bindings(preferences.bindings);
    video->select_controller_saved(preferences.guid, preferences.controller_identity);
    video->set_filter(preferences.filter);
    video->set_scaling(preferences.scaling);
    video->set_scanline_strength(preferences.scanlines);
    video->set_brightness(preferences.brightness);
    video->set_contrast(preferences.contrast);
    if (preferences.vsync && !video->set_vsync(true)) preferences.vsync = false;
    apply_window_size();
    if (preferences.fullscreen) video->set_fullscreen(true);
}
void fail(const char *message) {
    std::snprintf(allegro_error, sizeof(allegro_error), "%s", message);
    std::fprintf(stderr, "SDL backend: %s\n", message);
}
void pump() {
    if (video) {
        try {
#ifdef LASTBEER_AUTOMATION
            test_pump();
#endif
            if (!video->poll()) std::exit(0);
            if (preferences_loaded && preferences.fullscreen != video->fullscreen()) {
                preferences.fullscreen = video->fullscreen();
                if (!preferences.fullscreen) {
                    if (pending_auto_size) video->reset_window_size(); else apply_window_size();
                    pending_auto_size = false;
                }
                save_preferences();
            }
            if (preferences_loaded && !preferences.guid.empty() && video->selected_controller_id() &&
                !video->selected_controller_identity().empty() &&
                preferences.controller_identity != video->selected_controller_identity()) {
                preferences.controller_identity = video->selected_controller_identity();
                save_preferences();
            }
            const auto &input = video->input();
            int pressed_count = 0;
            for (const auto &m : mappings) {
                const bool movement = m.legacy == key_up || m.legacy == key_down || m.legacy == key_left ||
                                      m.legacy == key_right || m.legacy == key_fire;
                key[m.legacy] = input.down(m.sdl) && !(gameplay_input && preferences.input_mode == 2 && movement) ? 1 : 0;
                if (key[m.legacy]) ++pressed_count;
            }
            for (const auto &press : input.key_presses()) {
                for (const auto &m : mappings) if (m.sdl == press.scancode) {
                    auto symbol = SDL_GetKeyFromScancode(m.sdl, press.modifiers, true);
                    int ascii = symbol >= 32 && symbol <= 126 ? int(symbol) : 0;
                    if (m.legacy == KEY_ENTER) ascii = 13;
                    if (m.legacy == KEY_BACKSPACE) ascii = 8;
                    if (m.legacy == KEY_TAB) ascii = 9;
                    if (m.legacy == KEY_ESC) ascii = 27;
                    if (key_queue.size() < 64) key_queue.push_back({(m.legacy << 8) | ascii, false});
                    break;
                }
            }
            const std::array actions = {
                std::pair{lastbeer::PadAction::Up, key_up}, std::pair{lastbeer::PadAction::Down, key_down},
                std::pair{lastbeer::PadAction::Left, key_left}, std::pair{lastbeer::PadAction::Right, key_right},
                std::pair{lastbeer::PadAction::Confirm, key_fire}, std::pair{lastbeer::PadAction::Back, KEY_ESC},
                std::pair{lastbeer::PadAction::Pause, key_pause}
            };
            for (const auto &[action, legacy] : actions) {
                if (legacy <= 0 || legacy >= 128) continue;
                if (gameplay_input && preferences.input_mode != 2) continue;
                if (input.pad_down(action) && !key[legacy]) { key[legacy] = 1; ++pressed_count; }
                if (input.pad_pressed(action) && key_queue.size() < 64) {
                    // Confirm can also accept the default name on the legacy score screen.
                    int ascii = action == lastbeer::PadAction::Confirm ? 13 :
                                action == lastbeer::PadAction::Back ? 27 : 0;
                    key_queue.push_back({(legacy << 8) | ascii, true});
                }
            }
            pressedkeys = pressed_count;
        } catch (const std::exception &e) { fail(e.what()); std::exit(1); }
    }
    if (timer_callback && interval_ns) {
        auto now = SDL_GetTicksNS();
        int count = 0;
        while (now >= next_tick && count++ < 8) {
            timer_callback();
            next_tick += interval_ns;
        }
        if (now >= next_tick) next_tick = now + interval_ns;
    }
}
void present() {
    if (!screen || !video) return;
    if (skip_present) {
#ifdef LASTBEER_AUTOMATION
        if (test_stage == "playing" && std::getenv("LASTBEER_TEST_SLOW_RENDER")) ++test_skipped_presentations;
#endif
        pump(); return;
    }
    frame_pixels.resize(std::size_t(screen->w) * screen->h);
    for (int y = 0; y < screen->h; ++y)
        std::copy_n(screen->pixels + y * screen->pitch, screen->w, frame_pixels.data() + std::size_t(y) * screen->w);
    try { video->present(frame_pixels, colors); }
    catch (const std::exception &e) { fail(e.what()); std::exit(1); }
    last_presented_at = SDL_GetTicks();
#ifdef LASTBEER_AUTOMATION
    if (test_stage == "playing" && std::getenv("LASTBEER_TEST_SLOW_RENDER")) SDL_Delay(22);
#endif
    pump();
}
void draw_blit(BITMAP *src, BITMAP *dst, int sx, int sy, int dx, int dy, int w, int h, bool masked) {
    if (!src || !dst || w <= 0 || h <= 0) return;
    // Masked sprites and the gameplay pages are separate bitmaps. Avoid a
    // per-sprite allocation while retaining the overlap-safe path for blits.
    if (masked) {
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
            if (sx+x < 0 || sx+x >= src->w || sy+y < 0 || sy+y >= src->h ||
                dx+x < 0 || dx+x >= dst->w || dy+y < 0 || dy+y >= dst->h) continue;
            auto c = src->pixels[(sy+y)*src->pitch+sx+x];
            if (c) dst->pixels[(dy+y)*dst->pitch+dx+x] = c;
        }
        if (dst == screen) present();
        return;
    }
    // A temporary also makes overlapping screen-to-screen copies correct.
    std::vector<int> temp(std::size_t(w) * h, -1);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        if (sx+x >= 0 && sx+x < src->w && sy+y >= 0 && sy+y < src->h)
            temp[std::size_t(y)*w+x] = src->pixels[(sy+y)*src->pitch+sx+x];
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        int c = temp[std::size_t(y)*w+x];
        if (c >= 0 && (!masked || c != 0) && dx+x >= 0 && dx+x < dst->w && dy+y >= 0 && dy+y < dst->h)
            dst->pixels[(dy+y)*dst->pitch+dx+x] = static_cast<unsigned char>(c);
    }
    if (dst == screen) present();
}
}
extern "C" {
void compat_skip_present(int skip) {
    // A persistently overloaded machine must still refresh the window.
    skip_present = skip && SDL_GetTicks() - last_presented_at < 100;
}
int compat_replace_file(const char *source, const char *destination) {
    return SDL_RenamePath(source, destination) ? 1 : 0;
}
int allegro_init(void) { return 0; }
int install_keyboard(void) { return 0; }
int install_timer(void) { return 0; }
void install_int_ex(void (*callback)(void), int rate) {
    timer_callback = callback;
    interval_ns = rate > 0 ? 1000000000ull / unsigned(rate) : 0;
    next_tick = SDL_GetTicksNS() + interval_ns;
}
void set_leds(int) {}
uint64_t compat_ticks(void) { return SDL_GetTicks(); }
void compat_set_executable(const char *path) { executable_path = path ? path : ""; }
const char *compat_executable_directory(void) { return SDL_GetBasePath(); }
int compat_audio_probe_process(void) {
#ifdef LASTBEER_AUTOMATION
    if (std::getenv("LASTBEER_TEST_AUDIO_HANG")) SDL_Delay(5000);
#endif
    try { lastbeer::AudioDevice device; return 0; }
    catch (const std::exception &e) { std::fprintf(stderr, "Audio probe: %s\n", e.what()); return 1; }
}
void compat_show_startup(void) {
    try { if (video) video->show_loading(
#ifdef _WIN32
        true
#else
        false
#endif
    ); }
    catch (const std::exception &e) { fail(e.what()); }
}
int install_sound(int, int, const char *) {
    if (audio) return 0;
    if (audio_attempt) {
        std::lock_guard lock(audio_attempt->mutex);
        if (!audio_attempt->finished) {
            std::fprintf(stderr, "Sound disabled: the previous audio startup is still pending.\n");
            return -1;
        }
    }
    audio_attempt.reset();
    if (executable_path.empty()) { fail("Cannot determine executable path for audio startup"); return -1; }
#ifdef LASTBEER_AUTOMATION
    ++test_audio_probe_attempts;
#endif
    const char *args[] = {executable_path.c_str(), "--lastbeer-audio-probe", nullptr};
    SDL_Process *probe = SDL_CreateProcess(args, false);
    if (!probe) { std::fprintf(stderr, "Sound disabled: cannot start audio probe: %s\n", SDL_GetError()); return -1; }
    int exit_code = -1;
    bool finished = false;
    const auto deadline = SDL_GetTicks() + 1500;
    while (!(finished = SDL_WaitProcess(probe, false, &exit_code)) && SDL_GetTicks() < deadline) {
        pump();
        SDL_Delay(10);
    }
    if (!finished) {
        SDL_KillProcess(probe, true);
        // Reap promptly without allowing a broken backend to block startup.
        const auto kill_deadline = SDL_GetTicks() + 250;
        while (!SDL_WaitProcess(probe, false, &exit_code) && SDL_GetTicks() < kill_deadline) SDL_Delay(5);
    }
    SDL_DestroyProcess(probe);
    if (!finished || exit_code != 0) {
        std::fprintf(stderr, "Sound disabled: the audio device %s.\n", finished ? "could not be opened" : "did not respond");
        return -1;
    }
    // A successful probe cannot guarantee the next device open will return.
    // Isolate that actual open too, keeping the window responsive on timeout.
    auto attempt = std::make_shared<AudioAttempt>();
    audio_attempt = attempt;
#ifdef LASTBEER_AUTOMATION
    const bool test_open_hang = std::getenv("LASTBEER_TEST_AUDIO_OPEN_HANG") != nullptr;
#endif
    try {
        std::thread([attempt
#ifdef LASTBEER_AUTOMATION
                     , test_open_hang
#endif
        ] {
#ifdef LASTBEER_AUTOMATION
            if (test_open_hang) SDL_Delay(1800);
#endif
            {
                std::lock_guard lock(attempt->mutex);
                if (attempt->abandoned) { attempt->finished = true; return; }
            }
            std::unique_ptr<lastbeer::AudioDevice> device;
            std::string error;
            try { device = std::make_unique<lastbeer::AudioDevice>(); }
            catch (const std::exception &e) { error = e.what(); }
            {
                std::lock_guard lock(attempt->mutex);
                if (!attempt->abandoned) {
                    attempt->device = std::move(device);
                    attempt->error = std::move(error);
                    attempt->finished = true;
                    return;
                }
            }
            // If the timed-out open eventually returns, close it before a
            // later retry is allowed to start a second device.
            device.reset();
            std::lock_guard lock(attempt->mutex);
            attempt->finished = true;
        }).detach();
    } catch (const std::exception &e) {
        audio_attempt.reset();
        std::fprintf(stderr, "Sound disabled: cannot start audio worker: %s\n", e.what());
        return -1;
    }
    const auto open_deadline = SDL_GetTicks() + 1500;
    for (;;) {
        {
            std::lock_guard lock(attempt->mutex);
            if (attempt->finished) {
                audio = std::move(attempt->device);
                if (!audio) std::fprintf(stderr, "Sound disabled: %s\n", attempt->error.c_str());
                audio_attempt.reset();
                return audio ? 0 : -1;
            }
            if (SDL_GetTicks() >= open_deadline) {
                attempt->abandoned = true;
                std::fprintf(stderr, "Sound disabled: the audio device open did not respond.\n");
                return -1;
            }
        }
        pump();
        SDL_Delay(10);
    }
}
void remove_sound(void) { sample_voices.clear(); sample_cache.clear(); audio.reset(); }
void compat_stop_all_audio(void) { if (audio) audio->mixer().stop_all(); sample_voices.clear(); sample_cache.clear(); }
void compat_menu_sound(int effect) {
    if (effect < MENU_SOUND_MOVE || effect > MENU_SOUND_BACK) return;
    if (!audio || !soundon) {
#ifdef LASTBEER_AUTOMATION
        if (std::getenv("LASTBEER_TEST_MENU_SFX")) ++test_menu_sounds_muted;
#endif
        return;
    }
    try {
        static const std::array sounds = {
            std::make_shared<lastbeer::Sound>(lastbeer::make_menu_sound(lastbeer::MenuSound::Move)),
            std::make_shared<lastbeer::Sound>(lastbeer::make_menu_sound(lastbeer::MenuSound::Select)),
            std::make_shared<lastbeer::Sound>(lastbeer::make_menu_sound(lastbeer::MenuSound::Back))
        };
        audio->mixer().play(sounds[effect]);
#ifdef LASTBEER_AUTOMATION
        if (std::getenv("LASTBEER_TEST_MENU_SFX")) ++test_menu_sounds[effect];
#endif
    } catch (const std::exception &e) { std::fprintf(stderr, "Menu sound unavailable: %s\n", e.what()); }
}
int play_sample(const SAMPLE *sample, int vol, int, int freq, int loop) {
    if (!audio || !sample || !sample->data || !sample->len || sample->bits != 8 || sample->stereo) return -1;
    try {
        auto &voices = sample_voices[sample];
        std::erase_if(voices, [](auto id) { return !audio->mixer().active(id); });
        auto &sound = sample_cache[sample][freq];
        if (!sound) {
            auto decoded = std::make_shared<lastbeer::Sound>();
            decoded->sample_rate = int((long long)sample->freq * freq / 1000);
            decoded->priority = sample->priority;
            auto *p = static_cast<const unsigned char *>(sample->data);
            decoded->samples.reserve(sample->len);
            for (unsigned long i=0; i < sample->len; ++i) decoded->samples.push_back((int(p[i])-128)/128.0f);
            sound = std::move(decoded);
        }
        voices.push_back(audio->mixer().play(sound, loop != 0, std::clamp(vol/255.0f, 0.0f, 1.0f)));
        return 0;
    } catch (const std::exception &e) { fail(e.what()); return -1; }
}
void stop_sample(const SAMPLE *sample) {
    auto it = sample_voices.find(sample);
    if (it != sample_voices.end()) {
        if (audio) for (auto id : it->second) audio->mixer().stop(id);
        sample_voices.erase(it);
    }
    sample_cache.erase(sample);
}
void set_color_depth(int) {}
void request_refresh_rate(int) {}
int set_gfx_mode(int mode, int w, int h, int, int) {
    try {
        // All legacy modes share one resizable 4:3 canvas and one SDL window.
        // The intro's 640x350 image is centered until the 640x480 game starts.
        if (!video) {
            video = std::make_unique<lastbeer::Video>("The Last Eichhof", 640, 480, 2);
            video->set_beer_cursor();
            SDL_HideCursor();
            load_preferences();
            if (mode == GFX_AUTODETECT_FULLSCREEN && !video->fullscreen()) video->set_fullscreen(true);
        }
        auto *new_screen = create_bitmap(w, h);
        if (!new_screen) throw std::bad_alloc();
        try { video->set_frame_size(w, h); }
        catch (...) { destroy_bitmap(new_screen); throw; }
        destroy_bitmap(screen);
        screen = new_screen;
#ifdef LASTBEER_AUTOMATION
        if (!test_window) test_window = video->window_id();
        if (video->window_id() != test_window) {
            std::fprintf(stderr, "SMOKE: game window was replaced\n");
            std::exit(1);
        }
#endif
        return 0;
    } catch (const std::exception &e) { fail(e.what()); std::exit(1); }
}
BITMAP *create_bitmap(int w, int h) {
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return nullptr;
    auto *b = static_cast<BITMAP *>(std::calloc(1, sizeof(BITMAP)));
    if (!b) return nullptr;
    b->w = w; b->h = h; b->pitch = w + 2 * padding;
    b->allocation = static_cast<unsigned char *>(std::calloc(std::size_t(b->pitch), h));
    if (!b->allocation) { std::free(b); return nullptr; }
    b->pixels = b->allocation + padding;
    return b;
}
BITMAP *create_system_bitmap(int w, int h) { return create_bitmap(w,h); }
BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int w, int h) {
    if (!parent || x < 0 || y < 0 || w <= 0 || h <= 0 || x+w > parent->w || y+h > parent->h) return nullptr;
    auto *b = static_cast<BITMAP *>(std::calloc(1,sizeof(BITMAP)));
    if (!b) return nullptr;
    b->w=w; b->h=h; b->pitch=parent->pitch; b->pixels=parent->pixels+y*parent->pitch+x;
    return b;
}
void destroy_bitmap(BITMAP *b) { if (b) { std::free(b->allocation); std::free(b); } }
void clear_bitmap(BITMAP *b) { if(b) for(int y=0;y<b->h;++y) std::fill_n(b->pixels+y*b->pitch,b->w,0); }
void putpixel(BITMAP *b,int x,int y,int c) { if(b && x>=0 && x<b->w && y>=0 && y<b->h) b->pixels[y*b->pitch+x]=static_cast<unsigned char>(c); }
int getpixel(BITMAP *b,int x,int y) { return b && x>=0 && x<b->w && y>=0 && y<b->h ? b->pixels[y*b->pitch+x] : 0; }
void rectfill(BITMAP *b,int x1,int y1,int x2,int y2,int c) {
    if (!b) return;
    for(int y=std::max(0,y1);y<=std::min(y2,b->h-1);++y)
        for(int x=std::max(0,x1);x<=std::min(x2,b->w-1);++x) putpixel(b,x,y,c);
}
void blit(BITMAP *s,BITMAP *d,int sx,int sy,int dx,int dy,int w,int h) { draw_blit(s,d,sx,sy,dx,dy,w,h,false); }
void masked_blit(BITMAP *s,BITMAP *d,int sx,int sy,int dx,int dy,int w,int h) { draw_blit(s,d,sx,sy,dx,dy,w,h,true); }
void stretch_blit(BITMAP *s,BITMAP *d,int sx,int sy,int sw,int sh,int dx,int dy,int dw,int dh) {
    if(!s || !d || sw<=0 || sh<=0 || dw<=0 || dh<=0) return;
    for(int y=0;y<dh;++y) for(int x=0;x<dw;++x)
        putpixel(d,dx+x,dy+y,getpixel(s,sx+x*sw/dw,sy+y*sh/dh));
    if(d==screen) present();
}
void set_palette(const RGB *p) {
    for(int i=0;i<256;++i) colors[i]={static_cast<std::uint8_t>(std::min(int(p[i].r)*255/63,255)),static_cast<std::uint8_t>(std::min(int(p[i].g)*255/63,255)),static_cast<std::uint8_t>(std::min(int(p[i].b)*255/63,255))};
    present();
}
void set_color(int i,const RGB *p) {
    if(i>=0 && i<256) colors[i]={static_cast<std::uint8_t>(std::min(int(p->r)*255/63,255)),static_cast<std::uint8_t>(std::min(int(p->g)*255/63,255)),static_cast<std::uint8_t>(std::min(int(p->b)*255/63,255))};
}
void compat_bmp_select(BITMAP *) {}
uintptr_t bmp_write_line(BITMAP *b,int y) {
    if (!b || y<0 || y>=b->h) return reinterpret_cast<uintptr_t>(invalid_row.data()+padding);
    return reinterpret_cast<uintptr_t>(b->pixels+y*b->pitch);
}
void bmp_write8(uintptr_t address,int value) { *reinterpret_cast<unsigned char *>(address)=static_cast<unsigned char>(value); }
void bmp_unwrite_line(BITMAP *) {}
void rest(int ms) { pump(); if(ms>0) SDL_Delay(unsigned(ms)); pump(); }
void vsync(void) { present(); SDL_Delay(16); }
int keypressed(void) { pump(); if(key_queue.empty()) SDL_Delay(1); return !key_queue.empty(); }
int readkey(void) { while(!keypressed()) {} auto entry=key_queue.front(); key_queue.pop_front(); last_key_gamepad=entry.gamepad; return entry.value; }
int compat_last_key_gamepad(void) { return last_key_gamepad; }
void compat_video_repeat_reset(void) { video_repeat_key = 0; video_repeat_at = 0; }
void compat_video_clarity_controls(int visible) { if (video) video->set_clarity_controls(visible != 0); }
void compat_video_mark_ui_color(int index) { if (video) video->mark_clarity_ui_color(index); }
int compat_video_read(int repeat_adjustment) {
    if (!repeat_adjustment) compat_video_repeat_reset();
    for (;;) {
        const bool was_fullscreen = preferences.fullscreen;
        pump();
        // Fullscreen shortcuts are consumed by the video backend. Redraw the
        // settings immediately so the displayed value follows the shortcut.
        if (preferences.fullscreen != was_fullscreen) return 0;
        if (!key_queue.empty()) {
            int scan = (key_queue.front().value >> 8) & 255;
            key_queue.pop_front();
            if (repeat_adjustment && (scan == key_left || scan == key_right)) {
                video_repeat_key = scan; video_repeat_at = SDL_GetTicks() + 350;
            } else compat_video_repeat_reset();
            return scan;
        }
        if (video_repeat_key && !key[video_repeat_key]) compat_video_repeat_reset();
        if (repeat_adjustment && video_repeat_key && SDL_GetTicks() >= video_repeat_at) {
            video_repeat_at = SDL_GetTicks() + 60;
            return video_repeat_key;
        }
        SDL_Delay(5);
    }
}
int compat_mouse_poll(int *x, int *y) {
    pump();
    const auto &input = video->input();
    if (menu_click_seen != input.mouse_click_serial()) {
        menu_click_seen = input.mouse_click_serial();
        menu_motion_seen = input.mouse_motion_serial();
        const auto p = video->window_to_frame(input.mouse_click_x(), input.mouse_click_y());
        *x = int(p.first); *y = int(p.second);
        return -2;
    }
    if (menu_motion_seen != input.mouse_motion_serial()) {
        menu_motion_seen = input.mouse_motion_serial();
        if (input.mouse_position_valid()) {
            const auto p = video->window_to_frame(input.mouse_x(), input.mouse_y());
            *x = int(p.first); *y = int(p.second);
            return -1;
        }
    }
    return 0;
}
void compat_mouse_cursor_visible(int visible) {
    if (visible) SDL_ShowCursor(); else SDL_HideCursor();
}
int compat_menu_poll(int repeat_adjustment, int *x, int *y) {
    if (!repeat_adjustment) compat_video_repeat_reset();
    int mouse = compat_mouse_poll(x, y);
    if (mouse) return mouse;
    if (!key_queue.empty()) {
        int scan = (key_queue.front().value >> 8) & 255;
        key_queue.pop_front();
        if (repeat_adjustment && (scan == key_left || scan == key_right)) {
            video_repeat_key = scan; video_repeat_at = SDL_GetTicks() + 350;
        } else compat_video_repeat_reset();
        return scan;
    }
    if (video_repeat_key && !key[video_repeat_key]) compat_video_repeat_reset();
    if (repeat_adjustment && video_repeat_key && SDL_GetTicks() >= video_repeat_at) {
        video_repeat_at = SDL_GetTicks() + 60;
        return video_repeat_key;
    }
    return 0;
}
int compat_read_binding_key(void) {
    for (;;) {
        while (!keypressed()) {}
        auto key_event = key_queue.front();
        key_queue.pop_front();
        const int scan = (key_event.value >> 8) & 255;
        // Controller East cancels; other pad actions do not become keyboard bindings.
        if (!key_event.gamepad || scan == KEY_ESC) return scan;
    }
}
const char *compat_key_name(int keycode) {
    static char name[32];
    const char *source = "UNKNOWN";
    for (const auto &mapping : mappings)
        if (mapping.legacy == keycode) { source = SDL_GetScancodeName(mapping.sdl); break; }
    std::size_t i = 0;
    while (source && source[i] && i < sizeof(name) - 1) {
        unsigned char c = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(source[i])));
        name[i++] = c >= 32 && c <= 93 ? char(c) : '?';
    }
    name[i] = 0;
    return name;
}
void clear_keybuf(void) {
    pump(); key_queue.clear();
    if (video) {
        menu_motion_seen = video->input().mouse_motion_serial();
        menu_click_seen = video->input().mouse_click_serial();
    }
}
char *strupr(char *s) { for(char *p=s;*p;++p) *p=char(std::toupper(static_cast<unsigned char>(*p))); return s; }
const char *compat_config_path(void) {
    static std::string path;
    if (path.empty()) {
        char *dir = SDL_GetPrefPath("", "lastbeer");
        if (!dir) return nullptr;
        path = std::string(dir) + "CONFIG.HIG";
        SDL_free(dir);
    }
    return path.c_str();
}
int compat_controller_count(void) { return video ? int(video->controller_ids().size()) : 0; }
const char *compat_controller_name(int index) {
    if (!video) return "NO CONTROLLER";
    auto ids = video->controller_ids();
    if (index < 0 || std::size_t(index) >= ids.size()) return "NO CONTROLLER";
    return video->controller_name(ids[index]);
}
int compat_controller_selected(void) {
    if (!video || video->selected_controller_guid().empty()) return -1;
    auto ids = video->controller_ids();
    for (std::size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == video->selected_controller_id()) return int(i);
    return -2;
}
void compat_controller_select(int index) {
    if (!video) return;
    auto ids = video->controller_ids();
    if (index < -1 || (index >= 0 && std::size_t(index) >= ids.size())) return;
    video->select_controller(index < 0 ? 0 : ids[index]);
    preferences.guid = video->selected_controller_guid();
    preferences.controller_identity = video->selected_controller_identity();
    save_preferences();
}
const char *compat_pad_binding_name(int action) {
    static char name[48];
    if (action < 0 || action >= int(lastbeer::pad_action_count)) return "UNKNOWN";
    const auto b = preferences.bindings[action];
    if (b.primary >= 100) {
        int n = b.primary - 100;
        const char *axis = SDL_GetGamepadStringForAxis(static_cast<SDL_GamepadAxis>(n / 2));
        std::snprintf(name, sizeof(name), "%s %s", axis ? axis : "AXIS", (n & 1) ? "+" : "-");
    } else {
        const char *button = SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(b.primary));
        std::snprintf(name, sizeof(name), "%s%s", button ? button : "BUTTON", b.secondary >= 100 ? " / STICK" : "");
    }
    return name;
}
void compat_pad_reset(void) {
    preferences.bindings = lastbeer::default_pad_bindings();
    video->input().set_pad_bindings(preferences.bindings);
    save_preferences();
}
void compat_pad_begin_capture(void) { clear_keybuf(); video->input().begin_pad_capture(); }
int compat_pad_poll_capture(int *trigger) {
    pump();
    while (!key_queue.empty()) {
        auto entry = key_queue.front(); key_queue.pop_front();
        if (!entry.gamepad && ((entry.value >> 8) & 255) == KEY_ESC) return -1;
    }
    if (auto result = video->input().take_pad_capture()) {
        if (*result == -2) return -1;
        *trigger = *result; return 1;
    }
    SDL_Delay(5);
    return 0;
}
void compat_pad_end_capture(void) { video->input().cancel_pad_capture(); clear_keybuf(); }
void compat_pad_bind(int action, int trigger) {
    if (action < 0 || action >= int(lastbeer::pad_action_count) || trigger < 0 || !lastbeer::valid_pad_trigger(trigger)) return;
    for (int i = 0; i < int(lastbeer::pad_action_count); ++i) {
        if (i != action && (preferences.bindings[i].primary == trigger || preferences.bindings[i].secondary == trigger))
            preferences.bindings[i] = preferences.bindings[action];
    }
    preferences.bindings[action] = {trigger, -1};
    video->input().set_pad_bindings(preferences.bindings);
    save_preferences();
}
int compat_video_setting(int option) {
    if (option == 0) return preferences.fullscreen;
    if (option == 1) return selected_window_size();
    if (option == 2) return preferences.filter;
    if (option == 3) return preferences.scaling;
    if (option == 4) return preferences.scanlines;
    if (option == 5) return preferences.vsync;
    if (option == 6) return preferences.brightness;
    if (option == 7) return preferences.contrast;
    return 0;
}
int compat_video_window_size_count(void) { return int(video->window_sizes().size()); }
const char *compat_video_window_size_name(void) {
    static char name[40];
    const auto sizes = video->window_sizes();
    const int index = selected_window_size();
    if (index <= 0 || index >= int(sizes.size())) return "AUTO";
    std::snprintf(name, sizeof(name), "%d X %d", sizes[index].first, sizes[index].second);
    return name;
}
int compat_video_set(int option, int value) {
    try {
        if (option == 0 && (value == 0 || value == 1)) {
            video->set_fullscreen(value); preferences.fullscreen = value;
            if (!value) { if (pending_auto_size) video->reset_window_size(); else apply_window_size(); pending_auto_size = false; }
        }
        else if (option == 1 && value >= 0 && value < int(video->window_sizes().size())) {
            const auto sizes = video->window_sizes();
            preferences.window_size = value;
            preferences.window_width = sizes[value].first; preferences.window_height = sizes[value].second;
            pending_auto_size = value == 0 && video->fullscreen();
            if (!video->fullscreen()) { if (value == 0) video->reset_window_size(); else apply_window_size(); }
        }
        else if (option == 2 && value >= 0 && value <= 2) { video->set_filter(value); preferences.filter = value; }
        else if (option == 3 && value >= 0 && value <= 2) { video->set_scaling(value); preferences.scaling = value; }
        else if (option == 4 && value >= 0 && value <= 3) { video->set_scanline_strength(value); preferences.scanlines = value; }
        else if (option == 5 && (value == 0 || value == 1)) { if (!video->set_vsync(value)) return 0; preferences.vsync = value; }
        else if (option == 6 && value >= 0 && value <= 150) { video->set_brightness(value); preferences.brightness = value; }
        else if (option == 7 && value >= 0 && value <= 150) { video->set_contrast(value); preferences.contrast = value; }
        else return 0;
        save_preferences();
        return 1;
    } catch (const std::exception &e) { fail(e.what()); return 0; }
}
int compat_menu_style(int option) { return option == 0 ? preferences.font_style : option == 1 ? preferences.text_color : 0; }
void compat_menu_set_style(int option, int value) {
    if (option == 0 && value >= 0 && value <= 2) preferences.font_style = value;
    else if (option == 1 && value >= 0 && value <= 1) preferences.text_color = value;
    else return;
    save_preferences();
}
int compat_difficulty(void) { return preferences.difficulty; }
void compat_set_difficulty(int value) { if (value >= 0 && value <= 2) { preferences.difficulty = value; save_preferences(); } }
int compat_input_mode(void) { return preferences.input_mode; }
void compat_set_input_mode(int mode) {
    if (mode < 0 || mode > 2) return;
    preferences.input_mode = mode;
    preferences.mouse_control = mode == 1;
    mouse_target_active = false;
    save_preferences();
}
void compat_mouse_gameplay_begin(int x, int y) {
    gameplay_input = true;
    mouse_target_active = false;
    mouse_motion_seen = video->input().mouse_motion_serial();
    if (preferences.mouse_control) {
        try { video->warp_mouse_to_frame(float(x), float(y)); }
        catch (const std::exception &e) { fail(e.what()); }
    }
}
void compat_gameplay_end(void) { gameplay_input = false; }
int compat_mouse_target(int keyboard_moving, int *x, int *y) {
    if (!preferences.mouse_control || !video) return 0;
    const auto &input = video->input();
    if (mouse_motion_seen != input.mouse_motion_serial()) {
        mouse_motion_seen = input.mouse_motion_serial();
        mouse_target_active = true;
    }
    if (keyboard_moving || !input.mouse_position_valid()) mouse_target_active = false;
    if (!mouse_target_active) return 0;
    try {
        const auto position = video->window_to_frame(input.mouse_x(), input.mouse_y());
        *x = int(position.first); *y = int(position.second);
        return 1;
    } catch (const std::exception &e) { fail(e.what()); return 0; }
}
int compat_mouse_fire(void) {
    return preferences.mouse_control && video && video->input().mouse_left_down();
}
int compat_random_index(int count) {
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    auto value = std::uint64_t(now) ^ SDL_GetTicksNS() ^ SDL_GetPerformanceCounter();
    value ^= value >> 30; value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27; value *= 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return count > 0 ? int(value % std::uint64_t(count)) : 0;
}
void compat_shutdown(void) { remove_sound(); video.reset(); destroy_bitmap(screen); screen=nullptr; }
#ifdef LASTBEER_AUTOMATION
int compat_test_input_modes(void) {
    video->select_controller(0);
    for (int mode = 0; mode < 3; ++mode) {
        compat_set_input_mode(mode);
        load_preferences();
        if (preferences.input_mode != mode || preferences.mouse_control != (mode == 1)) return 0;
        gameplay_input = true;
        test_key(SDL_SCANCODE_LEFT, true);
        test_pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
        SDL_Event button{}; button.type = SDL_EVENT_MOUSE_BUTTON_DOWN; button.button.button = SDL_BUTTON_LEFT;
        SDL_PushEvent(&button);
        pump();
        if (!!key[key_left] != (mode != 2) || !!key[key_right] != (mode == 2) ||
            !!compat_mouse_fire() != (mode == 1)) return 0;
        gameplay_input = false;
        pump();
        if (!key[key_left] || !key[key_right]) return 0;
        test_key(SDL_SCANCODE_LEFT, false);
        test_pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
        button.type = SDL_EVENT_MOUSE_BUTTON_UP; SDL_PushEvent(&button);
        pump();
    }
    compat_set_input_mode(1);
    {
        const auto path = preferences_path();
        std::ifstream in(path);
        std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const auto marker = contents.find("INPUT_MODE ");
        if (marker == std::string::npos) return 0;
        contents.resize(marker);
        std::ofstream out(path, std::ios::trunc);
        out << contents;
        out.close();
        if (!out) return 0;
        load_preferences();
        if (preferences.input_mode != 1 || !preferences.mouse_control) return 0;
    }
    clear_keybuf();
    return 1;
}
void compat_test_hard_spawn(int foe, int slot, int x, int direction) {
    if (foe < 0 || slot < 0) { std::fprintf(stderr, "SMOKE: invalid hard-mode spawn\n"); std::exit(1); }
    ++test_hard_spawns;
    test_hard_start_x = x; test_hard_direction = direction;
}
void compat_test_hard_progress(int x) { if ((x - test_hard_start_x) * test_hard_direction > 0) test_hard_motion = 1; }
void compat_test_shot_power(int difficulty, int base, int actual) {
    const int expected = difficulty == 0 ? base * 5 : difficulty == 2 ? base * 3 : base;
    if (actual != expected) { std::fprintf(stderr, "SMOKE: weapon damage did not match difficulty\n"); std::exit(1); }
}
int compat_test_video_repeat(void) {
    key_left = KEY_LEFT; key_right = KEY_RIGHT;
    for (int pad = 0; pad < 2; ++pad) {
        compat_video_repeat_reset(); clear_keybuf();
        if (pad) test_pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
        else test_key(SDL_SCANCODE_RIGHT, true);
        if (compat_video_read(1) != KEY_RIGHT) return 0;
        auto start = SDL_GetTicks();
        if (compat_video_read(1) != KEY_RIGHT || SDL_GetTicks() - start < 250) return 0;
        if (pad) test_pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
        else test_key(SDL_SCANCODE_RIGHT, false);
        test_key(SDL_SCANCODE_ESCAPE, true);
        if (compat_video_read(1) != KEY_ESC) return 0;
        test_key(SDL_SCANCODE_ESCAPE, false);
        pump();
    }
    compat_video_repeat_reset(); clear_keybuf();
    return 1;
}
void compat_test_score_view(int difficulty) {
    if (!std::getenv("LASTBEER_TEST_SCORE_VIEW")) return;
    const int expected[] = {1, 2, 0};
    if (test_score_views >= 3 || difficulty != expected[test_score_views++]) {
        std::fprintf(stderr, "SMOKE: score difficulty navigation failed\n"); std::exit(1);
    }
}
void compat_test_menu_frame(int frame) {
    if (test_menu_last_frame >= 0 && frame != test_menu_last_frame) ++test_menu_animation_changes;
    test_menu_last_frame = frame;
}
void compat_test_menu_background(BITMAP *art) {
    for (int y = 45; y <= 119; ++y) for (int x = 134; x <= 160; ++x)
        if (getpixel(art, x, y) != 0) { std::fprintf(stderr, "SMOKE: obsolete menu bracket remains\n"); std::exit(1); }
}
void compat_test_stage(const char *stage) {
    test_stage = stage;
    if (test_stage == "intro" && std::getenv("LASTBEER_TEST_SOUND_OVERLAP")) {
        std::array<unsigned char, 24000> data{};
        data.fill(160);
        SAMPLE a{}; a.bits = 8; a.freq = 24000; a.len = data.size(); a.data = data.data();
        SAMPLE b = a;
        if (!audio || play_sample(&a, 255, 128, 1000, 1) || sample_voices[&a].size() != 1) {
            std::fprintf(stderr, "SMOKE: first sample did not play\n"); std::exit(1);
        }
        auto first = sample_voices[&a][0];
        for (int i = 0; i < 3; ++i) {
            if (play_sample(&a, 255, 128, 1000, 1)) {
                std::fprintf(stderr, "SMOKE: repeated sample could not play\n"); std::exit(1);
            }
        }
        if (sample_voices[&a].size() != 4 || sample_cache[&a].size() != 1 ||
            play_sample(&b, 255, 128, 1000, 1) || !audio->mixer().active(sample_voices[&b][0])) {
            std::fprintf(stderr, "SMOKE: overlapping samples were suppressed\n"); std::exit(1);
        }
        const auto copies = sample_voices[&a];
        for (std::size_t i = 0; i < copies.size(); ++i) {
            if (!audio->mixer().active(copies[i]) || (i && copies[i] == copies[i - 1])) {
                std::fprintf(stderr, "SMOKE: repeated sound did not get its own voice\n"); std::exit(1);
            }
        }
        if (play_sample(&a, 255, 128, 800, 1) || sample_cache[&a].size() != 2 ||
            sample_cache[&a][800]->sample_rate != 19200) {
            std::fprintf(stderr, "SMOKE: alternate playback rate was not preserved\n"); std::exit(1);
        }
        stop_sample(&a);
        for (auto id : copies) if (audio->mixer().active(id)) {
            std::fprintf(stderr, "SMOKE: stop left a repeated sample playing\n"); std::exit(1);
        }
        if (sample_cache.count(&a) || play_sample(&a, 255, 128, 1000, 1) ||
            sample_voices[&a].size() != 1 || sample_voices[&a][0] == first ||
            !audio->mixer().active(sample_voices[&b][0])) {
            std::fprintf(stderr, "SMOKE: stopped sample did not restart independently\n"); std::exit(1);
        }
        stop_sample(&a);
        stop_sample(&b);
        SAMPLE c = a; c.len = 2400;
        if (play_sample(&c, 255, 128, 1000, 0)) {
            std::fprintf(stderr, "SMOKE: one-shot sample did not play\n"); std::exit(1);
        }
        const auto completed = sample_voices[&c][0];
        const auto deadline = SDL_GetTicks() + 2000;
        while (audio->mixer().active(completed) && SDL_GetTicks() < deadline) SDL_Delay(10);
        if (audio->mixer().active(completed) || play_sample(&c, 255, 128, 1000, 0) ||
            sample_voices[&c].size() != 1 || sample_voices[&c][0] == completed) {
            std::fprintf(stderr, "SMOKE: completed sample could not play again\n"); std::exit(1);
        }
        stop_sample(&c);
        std::fprintf(stderr, "SMOKE: overlapping sound playback verified\n");
    }
    if (std::getenv("LASTBEER_TEST_MENU_SFX")) {
        if (test_stage == "options" && test_options == 3 &&
            (soundon || test_menu_sounds_muted < 2 || test_menu_sounds[MENU_SOUND_SELECT] != 2)) {
            std::fprintf(stderr, "SMOKE: muted menu sounds were played or sound was not disabled\n"); std::exit(1);
        }
        if (test_stage == "closing") {
            if (!soundon || test_menu_sounds[MENU_SOUND_MOVE] < 1 || test_menu_sounds[MENU_SOUND_SELECT] < 4 ||
                test_menu_sounds[MENU_SOUND_BACK] < 1 || test_menu_sounds_muted < 2) {
                std::fprintf(stderr, "SMOKE: menu sound effects missing\n"); std::exit(1);
            }
            std::fprintf(stderr, "SMOKE: menu sounds and mute verified\n");
        }
    }
    if (std::getenv("LASTBEER_TEST_START_CLICK") && test_stage == "playing" && !test_start_click_sent) {
        std::fprintf(stderr, "SMOKE: level start dismissed before mouse click\n"); std::exit(1);
    }
    if (std::getenv("LASTBEER_TEST_SHOP_MOUSE")) {
        const bool shown = test_stage == "menu" || test_stage == "score" || test_stage.rfind("shop-", 0) == 0;
        const bool hidden = test_stage == "intro" || test_stage == "title" || test_stage == "start" ||
                            test_stage == "playing" || test_stage == "end" || test_stage == "closing";
        if ((shown && !SDL_CursorVisible()) || (hidden && SDL_CursorVisible())) {
            std::fprintf(stderr, "SMOKE: wrong cursor visibility during %s\n", stage); std::exit(1);
        }
    }
    if (const char *play_test = std::getenv("LASTBEER_TEST_PLAY_DIFFICULTY")) {
        if (test_stage == "play-difficulty") ++test_play_difficulty_steps;
        if (test_stage == "menu" && test_menus == 1 &&
            (strcmp(play_test, "cancel") == 0 || strcmp(play_test, "back") == 0) &&
            (test_frames != 0 || preferences.difficulty != 1)) {
            std::fprintf(stderr, "SMOKE: canceling difficulty started a game or changed the selection\n"); std::exit(1);
        }
        if (test_stage == "start") {
            const int expected = strcmp(play_test, "easy") == 0 ? 0 :
                                 (strcmp(play_test, "hard") == 0 || strcmp(play_test, "pad") == 0) ? 2 : 1;
            load_preferences();
            if (preferences.difficulty != expected || game_difficulty != expected) {
                std::fprintf(stderr, "SMOKE: play difficulty was not applied and saved\n"); std::exit(1);
            }
        }
    }
    if (std::getenv("LASTBEER_TEST_SCORE_ENTRY")) {
        const char *rank = std::getenv("LASTBEER_TEST_SCORE_RANK");
        const bool no_entry = rank && strcmp(rank, "none") == 0;
        if (test_stage == "score-name") ++test_score_name_keys;
        if (test_stage == "score-winner") ++test_score_winner_keys;
        if (test_stage == "score-name" && test_score_name_keys == 1)
            if (const char *capture = std::getenv("LASTBEER_CAPTURE_SCORE")) video->save_screenshot(capture);
        if (test_stage == "score" && !no_entry) { std::fprintf(stderr, "SMOKE: extra wait after score entry\n"); std::exit(1); }
        if (test_stage == "score-entry-done") test_score_entry_done = true;
        if (test_stage == "closing") {
            const bool pad_entry = std::getenv("LASTBEER_TEST_SCORE_PAD") != nullptr;
            const bool keyboard_entry = std::getenv("LASTBEER_TEST_SCORE_ARROWS") != nullptr;
            const bool batch_entry = std::getenv("LASTBEER_TEST_SCORE_BATCH") != nullptr;
            const bool mouse_entry = std::getenv("LASTBEER_TEST_SCORE_MOUSE") != nullptr;
            const bool mixed_entry = std::getenv("LASTBEER_TEST_SCORE_MIXED") != nullptr;
            const bool empty_entry = std::getenv("LASTBEER_TEST_SCORE_EMPTY") != nullptr;
            const int winner_keys = no_entry || (rank && strcmp(rank, "second") == 0) ? 0 : empty_entry ? 1 : (pad_entry || keyboard_entry) ? 5 : 2;
            const int name_keys = no_entry ? 0 : empty_entry ? 1 : (pad_entry || keyboard_entry) ? 11 : mixed_entry ? 6 : mouse_entry ? 5 : batch_entry ? 3 : 2;
            if (!test_score_entry_done || test_score_name_keys != name_keys || test_score_winner_keys != winner_keys) {
                std::fprintf(stderr, "SMOKE: score entry did not return to the menu\n"); std::exit(1);
            }
            if (std::getenv("LASTBEER_TEST_SCORE_HOVER")) {
                if (test_score_hover_events[0] != 2 || test_score_hover_events[1] != 2) {
                    std::fprintf(stderr, "SMOKE: score hover was not exercised on both entry screens\n"); std::exit(1);
                }
                std::fprintf(stderr, "SMOKE: score letter hover verified\n");
            }
        }
    }
    if (test_stage == "start" && !test_menu_animation_changes) {
        std::fprintf(stderr, "SMOKE: menu pointer did not animate\n"); std::exit(1);
    }
    if (test_stage == "closing" && std::getenv("LASTBEER_TEST_SCORE_VIEW") && test_score_views != 3) {
        std::fprintf(stderr, "SMOKE: did not visit all score tables\n"); std::exit(1);
    }
    if (test_stage == "intro" && std::getenv("LASTBEER_EXPECT_AUDIO") &&
        (!audio || !soundon || test_audio_probe_attempts != 1)) {
        std::fprintf(stderr, "SMOKE: audio did not start successfully\n"); std::exit(1);
    }
    if (test_stage == "options" && std::getenv("LASTBEER_TEST_MENU_AUDIO") && test_options == 2) {
        bool active = false;
        for (const auto &[sample, voices] : sample_voices) {
            (void)sample;
            for (auto id : voices) if (audio && audio->mixer().active(id)) active = true;
        }
        if (!soundon || !active) { std::fprintf(stderr, "SMOKE: menu sound did not resume\n"); std::exit(1); }
    }
    if (test_stage == "intro" && std::getenv("LASTBEER_TEST_PREVIOUS_VIDEO_CONFIG")) {
        const int version = std::getenv("LASTBEER_TEST_PREVIOUS_VIDEO_CONFIG")[0] - '0';
        if (preferences.window_size != 2 || preferences.filter != 1 || preferences.fullscreen ||
            preferences.scaling != (version == 1 ? 0 : version == 2 ? 1 : 2) || preferences.scanlines != (version != 1) ||
            preferences.window_width != 960 || preferences.window_height != 720 ||
            preferences.brightness != (version == 4 ? 110 : version == 5 ? 90 : version >= 6 ? 123 : 100) ||
            preferences.contrast != (version == 5 ? 120 : version >= 6 ? 145 : 100) || preferences.difficulty != (version >= 4 ? 2 : 1)) {
            std::fprintf(stderr, "SMOKE: older video settings did not load\n"); std::exit(1);
        }
        std::fprintf(stderr, "SMOKE: older video settings loaded\n");
    }
    if (test_stage == "intro" && std::getenv("LASTBEER_EXPECT_AUDIO_FALLBACK") &&
        (audio || soundon || test_audio_probe_attempts != 1)) {
        std::fprintf(stderr, "SMOKE: audio timeout did not fall back to silence\n"); std::exit(1);
    }
    if (test_stage == "intro" && std::getenv("LASTBEER_EXPECT_AUDIO_OPEN_FALLBACK") &&
        (audio || soundon || test_audio_probe_attempts != 1)) {
        std::fprintf(stderr, "SMOKE: audio device open timeout did not fall back to silence\n"); std::exit(1);
    }
    if (test_stage == "intro" && std::getenv("LASTBEER_EXPECT_AUDIO_OPEN_FALLBACK")) {
        SDL_unsetenv_unsafe("LASTBEER_TEST_AUDIO_OPEN_HANG");
        const auto retry_deadline = SDL_GetTicks() + 2500;
        while (audio_attempt && SDL_GetTicks() < retry_deadline) {
            bool finished;
            { std::lock_guard lock(audio_attempt->mutex); finished = audio_attempt->finished; }
            if (finished) break;
            SDL_Delay(10);
        }
        if (!speaker(1) || !audio || test_audio_probe_attempts != 2) {
            std::fprintf(stderr, "SMOKE: sound could not recover after the device open timeout\n"); std::exit(1);
        }
    }
    if (test_stage == "intro" && std::getenv("LASTBEER_EXPECT_AUDIO_FALLBACK")) {
        SDL_unsetenv_unsafe("LASTBEER_TEST_AUDIO_HANG");
        if (!speaker(1) || !audio || test_audio_probe_attempts != 2) {
            std::fprintf(stderr, "SMOKE: sound could not recover after a timeout\n"); std::exit(1);
        }
    }
    if (test_stage == "completed" && std::getenv("LASTBEER_EXPECT_NO_AUDIO") && test_audio_probe_attempts != 0) {
        std::fprintf(stderr, "SMOKE: no-sound mode attempted audio startup\n"); std::exit(1);
    }
    if (test_stage == "menu") {
        ++test_menus;
        if (test_menus == 1 && std::getenv("LASTBEER_TEST_PLATFORM")) {
            SDL_VirtualJoystickDesc desc{}; SDL_INIT_INTERFACE(&desc);
            desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
            desc.naxes = SDL_GAMEPAD_AXIS_COUNT; desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
            desc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
            desc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
            desc.name = "Lastbeer test controller";
            test_pad_id = SDL_AttachVirtualJoystick(&desc);
            if (!test_pad_id) { std::fprintf(stderr, "SMOKE: could not attach controller\n"); std::exit(1); }
        }
    }
    if (test_stage == "options") {
        ++test_options;
        if (std::getenv("LASTBEER_TEST_MOUSE") && test_options == 3) {
            if (!preferences.mouse_control) { std::fprintf(stderr, "SMOKE: mouse option did not enable\n"); std::exit(1); }
            load_preferences();
            if (!preferences.mouse_control) { std::fprintf(stderr, "SMOKE: mouse option did not persist\n"); std::exit(1); }
        }
        if (std::getenv("LASTBEER_TEST_MENU_STYLE") && test_options == 6) {
            load_preferences();
            if (preferences.font_style != 2 || preferences.text_color != 1 || test_menu_styles != 8) {
                std::fprintf(stderr, "SMOKE: menu style did not persist\n"); std::exit(1);
            }
        }
        if (std::getenv("LASTBEER_TEST_VIDEO_ADJUST") && test_options == 4) {
            load_preferences();
            if (preferences.brightness != 1 || preferences.contrast != 149) {
                std::fprintf(stderr, "SMOKE: adjusted percentages did not persist\n"); std::exit(1);
            }
        }
        if (std::getenv("LASTBEER_TEST_DIFFICULTY_MENU") && test_options == 5) {
            load_preferences();
            if (preferences.difficulty != 2) { std::fprintf(stderr, "SMOKE: difficulty setting did not persist\n"); std::exit(1); }
        }
        if (std::getenv("LASTBEER_TEST_PLATFORM") && test_options == 5) {
            load_preferences();
            if (preferences.window_size != 1 || !preferences.fullscreen || preferences.filter != 2 ||
                preferences.scaling != 2 || preferences.scanlines != 1 || preferences.brightness != 101 || preferences.contrast != 101 ||
                preferences.input_mode != 2 ||
                preferences.guid.empty() || video->selected_controller_id() != test_pad_id ||
                preferences.bindings[0].primary != SDL_GAMEPAD_BUTTON_DPAD_UP) {
                std::fprintf(stderr, "SMOKE: platform settings did not persist\n"); std::exit(1);
            }
            if (!compat_video_set(0, 0)) { std::fprintf(stderr, "SMOKE: cannot return to windowed mode\n"); std::exit(1); }
            int width = 0, height = 0;
            if (!SDL_GetWindowSize(SDL_GetWindowFromID(video->window_id()), &width, &height) || width != 640 || height != 480) {
                std::fprintf(stderr, "SMOKE: chosen window size not applied (%d x %d)\n", width, height); std::exit(1);
            }
            std::fprintf(stderr, "SMOKE: controller and video settings persisted\n");
        }
    }
    if (test_stage == "input-settings") {
        ++test_input_settings;
        if (test_input_settings == 1)
            if (const char *capture = std::getenv("LASTBEER_CAPTURE_CONTROLS")) video->save_screenshot(capture);
        const char *menu_mode = std::getenv("LASTBEER_TEST_MENU_MODE");
        const int expected_mode = menu_mode ? (std::atoi(menu_mode) + 1) % 3 : 1;
        if (std::getenv("LASTBEER_TEST_MOUSE_MENU") && test_input_settings == 2 && preferences.input_mode != expected_mode) {
            std::fprintf(stderr, "SMOKE: mouse click did not select the input mode\n"); std::exit(1);
        }
    }
    if (test_stage == "controller-select") {
        ++test_controller_select;
        if (std::getenv("LASTBEER_TEST_PLATFORM") && test_controller_select == 3 && video->selected_controller_id() != test_pad_id) {
            std::fprintf(stderr, "SMOKE: controller selection failed\n"); std::exit(1);
        }
    }
    if (test_stage == "pad-controls") {
        ++test_pad_controls;
        if (std::getenv("LASTBEER_TEST_PLATFORM") && test_pad_controls == 4) {
            load_preferences();
            if (preferences.bindings[0].primary != SDL_GAMEPAD_BUTTON_DPAD_DOWN ||
                preferences.bindings[1].primary != SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
                std::fprintf(stderr, "SMOKE: controller binding swap or persistence failed\n"); std::exit(1);
            }
        }
        if (std::getenv("LASTBEER_TEST_PLATFORM") && test_pad_controls == 11 && preferences.bindings[0].primary != SDL_GAMEPAD_BUTTON_DPAD_UP) {
            std::fprintf(stderr, "SMOKE: controller reset failed\n"); std::exit(1);
        }
    }
    if (test_stage == "pad-bind") ++test_pad_bind;
    if (test_stage == "menu-style") {
        ++test_menu_styles;
        if (std::getenv("LASTBEER_TEST_MENU_STYLE")) {
            if ((test_menu_styles == 2 && preferences.font_style != 0) ||
                (test_menu_styles == 3 && preferences.font_style != 1) ||
                (test_menu_styles == 4 && preferences.font_style != 2) ||
                (test_menu_styles == 6 && preferences.text_color != 0) ||
                (test_menu_styles == 7 && preferences.text_color != 1)) {
                std::fprintf(stderr, "SMOKE: menu style selection failed\n"); std::exit(1);
            }
        }
    }
    if (test_stage == "video-settings") {
        ++test_video_settings;
        if (std::getenv("LASTBEER_TEST_VIDEO_ADJUST")) {
            if (test_video_settings == 1 && (!compat_video_set(6, 0) || !compat_video_set(7, 150))) {
                std::fprintf(stderr, "SMOKE: cannot set adjustment endpoints\n"); std::exit(1);
            }
            if ((test_video_settings == 8 && preferences.brightness != 0) ||
                (test_video_settings == 9 && preferences.brightness != 1) ||
                (test_video_settings == 11 && preferences.contrast != 150) ||
                (test_video_settings == 12 && preferences.contrast != 149)) {
                std::fprintf(stderr, "SMOKE: adjustment did not step or clamp correctly\n"); std::exit(1);
            }
        }
    }
    if (test_stage == "controls") {
        ++test_controls;
        if (std::getenv("LASTBEER_TEST_CONTROLS")) {
            if ((test_controls == 4 || test_controls == 5) && (key_up != KEY_DOWN || key_down != 17)) {
                std::fprintf(stderr, "SMOKE: binding swap or cancellation failed (%d, %d)\n", key_up, key_down);
                std::exit(1);
            }
            if (test_controls == 11 && (key_up != KEY_UP || key_down != KEY_DOWN)) {
                std::fprintf(stderr, "SMOKE: binding reset failed (%d, %d)\n", key_up, key_down);
                std::exit(1);
            }
        }
    }
    if (test_stage == "bind") ++test_bindings;
    test_stage_at = SDL_GetTicks();
    test_sent = false;
    std::fprintf(stderr, "SMOKE: %s\n", stage);
    std::fflush(stderr);
}
void compat_test_capture_menu(int enabled) {
    if (std::getenv("LASTBEER_CAPTURE_MENU")) SDL_SetHint(SDL_HINT_VIDEO_DUMMY_SAVE_FRAMES, enabled ? "1" : "0");
}
void compat_test_shop_stage(const char *stage, int x1, int y1, int x2, int y2) {
    if (std::getenv("LASTBEER_TEST_SHOP_CANCEL")) {
        if (strcmp(stage, "shop-buy") == 0 || strcmp(stage, "shop-card") == 0) {
            test_shop_balance_before_selection = money;
            test_shop_weapons_before_selection = nweapons;
        } else if (strcmp(stage, "shop-place") == 0 &&
                   (money != test_shop_balance_before_selection || nweapons != test_shop_weapons_before_selection)) {
            std::fprintf(stderr, "SMOKE: purchase was charged before placement\n"); std::exit(1);
        }
    }
    test_shop_x1 = x1; test_shop_y1 = y1; test_shop_x2 = x2; test_shop_y2 = y2;
    test_shop_hover_sent = false; test_shop_hover_checked = false;
    test_shop_leave_sent = false; test_shop_redraw_checked = false;
    if (strcmp(stage, "shop-buy") == 0) ++test_shop_buy;
    else if (strcmp(stage, "shop-card") == 0) ++test_shop_card;
    else if (strcmp(stage, "shop-place") == 0) ++test_shop_place;
    else if (strcmp(stage, "shop-sell") == 0) ++test_shop_sell;
    compat_test_stage(stage);
}
void compat_test_shop_purchase(long before_money, long after_money, int before_weapons, int after_weapons, int cost, int placed) {
    const bool expected_placed = test_shop_place == 2 || test_shop_place == 4;
    if (test_shop_place < 1 || test_shop_place > 4 || cost <= 0 || placed != expected_placed ||
        after_money != before_money - (placed ? cost : 0) ||
        after_weapons != before_weapons + (placed ? 1 : 0)) {
        std::fprintf(stderr, "SMOKE: shop purchase/cancel failed (step=%d placed=%d money=%ld/%ld weapons=%d/%d cost=%d)\n",
                     test_shop_place, placed, before_money, after_money, before_weapons, after_weapons, cost);
        std::exit(1);
    }
    if (test_shop_place == 4) std::fprintf(stderr, "SMOKE: shop cancellation and purchases verified\n");
}
void compat_test_shop_hover(int target, int selected, int fired) {
    if (!std::getenv("LASTBEER_TEST_SHOP_HOVER") || !test_shop_hover_sent) return;
    if (target != selected || fired) {
        std::fprintf(stderr, "SMOKE: shop hover did not immediately select without buying\n"); std::exit(1);
    }
    test_shop_hover_checked = true;
}
void compat_test_shop_exit_redraw(void) {
    if (test_stage == "shop-buy" && test_shop_buy == 1 && test_shop_leave_sent) {
        test_shop_redraw_checked = true;
        std::fprintf(stderr, "SMOKE: exit sign restored on both pages\n");
    }
}
void compat_test_score_hover(int index) {
    const int which = test_stage == "score-name" ? 0 : test_stage == "score-winner" ? 1 : -1;
    if (which < 0 || index != 3 + test_score_hover_events[which] || test_score_hover_events[which] >= 2) {
        std::fprintf(stderr, "SMOKE: wrong score letter hovered\n"); std::exit(1);
    }
    ++test_score_hover_events[which];
}
void compat_test_closing_result(int reason, uint64_t elapsed) {
    const char *mode = std::getenv("LASTBEER_TEST_CLOSING");
    if (!mode) return;
    const bool click = strcmp(mode, "click") == 0;
    if ((click && (reason != 1 || elapsed < 100 || elapsed >= 4000)) ||
        (!click && (reason != 0 || elapsed < 5000 || elapsed >= 10000))) {
        std::fprintf(stderr, "SMOKE: closing screen dismissal failed (reason=%d elapsed=%llu)\n",
                     reason, static_cast<unsigned long long>(elapsed));
        std::exit(1);
    }
    std::fprintf(stderr, "SMOKE: closing screen %s after %llu ms\n", mode, static_cast<unsigned long long>(elapsed));
}
void compat_test_frame(int ship_x, int active_shots) {
    ++test_frames;
    test_peak_shots = std::max(test_peak_shots, active_shots);
    if (test_start_x < 0 && ship_x >= 0) test_start_x = ship_x;
    if (ship_x > test_max_x) test_max_x = ship_x;
    if (active_shots > 0) test_saw_shot = true;
    if (test_frames == 5) {
        if (std::getenv("LASTBEER_TEST_MOUSE")) {
            int w = 0, h = 0;
            SDL_GetWindowSize(SDL_GetWindowFromID(video->window_id()), &w, &h);
            SDL_Event motion{}; motion.type = SDL_EVENT_MOUSE_MOTION; motion.motion.x = float(w - 1); motion.motion.y = float(h / 2); SDL_PushEvent(&motion);
            SDL_Event button{}; button.type = SDL_EVENT_MOUSE_BUTTON_DOWN; button.button.button = SDL_BUTTON_LEFT; SDL_PushEvent(&button);
        }
        else if (std::getenv("LASTBEER_TEST_PAD") || (std::getenv("LASTBEER_TEST_SHOP_MODE") && preferences.input_mode == 2)) {
            test_pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true); test_pad(SDL_GAMEPAD_BUTTON_SOUTH, true);
        }
        else {
            test_key(SDL_SCANCODE_RIGHT, true);
            if (!std::getenv("LASTBEER_TEST_FULL_FIRE")) test_key(SDL_SCANCODE_SPACE, true);
        }
    }
    if (test_frames == 20 && std::getenv("LASTBEER_TEST_FULL_FIRE")) test_key(SDL_SCANCODE_SPACE, true);
    if (test_frames == 35) {
        if (std::getenv("LASTBEER_TEST_MOUSE")) {
            SDL_Event button{}; button.type = SDL_EVENT_MOUSE_BUTTON_UP; button.button.button = SDL_BUTTON_LEFT; SDL_PushEvent(&button);
        }
        else if (std::getenv("LASTBEER_TEST_PAD") || (std::getenv("LASTBEER_TEST_SHOP_MODE") && preferences.input_mode == 2)) {
            test_pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false); test_pad(SDL_GAMEPAD_BUTTON_SOUTH, false);
        }
        else {
            test_key(SDL_SCANCODE_RIGHT, false);
            if (!std::getenv("LASTBEER_TEST_FULL_FIRE")) test_key(SDL_SCANCODE_SPACE, false);
        }
    }
    if (test_frames == 350 && std::getenv("LASTBEER_TEST_FULL_FIRE")) test_key(SDL_SCANCODE_SPACE, false);
    if (test_frames == 40 || test_frames == 50) test_key(SDL_SCANCODE_F11, true);
    if (test_frames == 42 || test_frames == 52) test_key(SDL_SCANCODE_F11, false);
    if (test_frames == 42) test_fullscreen_entered = video->fullscreen();
    if (test_frames == 52) test_fullscreen_exited = !video->fullscreen();
    if (std::getenv("LASTBEER_CAPTURE_FRAME")) {
        if (test_frames == 79) SDL_SetHint(SDL_HINT_VIDEO_DUMMY_SAVE_FRAMES, "1");
        if (test_frames == 80) SDL_SetHint(SDL_HINT_VIDEO_DUMMY_SAVE_FRAMES, "0");
    }
}
void compat_test_shot_failure(void) {
    if (std::getenv("LASTBEER_TEST_FULL_FIRE")) ++test_shot_failures;
}
#endif
}

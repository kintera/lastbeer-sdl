/* Temporary Allegro 4 subset backed by SDL 3. GPL-3.0-or-later. */
#ifndef LASTBEER_ALLEGRO_COMPAT_H
#define LASTBEER_ALLEGRO_COMPAT_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TRUE 1
#define FALSE 0
#define GFX_AUTODETECT_WINDOWED 0
#define GFX_AUTODETECT_FULLSCREEN 1
#define DIGI_AUTODETECT 0
#define MIDI_NONE 0
#define LOCK_VARIABLE(x) ((void)0)
#define LOCK_FUNCTION(x) ((void)0)
#define END_OF_FUNCTION(x)
#define BPS_TO_TIMER(x) (x)
int min(int a, int b);
int max(int a, int b);

typedef struct { unsigned char r,g,b; } RGB;
typedef RGB PALETTE[256];
typedef struct BITMAP {
    int w, h, pitch;
    unsigned char *pixels;
    unsigned char *allocation;
} BITMAP;
typedef struct SAMPLE {
    int bits, stereo, freq, priority;
    unsigned long len, loop_start, loop_end;
    void *data;
} SAMPLE;
extern BITMAP *screen;
extern char allegro_error[256];

/* Legacy scan codes: chosen to keep key[] within the original 128 entries. */
#define KEY_ESC 1
#define KEY_1 2
#define KEY_2 3
#define KEY_3 4
#define KEY_4 5
#define KEY_5 6
#define KEY_6 7
#define KEY_BACKSPACE 14
#define KEY_TAB 15
#define KEY_P 25
#define KEY_ENTER 28
#define KEY_LCONTROL 29
#define KEY_LSHIFT 42
#define KEY_RSHIFT 54
#define KEY_ASTERISK 55
#define KEY_ALT 56
#define KEY_SPACE 57
#define KEY_CAPSLOCK 58
#define KEY_F1 59
#define KEY_F2 60
#define KEY_F3 61
#define KEY_F4 62
#define KEY_F5 63
#define KEY_F6 64
#define KEY_F7 65
#define KEY_F8 66
#define KEY_F9 67
#define KEY_F10 68
#define KEY_NUMLOCK 69
#define KEY_SCRLOCK 70
#define KEY_UP 72
#define KEY_PGUP 73
#define KEY_MINUS_PAD 74
#define KEY_LEFT 75
#define KEY_5_PAD 76
#define KEY_RIGHT 77
#define KEY_PLUS_PAD 78
#define KEY_END 79
#define KEY_DOWN 80
#define KEY_PGDN 81
#define KEY_INSERT 82
#define KEY_DEL 83
#define KEY_F11 87
#define KEY_F12 88
#define KEY_RCONTROL 96

int allegro_init(void);
int install_keyboard(void);
int install_timer(void);
void install_int_ex(void (*callback)(void), int rate);
void set_leds(int leds);
uint64_t compat_ticks(void);
int install_sound(int digi, int midi, const char *config);
void remove_sound(void);
int play_sample(const SAMPLE *sample, int vol, int pan, int freq, int loop);
void stop_sample(const SAMPLE *sample);
void set_color_depth(int depth);
void request_refresh_rate(int rate);
int set_gfx_mode(int mode, int w, int h, int virtual_w, int virtual_h);
BITMAP *create_bitmap(int w, int h);
BITMAP *create_system_bitmap(int w, int h);
BITMAP *create_sub_bitmap(BITMAP *parent, int x, int y, int w, int h);
void destroy_bitmap(BITMAP *bitmap);
void clear_bitmap(BITMAP *bitmap);
void putpixel(BITMAP *bitmap, int x, int y, int color);
int getpixel(BITMAP *bitmap, int x, int y);
void rectfill(BITMAP *bitmap, int x1, int y1, int x2, int y2, int color);
void blit(BITMAP *src, BITMAP *dst, int sx, int sy, int dx, int dy, int w, int h);
void stretch_blit(BITMAP *src, BITMAP *dst, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
void masked_blit(BITMAP *src, BITMAP *dst, int sx, int sy, int dx, int dy, int w, int h);
void set_palette(const RGB *palette);
void set_color(int index, const RGB *color);
void compat_bmp_select(BITMAP *bitmap);
uintptr_t bmp_write_line(BITMAP *bitmap, int y);
void bmp_write8(uintptr_t address, int value);
void bmp_unwrite_line(BITMAP *bitmap);
#define bmp_select(bitmap) compat_bmp_select(bitmap)
void rest(int milliseconds);
void vsync(void);
int keypressed(void);
int readkey(void);
int compat_last_key_gamepad(void);
void clear_keybuf(void);
char *strupr(char *s);
const char *compat_config_path(void);
int compat_replace_file(const char *source, const char *destination);
const char *compat_key_name(int keycode);
int compat_read_binding_key(void);
int compat_controller_count(void);
const char *compat_controller_name(int index);
int compat_controller_selected(void);
void compat_controller_select(int index);
const char *compat_pad_binding_name(int action);
void compat_pad_reset(void);
void compat_pad_begin_capture(void);
int compat_pad_poll_capture(int *trigger);
void compat_pad_end_capture(void);
void compat_pad_bind(int action, int trigger);
int compat_video_setting(int option);
int compat_video_set(int option, int value);
int compat_video_window_size_count(void);
const char *compat_video_window_size_name(void);
int compat_video_read(int repeat_adjustment);
void compat_video_repeat_reset(void);
int compat_menu_poll(int repeat_adjustment, int *x, int *y);
int compat_mouse_poll(int *x, int *y);
void compat_mouse_cursor_visible(int visible);
void compat_video_clarity_controls(int visible);
void compat_video_mark_ui_color(int index);
int compat_menu_style(int option);
void compat_menu_set_style(int option, int value);
int compat_difficulty(void);
void compat_set_difficulty(int difficulty);
int compat_input_mode(void);
void compat_set_input_mode(int mode);
void compat_mouse_gameplay_begin(int x, int y);
void compat_gameplay_end(void);
int compat_mouse_target(int keyboard_moving, int *x, int *y);
int compat_mouse_fire(void);
int compat_random_index(int count);
void compat_shutdown(void);
void compat_set_executable(const char *path);
int compat_audio_probe_process(void);
void compat_show_startup(void);
void compat_skip_present(int skip);
void compat_stop_all_audio(void);
#ifdef LASTBEER_AUTOMATION
void compat_test_menu_frame(int frame);
void compat_test_menu_background(BITMAP *art);
void compat_test_hard_spawn(int foe, int slot, int x, int direction);
void compat_test_hard_progress(int x);
void compat_test_shot_power(int difficulty, int base, int actual);
void compat_test_score_view(int difficulty);
int compat_test_score_storage(void);
void compat_test_set_save_failure(int enabled);
void compat_test_prepare_score_entry(int difficulty);
int compat_test_score_entry(int difficulty);
int compat_test_video_repeat(void);
int compat_test_input_modes(void);
void compat_test_stage(const char *stage);
void compat_test_frame(int ship_x, int active_shots);
void compat_test_shop_stage(const char *stage, int x1, int y1, int x2, int y2);
void compat_test_shop_hover(int target, int selected, int fired);
void compat_test_closing_result(int reason, uint64_t elapsed);
void compat_test_capture_menu(int enabled);
#define TEST_STAGE(name) compat_test_stage(name)
#define TEST_MENU_CAPTURE(enabled) compat_test_capture_menu(enabled)
#define TEST_FRAME(x, shots) compat_test_frame(x, shots)
#else
#define TEST_STAGE(name) ((void)0)
#define TEST_MENU_CAPTURE(enabled) ((void)0)
#define TEST_FRAME(x, shots) ((void)0)
#endif
#ifdef __cplusplus
}
#endif
#endif

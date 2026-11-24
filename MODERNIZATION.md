# The Last Eichhof: SDL 3 port

The experimental SDL build runs the original C gameplay through a temporary
C++ compatibility layer. It uses SDL 3 for its window, indexed-color display,
keyboard, controller, timing and audio, and reads the original `BEER.DAT`. The legacy
Allegro 4 build remains available.

## Build and run

Requirements: CMake 3.16 or newer, C and C++20 compilers, and SDL 3 development
files. From this checkout:

```sh
cmake -S . -B build-sdl -DCMAKE_BUILD_TYPE=Release
cmake --build build-sdl
ctest --test-dir build-sdl --output-on-failure
./build-sdl/beer-sdl
```

The game searches this checkout for `BEER.DAT` and also checks its current
working directory. This is a development build, not a relocatable package.
Audio is optional: the window opens before audio initialization, and the game
continues silently if the audio check fails or times out. A temporary failure
does not change the saved sound preference. If the audio service recovers,
choose **Options → Sound → Retry**. If audio still causes startup trouble,
use `./build-sdl/beer-sdl --no-sound` to bypass it entirely.
The SDL build saves settings and scores to SDL's per-user preference
directory (typically `~/.local/share/lastbeer/CONFIG.HIG` on Linux). It uses a
separate file from Allegro 4 because the key codes differ. The score format is
still the original native binary format and is not portable across all systems.

The game uses one resizable window throughout, with a consistent 4:3 canvas.
It starts at up to 1280×960, reduced when necessary to fit the display. The
intro retains its original proportions; the surrounding space is letterboxed.
Resize the window as needed. **F11** or **Alt+Enter** toggles fullscreen on any
screen. **Alt+S** saves a BMP screenshot with the active display effects to
the `screenshots` folder in the game's SDL preference directory (typically
`~/.local/share/lastbeer/screenshots` on Linux).

Default controls:

- **Keyboard:** arrows move and navigate; Space fires or selects; Enter also
  selects; P pauses; Escape goes back or leaves the current level. Keys 1–6
  select main-menu entries directly.
- **Controller:** D-pad or left stick moves and navigates; the bottom face
  button (A / Cross) fires and selects; the right face button (B / Circle) goes
  back or leaves a level; Start pauses.

Choose **Options → Controls → Input Mode** for Keyboard Only, Keyboard & Mouse,
or Controller. The mode controls gameplay; menus accept keyboard and controller
input, and the shop also accepts mouse input. Keyboard Escape and pause remain
available during controller gameplay. In Keyboard & Mouse mode, steer the ship
toward the mouse pointer and fire with the left mouse button. The pointer starts at the bottle's center.
The beer-mug pointer can also select main-menu and Options rows. The ship remains limited to its normal
movement speed. Keyboard movement takes over until the mouse is
moved again. The selected mode is saved between runs; older mouse-enabled settings
are migrated to Keyboard & Mouse.

Open **Options → Controls → Bindings** in a keyboard mode to change one keyboard action at a time.
Escape or the controller's back button cancels a pending keyboard change. If a
key is already in use, its binding swaps with the selected action. Reset Defaults
restores all six keys. Enter, Escape and F11 are reserved for menu control and
fullscreen.

Use **Options → Controls → Select Controller** to choose a connected gamepad, or choose
Auto to accept input from all connected gamepads. A specific selection uses the
device GUID and, when available, a unique serial number. Controllers without a
unique serial are distinguished by their order among identical models, so they
may need to be selected again if their connection order changes. A missing
selected controller does not prevent keyboard navigation.

Use **Options → Controls → Controller Bindings** to rebind each action to a button or
stick direction. Selecting a used control swaps its existing action; Reset
Defaults restores the D-pad and left-stick pair for movement. Escape or holding
the controller's right face button cancels a pending binding; tap that button
to bind it. Mappings apply to all controllers, and SDL must recognize the device
as a gamepad. On the score-entry screen, move left/right between positions and
up/down to change the letter; Fire advances, or finishes when DONE is selected.
Back also finishes. Blank names and winner messages use a random spoof beer name. Close
the window to exit.

Fresh score tables start with random displayed scores from 100.01 to 499.99 and ten spoof beer brands.
Run `beer-sdl --reset-highscores` to replace all three saved score tables with
fresh defaults while keeping the other settings.
Existing player scores are preserved; empty slots are filled, and the untouched
original default table is replaced with the new defaults.

**Options → Video Settings** provides windowed/fullscreen display, automatic or
4:3 window sizes up to the current display's detected bounds, pixel-sharp, smooth and Scale2x image filters, fit, integer or stretch
scaling, adjustable scanline strength, brightness and contrast (0–150% in 1% steps), and vertical sync when supported by the SDL renderer.
Smooth scaling uses linear filtering; Scale2x softens edges in the original
pixel art. Neither is multisample antialiasing. Integer scaling may leave larger
borders, while stretch fills the window and can distort the image. The SDL renderer uploads
an indexed 320×240 gameplay framebuffer and reuses its conversion buffer. The
game catches up on short rendering delays by skipping presentations; sustained
overload can still slow it down. VSync is off by default. Hold left or right to adjust brightness or contrast
continuously; both stop at 0% and 150%. The menu controls remain readable at
extreme values so the adjustment can be reversed.

**Options → Menu Style** selects the original small font, a bold version, or
the original game's title lettering. Choose white or rainbow-colored text.
Options panels retain a subtle view of the artwork beneath them.

Choose a difficulty from the menu that appears when you select **Play**. Your
last choice is preselected and can also be changed in **Options → Difficulty**.
Classic retains the
original movement and weapon damage. Easy moves one speed unit faster and
does 25% more damage and earns twice the level money bonuses and score-based shop payout; Hard moves one unit slower, does 25% less damage, and
occasionally sends a regular enemy from the current level across the top of
the screen. Weapon-shop speed upgrades still apply. Difficulty remains fixed
for the entire run.

At each shop, Classic and Hard convert points earned since the previous shop
into money using `5 * ((points + 1500) / 2500)` with integer division. Easy
doubles that payout and the fixed level money bonuses. Classic and Hard use the same payouts.
The mouse can select items, place weapons and sell them in the between-level shop.

Each difficulty has its own high-score table. Open **High Scores** from the
main menu and use left/right to switch tables; score entry uses the difficulty
of the completed run. Existing scores remain in the Classic table.

## Possible next improvements

These ideas are informed by [ScummVM's graphics settings](https://docs.scummvm.org/en/v2026.3.0/settings/graphics.html)
and [RetroArch's quick menu](https://docs.libretro.com/guides/quick-menu/):

- Offer named display presets and a reset option, with a timed revert when a
  fullscreen or renderer change makes the display unusable.
- Improve integer scaling so gameplay can use every whole-number multiple of
  its 320×240 framebuffer. The current option scales the 640×480 logical
  canvas, so at some window sizes it leaves more border than necessary.
- Add an in-game pause menu for video and controller settings, plus separate
  music and effects volume. The existing sample mixer would need to identify
  which sounds belong to each category.
- Explore a GPU shader pipeline for adjustable CRT masks, glow and curvature.
  The current scanline effect is simpler; compatibility with RetroArch shader
  presets would require additional implementation and testing.
- Consider save states or rewind only after the game state can be serialized
  safely; the original game uses pointers and mutable global state.

## Validation and limitations

The SDL smoke test uses dummy video and audio drivers. It moves through the
intro, title and menu, starts a level, checks movement and firing, toggles
fullscreen without replacing the window, leaves the level and exits. Separate
tests cover controller event handling and virtual-controller connection,
keyboard and controller rebinding, cancellation and reset, controller selection,
video settings and persistence, settings round trips, the archive, artwork and
sound samples, plus audio startup, timeout and silent startup. They do not establish that all levels, the shop,
high-score entry, real-controller compatibility, timing or audible playback
match the original. An interactive run needs a working desktop and audio device.

The `beer-viewer` utility can inspect an original PCX and optionally play a
standalone sound:

```sh
./build-sdl/beer-viewer BEER.DAT TITLE.PCX --sound TITLE.SND
```

Omit the sound option for silent viewing. Escape closes the viewer and
Alt+Enter toggles fullscreen.

## Other build options

To run the asset-manager test without SDL or Allegro:

```sh
cmake -S . -B build-tests -DLASTBEER_BUILD_SDL=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

For the original Allegro 4 game, install its development files (including
`allegro-config`) and use:

```sh
cmake -S . -B build-legacy -DLASTBEER_BUILD_GAME=ON -DLASTBEER_BUILD_SDL=OFF
cmake --build build-legacy
./build-legacy/beer
```

The original Autotools build remains available. See [README.md](README.md) and
[LICENSE.GPL](LICENSE.GPL) for credits and licensing.

## Direction

C++ is the recommended language for new platform code because it can call the
existing C engine directly. That lets the port preserve gameplay while replacing
parts in stages. C# would require more native interop or rewriting; PyQt would
be more natural for a launcher or editor than this game loop.

The next work is interactive playtesting and comparison against Allegro 4,
including all levels, the shop, and physical gamepads, followed by replacing
the temporary compatibility layer with a smaller explicit platform interface.
Portable packaging and a portable save format remain future work.

<img src="packaging/icons/lastbeer.png" alt="The Last Eichhof icon" width="144" align="right">

# The Last Eichhof — SDL3

A modernized port of Alpha-Helix's wonderfully strange 1993 beer-themed arcade shooter. Fly the Eichhof bottle through waves of hostile drinks, buy weapons between levels, and try to make the high-score table.

This fork builds on [kilobyte/lastbeer](https://github.com/kilobyte/lastbeer), Gavin Smith's Allegro port, and keeps the original game's art, sounds and gameplay at its heart.

![Main menu with the original squirrel artwork and rainbow lettering](docs/screenshots/main-menu.png)

## What's new

- **One resizable window** throughout the game, with fullscreen, a consistent 4:3 game view, and pixel-art beer glasses in the fullscreen sidebars.
- **Keyboard and gamepad support**, including controller selection, remappable controls and menu navigation.
- **Video settings** for display size, pixel/linear/Scale2x filters, scaling, scanlines, vertical sync, brightness and contrast.
- **Easy, Classic and Hard** selectable when starting a game, with separate high-score tables for each difficulty.
- **A refreshed menu** with the original artwork, an animated pointer, arcade-style sound effects, and selectable classic, bold or title-style fonts with white or rainbow text. Menu sounds follow the Sound setting.
- **SDL3 audio and screenshots**, plus fixes for projectile cleanup and high-score entry.

Classic uses the existing movement and damage rules. Easy increases movement speed and weapon damage; Hard slows movement, reduces weapon damage and occasionally adds an enemy crossing the top of the screen. The selection stays fixed for the whole run.

| Choose your challenge | Tune the display |
| --- | --- |
| ![Difficulty selection screen](docs/screenshots/difficulty.png) | ![Video settings screen](docs/screenshots/video-settings.png) |

![Gameplay with the Eichhof bottle and classic sprites](docs/screenshots/gameplay.png)

## Download and play

Successful [package builds](https://github.com/kintera/lastbeer-sdl/actions/workflows/build-packages.yml) provide x86-64 Windows ZIP and Linux AppImage artifacts. Select a completed workflow run to download its artifacts.

- **Windows:** Extract the ZIP and run `beer-sdl.exe` from the extracted `LastEichhof` folder. Keep `SDL3.dll` and `BEER.DAT` next to the executable.
- **Linux:** Make the AppImage executable (`chmod +x LastEichhof-x86_64.AppImage`) and run it. It includes SDL3 and the game data. AppImage compatibility depends on the Linux distribution and the build system's library baseline.

## Build and run

Requirements: SDL3 development files, CMake 3.16 or newer, and C and C++20 compilers. From a checkout:

```sh
cmake -S . -B build-sdl -DCMAKE_BUILD_TYPE=Release
cmake --build build-sdl
ctest --test-dir build-sdl --output-on-failure
./build-sdl/beer-sdl
```

The game looks for `BEER.DAT` next to the executable, then in the configured data directory or current directory. Run `beer-sdl --no-sound` to bypass audio initialization. See [MODERNIZATION.md](MODERNIZATION.md) for detailed settings, build notes and the status of the port.

To reset all three high-score tables to fresh randomized defaults while keeping your other settings, run the game with `--reset-highscores`:

- **Windows:** In PowerShell, open the extracted `LastEichhof` folder and run `.\beer-sdl.exe --reset-highscores`.
- **Linux AppImage:** Run `./LastEichhof-x86_64.AppImage --reset-highscores`.

Existing high scores are otherwise preserved when updating the game.

## Controls

| Action | Keyboard | Controller |
| --- | --- | --- |
| Move / navigate | Arrow keys | D-pad or left stick |
| Fire / select | Space or Enter | Bottom face button (A / Cross) |
| Back / leave level | Escape | Right face button (B / Circle) |
| Pause | P | Start |
| Toggle fullscreen | F11 or Alt+Enter | — |
| Save screenshot | Alt+S | — |

Choose Keyboard Only, Keyboard & Mouse, or Controller under **Options → Controls**. The Bindings link opens the selected mode's keyboard or controller bindings; controller selection and controller bindings are also available there. Keyboard and controller can always navigate menus. For high-score text, use left/right to move between character positions, up/down to change the current character, and Fire to advance. Move to DONE and press Fire to finish; Back also finishes. Blank entries receive a randomly chosen spoof beer name.

Use the beer-mug pointer to select main-menu and Options rows. The pointer is visible in menus, the shop, and high scores, and hidden during gameplay. Choose **Keyboard & Mouse** to steer the ship toward the pointer and fire with the left mouse button. The pointer is placed at the bottle when a life starts, and the ship keeps its normal speed limit. Keyboard movement can take over; move the mouse again to resume mouse steering. Pause and leaving a level use the keyboard or the selected controller.

In the between-level shop, move the pointer over an item, the menu card, or the exit to highlight it; click to select it. Click inside the placement box to position a weapon. Easy doubles both level money bonuses and the money earned from points at the shop; Classic and Hard use the same payouts.

Mouse selection works in menus, the shop, and high scores in every input mode. On score entry, type directly or use left/right arrows to choose a character position and up/down to change its letter. Hover over the letters along the bottom to move the highlight, then click a letter to enter it. You can also click a character position to edit it and click DONE to submit. On the score table, click the arrows to change difficulty or BACK to leave.

## Credits and license

The original game and data are by Alpha-Helix. The original MS-DOS source was released by Dany Schoch with this notice:

> Do what ever you want with this code. Use it in your code, change it, delete it. HOWEVER: Don't blame me if it doesn't work the way it should.

The game data is copyrighted by its original authors and was described as freely distributable under the same license. The Allegro port is copyright 2014 Gavin Smith and licensed under the GNU GPL version 3; see [LICENSE.GPL](LICENSE.GPL). SDL3 is distributed under its [zlib license](packaging/SDL3-LICENSE.txt).

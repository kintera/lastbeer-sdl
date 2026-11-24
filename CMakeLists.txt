cmake_minimum_required(VERSION 3.16)
project(lastbeer VERSION 2.0 LANGUAGES C)

include(GNUInstallDirs)
include(CTest)
option(LASTBEER_BUILD_GAME "Build the original Allegro 4 game" OFF)

if(LASTBEER_BUILD_GAME)
  find_program(ALLEGRO_CONFIG allegro-config)
  if(NOT ALLEGRO_CONFIG)
    message(FATAL_ERROR "Allegro 4 development files are required (allegro-config not found). To run asset tests only, configure with -DLASTBEER_BUILD_GAME=OFF.")
  endif()
  foreach(option cflags cppflags libs)
    execute_process(COMMAND "${ALLEGRO_CONFIG}" --${option}
      OUTPUT_VARIABLE ALLEGRO_${option} OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE ALLEGRO_RESULT)
    if(NOT ALLEGRO_RESULT EQUAL 0)
      message(FATAL_ERROR "allegro-config --${option} failed")
    endif()
    separate_arguments(ALLEGRO_${option} NATIVE_COMMAND "${ALLEGRO_${option}}")
  endforeach()
  add_executable(beer fileman.c xmodeasm.c xmodec.c xmodedef.c soundc.c
    gameasm.c support.c gameplay.c shop.c menu.c baller.c hiscore.c intro.c)
  target_compile_features(beer PRIVATE c_std_99)
  target_compile_options(beer PRIVATE ${ALLEGRO_cflags} ${ALLEGRO_cppflags})
  target_compile_definitions(beer PRIVATE DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/lastbeer"
    LOCALSTATEDIR="${CMAKE_INSTALL_FULL_LOCALSTATEDIR}")
  target_link_libraries(beer PRIVATE ${ALLEGRO_libs})
  install(TARGETS beer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES BEER.DAT DESTINATION ${CMAKE_INSTALL_DATADIR}/lastbeer)
endif()

if(BUILD_TESTING)
  add_executable(test_fileman tests/test_fileman.c fileman.c)
  target_compile_features(test_fileman PRIVATE c_std_99)
  target_include_directories(test_fileman PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(test_fileman PRIVATE DATADIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME asset_manager COMMAND test_fileman "${CMAKE_CURRENT_SOURCE_DIR}/BEER.DAT" "${CMAKE_CURRENT_BINARY_DIR}/test-archive.dat")
endif()

# SDL 3 runs the original gameplay through the evolving compatibility layer.
option(LASTBEER_BUILD_SDL "Build the experimental SDL 3 game and asset viewer" ON)
if(LASTBEER_BUILD_SDL)
  set(LASTBEER_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Fallback directory for the game data archive")
  enable_language(CXX)
  find_package(SDL3 CONFIG REQUIRED)
  add_library(lastbeer_pcx STATIC modern/pcx.cpp)
  target_compile_features(lastbeer_pcx PUBLIC cxx_std_20)
  target_include_directories(lastbeer_pcx PUBLIC modern)
  add_library(lastbeer_video STATIC modern/video.cpp modern/input.cpp)
  target_compile_features(lastbeer_video PUBLIC cxx_std_20)
  target_include_directories(lastbeer_video PUBLIC modern)
  target_link_libraries(lastbeer_video PUBLIC SDL3::SDL3)
  add_library(lastbeer_audio STATIC modern/audio.cpp modern/sound.cpp)
  target_compile_features(lastbeer_audio PUBLIC cxx_std_20)
  target_include_directories(lastbeer_audio PUBLIC modern)
  target_link_libraries(lastbeer_audio PUBLIC SDL3::SDL3)
  set(LASTBEER_GAME_SOURCES fileman.c xmodeasm.c xmodec.c xmodedef.c soundc.c
    gameasm.c support.c gameplay.c shop.c menu.c baller.c hiscore.c intro.c
    modern/difficulty.c modern/compat/allegro_sdl.cpp)
  add_executable(beer-sdl ${LASTBEER_GAME_SOURCES})
  if(WIN32)
    enable_language(RC)
    configure_file(packaging/windows/icon.rc.in "${CMAKE_CURRENT_BINARY_DIR}/lastbeer-icon.rc" @ONLY)
    target_sources(beer-sdl PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/lastbeer-icon.rc")
  endif()
  target_include_directories(beer-sdl BEFORE PRIVATE modern/compat)
  target_compile_definitions(beer-sdl PRIVATE LASTBEER_SDL
    DATADIR="${LASTBEER_DATA_DIR}" LOCALSTATEDIR="${CMAKE_INSTALL_FULL_LOCALSTATEDIR}")
  target_compile_features(beer-sdl PRIVATE c_std_99 cxx_std_20)
  target_link_libraries(beer-sdl PRIVATE lastbeer_video lastbeer_audio)
  add_executable(beer-viewer modern/viewer.cpp fileman.c)
  target_include_directories(beer-viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(beer-viewer PRIVATE DATADIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(beer-viewer PRIVATE lastbeer_video lastbeer_pcx lastbeer_audio)
  if(BUILD_TESTING)
    add_executable(test_difficulty tests/test_difficulty.c modern/difficulty.c fileman.c)
    target_include_directories(test_difficulty PRIVATE modern ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(test_difficulty PRIVATE DATADIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME difficulty_rules COMMAND test_difficulty "${CMAKE_CURRENT_SOURCE_DIR}/BEER.DAT")
    add_executable(beer-sdl-automation ${LASTBEER_GAME_SOURCES})
    target_sources(beer-sdl-automation PRIVATE tests/test_projectiles.c)
    target_include_directories(beer-sdl-automation BEFORE PRIVATE modern/compat)
    target_compile_definitions(beer-sdl-automation PRIVATE LASTBEER_SDL LASTBEER_AUTOMATION
      DATADIR="${LASTBEER_DATA_DIR}" LOCALSTATEDIR="${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_features(beer-sdl-automation PRIVATE c_std_99 cxx_std_20)
    target_link_libraries(beer-sdl-automation PRIVATE lastbeer_video lastbeer_audio)
    add_test(NAME sdl_projectile_cleanup COMMAND beer-sdl-automation)
    set_tests_properties(sdl_projectile_cleanup PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;LASTBEER_TEST_PROJECTILES=1"
      PASS_REGULAR_EXPRESSION "SMOKE: projectile cleanup" TIMEOUT 15)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-full-fire")
    add_test(NAME sdl_full_ship_fire COMMAND beer-sdl-automation)
    set_tests_properties(sdl_full_ship_fire PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-full-fire;LASTBEER_TEST_FULL_FIRE=1"
      PASS_REGULAR_EXPRESSION "SMOKE: full ship fire peak=.* with no dropped shots" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-user" "${CMAKE_CURRENT_BINARY_DIR}/test-config")
    add_test(NAME sdl_gameplay_smoke COMMAND beer-sdl-automation)
    set_tests_properties(sdl_gameplay_smoke PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-user;LASTBEER_EXPECT_AUDIO=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    add_test(NAME sdl_startup_splash_duration COMMAND beer-sdl-automation)
    set_tests_properties(sdl_startup_splash_duration PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-startup-duration;LASTBEER_TEST_STARTUP_DURATION=1"
      PASS_REGULAR_EXPRESSION "SMOKE: startup screen visible for" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-gameplay-hud")
    add_test(NAME sdl_gameplay_hud COMMAND beer-sdl-automation)
    set_tests_properties(sdl_gameplay_hud PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-gameplay-hud;LASTBEER_TEST_GAMEPLAY_HUD=1"
      PASS_REGULAR_EXPRESSION "SMOKE: gameplay status bar reaches the bottom" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-start-click")
    add_test(NAME sdl_level_start_click COMMAND beer-sdl-automation)
    set_tests_properties(sdl_level_start_click PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-start-click;LASTBEER_TEST_START_CLICK=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    foreach(mode timeout click)
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-closing-${mode}")
      add_test(NAME sdl_closing_${mode} COMMAND beer-sdl-automation)
      set_tests_properties(sdl_closing_${mode} PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-closing-${mode};LASTBEER_TEST_CLOSING=${mode}"
        PASS_REGULAR_EXPRESSION "SMOKE: closing screen ${mode}" TIMEOUT 20)
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-mouse")
    add_test(NAME sdl_mouse_control COMMAND beer-sdl-automation)
    set_tests_properties(sdl_mouse_control PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-mouse;LASTBEER_TEST_MOUSE=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-input-modes")
    add_test(NAME sdl_input_modes COMMAND beer-sdl-automation)
    set_tests_properties(sdl_input_modes PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-input-modes;LASTBEER_TEST_INPUT_MODES=1"
      PASS_REGULAR_EXPRESSION "SMOKE: input modes and persistence" TIMEOUT 15)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-mouse-menu")
    add_test(NAME sdl_mouse_menus COMMAND beer-sdl-automation)
    set_tests_properties(sdl_mouse_menus PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-mouse-menu;LASTBEER_TEST_MOUSE_MENU=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-mouse-menu-controller")
    add_test(NAME sdl_mouse_menus_controller COMMAND beer-sdl-automation)
    set_tests_properties(sdl_mouse_menus_controller PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-mouse-menu-controller;LASTBEER_TEST_MOUSE_MENU=1;LASTBEER_TEST_MENU_MODE=2"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-mouse-shop")
    add_test(NAME sdl_mouse_shop COMMAND beer-sdl-automation)
    set_tests_properties(sdl_mouse_shop PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-mouse-shop;LASTBEER_TEST_SHOP_MOUSE=1"
      PASS_REGULAR_EXPRESSION "SMOKE: mouse shop purchase placement and selling" TIMEOUT 60)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-mouse-shop-hover")
    add_test(NAME sdl_mouse_shop_hover COMMAND beer-sdl-automation)
    set_tests_properties(sdl_mouse_shop_hover PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-mouse-shop-hover;LASTBEER_TEST_SHOP_MOUSE=1;LASTBEER_TEST_SHOP_HOVER=1"
      PASS_REGULAR_EXPRESSION "SMOKE: exit sign restored on both pages" TIMEOUT 60)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-shop-cancel")
    add_test(NAME sdl_shop_cancel_purchase COMMAND beer-sdl-automation)
    set_tests_properties(sdl_shop_cancel_purchase PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-shop-cancel;LASTBEER_TEST_SHOP_CANCEL=1"
      PASS_REGULAR_EXPRESSION "SMOKE: shop cancellation and purchases verified" TIMEOUT 60)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-shop-balance")
    add_test(NAME sdl_shop_balance_redraw COMMAND beer-sdl-automation)
    set_tests_properties(sdl_shop_balance_redraw PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-shop-balance;LASTBEER_TEST_SHOP_BALANCE=1"
      PASS_REGULAR_EXPRESSION "SMOKE: shop balance crosses ten correctly" TIMEOUT 45)
    foreach(mode 1 2)
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-mouse-shop-${mode}")
      add_test(NAME sdl_mouse_shop_mode_${mode} COMMAND beer-sdl-automation)
      set_tests_properties(sdl_mouse_shop_mode_${mode} PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-mouse-shop-${mode};LASTBEER_TEST_SHOP_MOUSE=1;LASTBEER_TEST_SHOP_MODE=${mode}"
        PASS_REGULAR_EXPRESSION "SMOKE: mouse shop purchase placement and selling" TIMEOUT 60)
    endforeach()
    add_test(NAME sdl_slow_render_timing COMMAND beer-sdl-automation)
    set_tests_properties(sdl_slow_render_timing PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-slow-render;LASTBEER_TEST_SLOW_RENDER=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-hard-mode")
    add_test(NAME sdl_hard_mode COMMAND beer-sdl-automation)
    set_tests_properties(sdl_hard_mode PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-hard-mode;LASTBEER_TEST_HARD_SPAWN=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-easy-mode")
    add_test(NAME sdl_easy_mode COMMAND beer-sdl-automation)
    set_tests_properties(sdl_easy_mode PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-easy-mode;LASTBEER_TEST_EASY_MODE=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    add_test(NAME sdl_score_tables COMMAND beer-sdl-automation)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-score-storage")
    set_tests_properties(sdl_score_tables PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-storage;LASTBEER_TEST_SCORE_STORAGE=1"
      PASS_REGULAR_EXPRESSION "SMOKE: difficulty score tables and classic migration" TIMEOUT 15)
    foreach(difficulty easy classic hard)
      foreach(rank first second none)
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-${difficulty}-${rank}")
        add_test(NAME sdl_score_entry_${difficulty}_${rank} COMMAND beer-sdl-automation)
        set_tests_properties(sdl_score_entry_${difficulty}_${rank} PROPERTIES
          ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-${difficulty}-${rank};LASTBEER_TEST_SCORE_ENTRY=${difficulty};LASTBEER_TEST_SCORE_RANK=${rank}"
          PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
      endforeach()
    endforeach()
    foreach(mode batch pad)
      add_test(NAME sdl_score_entry_${mode} COMMAND beer-sdl-automation)
      string(TOUPPER "${mode}" mode_upper)
      set_tests_properties(sdl_score_entry_${mode} PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-${mode};LASTBEER_TEST_SCORE_ENTRY=easy;LASTBEER_TEST_SCORE_RANK=first;LASTBEER_TEST_SCORE_${mode_upper}=1"
        PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-arrows")
    add_test(NAME sdl_score_entry_arrows COMMAND beer-sdl-automation)
    set_tests_properties(sdl_score_entry_arrows PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-arrows;LASTBEER_TEST_SCORE_ENTRY=easy;LASTBEER_TEST_SCORE_RANK=first;LASTBEER_TEST_SCORE_ARROWS=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-mixed")
    add_test(NAME sdl_score_entry_mixed COMMAND beer-sdl-automation)
    set_tests_properties(sdl_score_entry_mixed PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-mixed;LASTBEER_TEST_SCORE_ENTRY=easy;LASTBEER_TEST_SCORE_RANK=first;LASTBEER_TEST_SCORE_MIXED=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-mouse")
    add_test(NAME sdl_score_entry_mouse COMMAND beer-sdl-automation)
    set_tests_properties(sdl_score_entry_mouse PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-mouse;LASTBEER_TEST_SCORE_ENTRY=easy;LASTBEER_TEST_SCORE_RANK=first;LASTBEER_TEST_SCORE_MOUSE=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-hover")
    add_test(NAME sdl_score_entry_hover COMMAND beer-sdl-automation)
    set_tests_properties(sdl_score_entry_hover PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-hover;LASTBEER_TEST_SCORE_ENTRY=easy;LASTBEER_TEST_SCORE_RANK=first;LASTBEER_TEST_SCORE_MOUSE=1;LASTBEER_TEST_SCORE_HOVER=1"
      PASS_REGULAR_EXPRESSION "SMOKE: score letter hover verified" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-empty")
    add_test(NAME sdl_score_entry_empty COMMAND beer-sdl-automation)
    set_tests_properties(sdl_score_entry_empty PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-entry-empty;LASTBEER_TEST_SCORE_ENTRY=easy;LASTBEER_TEST_SCORE_RANK=first;LASTBEER_TEST_SCORE_EMPTY=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    add_test(NAME sdl_menu_audio_retry COMMAND beer-sdl-automation)
    set_tests_properties(sdl_menu_audio_retry PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-menu-audio;LASTBEER_TEST_MENU_AUDIO=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-menu-sfx")
    add_test(NAME sdl_menu_sound_effects COMMAND beer-sdl-automation)
    set_tests_properties(sdl_menu_sound_effects PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-menu-sfx;LASTBEER_TEST_MENU_SFX=1"
      PASS_REGULAR_EXPRESSION "SMOKE: menu sounds and mute verified" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-sound-overlap")
    add_test(NAME sdl_sound_overlap COMMAND beer-sdl-automation)
    set_tests_properties(sdl_sound_overlap PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-sound-overlap;LASTBEER_TEST_SOUND_OVERLAP=1"
      PASS_REGULAR_EXPRESSION "SMOKE: overlapping sound playback verified" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-difficulty-menu")
    add_test(NAME sdl_difficulty_menu COMMAND beer-sdl-automation)
    set_tests_properties(sdl_difficulty_menu PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-difficulty-menu;LASTBEER_TEST_DIFFICULTY_MENU=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    foreach(selection easy classic hard pad cancel back)
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-play-difficulty-${selection}")
      add_test(NAME sdl_play_difficulty_${selection} COMMAND beer-sdl-automation)
      set_tests_properties(sdl_play_difficulty_${selection} PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-play-difficulty-${selection};LASTBEER_TEST_PLAY_DIFFICULTY=${selection}"
        PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-score-view")
    add_test(NAME sdl_score_navigation COMMAND beer-sdl-automation)
    set_tests_properties(sdl_score_navigation PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-view;LASTBEER_TEST_SCORE_VIEW=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-score-mouse-view")
    add_test(NAME sdl_score_mouse_navigation COMMAND beer-sdl-automation)
    set_tests_properties(sdl_score_mouse_navigation PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-score-mouse-view;LASTBEER_TEST_SCORE_VIEW=1;LASTBEER_TEST_SCORE_MOUSE_VIEW=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-audio-hang" "${CMAKE_CURRENT_BINARY_DIR}/test-no-audio")
    add_test(NAME sdl_audio_hang_fallback COMMAND beer-sdl-automation)
    set_tests_properties(sdl_audio_hang_fallback PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-audio-hang;LASTBEER_TEST_AUDIO_HANG=1;LASTBEER_EXPECT_AUDIO_FALLBACK=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 20)
    add_test(NAME sdl_audio_open_hang_fallback COMMAND beer-sdl-automation)
    set_tests_properties(sdl_audio_open_hang_fallback PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-audio-open-hang;LASTBEER_TEST_AUDIO_OPEN_HANG=1;LASTBEER_EXPECT_AUDIO_OPEN_FALLBACK=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 20)
    add_test(NAME sdl_no_sound_startup COMMAND beer-sdl-automation --no-sound)
    set_tests_properties(sdl_no_sound_startup PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=invalid;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-no-audio;LASTBEER_EXPECT_NO_AUDIO=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 20)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-controls" "${CMAKE_CURRENT_BINARY_DIR}/test-pad")
    add_test(NAME sdl_controls_menu COMMAND beer-sdl-automation)
    set_tests_properties(sdl_controls_menu PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-controls;LASTBEER_TEST_CONTROLS=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    add_test(NAME sdl_controller_gameplay COMMAND beer-sdl-automation)
    set_tests_properties(sdl_controller_gameplay PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-pad;LASTBEER_TEST_PAD=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-platform")
    add_test(NAME sdl_platform_menus COMMAND beer-sdl-automation)
    set_tests_properties(sdl_platform_menus PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-platform;LASTBEER_TEST_PLATFORM=1"
      PASS_REGULAR_EXPRESSION "SMOKE: controller and video settings persisted" TIMEOUT 45)
    add_test(NAME sdl_settings_roundtrip COMMAND beer-sdl-automation)
    set_tests_properties(sdl_settings_roundtrip PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config;LASTBEER_TEST_CONFIG=1"
      PASS_REGULAR_EXPRESSION "SMOKE: settings roundtrip" TIMEOUT 15)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-video-adjust")
    add_test(NAME sdl_video_adjustment COMMAND beer-sdl-automation)
    set_tests_properties(sdl_video_adjustment PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-video-adjust;LASTBEER_TEST_VIDEO_ADJUST=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    add_test(NAME sdl_video_adjustment_repeat COMMAND beer-sdl-automation)
    set_tests_properties(sdl_video_adjustment_repeat PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-video-adjust;LASTBEER_TEST_VIDEO_REPEAT=1"
      PASS_REGULAR_EXPRESSION "SMOKE: held keyboard and controller video adjustment" TIMEOUT 15)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-menu-style")
    add_test(NAME sdl_menu_style COMMAND beer-sdl-automation)
    set_tests_properties(sdl_menu_style PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-menu-style;LASTBEER_TEST_MENU_STYLE=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    foreach(version 1 2 3 4 5 6 7)
      file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-video-migration-${version}")
      add_test(NAME sdl_video_migration_${version} COMMAND beer-sdl-automation)
      set_tests_properties(sdl_video_migration_${version} PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-video-migration-${version};LASTBEER_TEST_PREVIOUS_VIDEO_CONFIG=${version}"
        PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45)
    endforeach()
    add_executable(test_pcx tests/test_pcx.cpp fileman.c)
    target_include_directories(test_pcx PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(test_pcx PRIVATE DATADIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(test_pcx PRIVATE lastbeer_pcx)
    add_test(NAME original_pcx COMMAND test_pcx "${CMAKE_CURRENT_SOURCE_DIR}/BEER.DAT")
    add_executable(test_video tests/test_video.cpp)
    target_link_libraries(test_video PRIVATE lastbeer_video)
    add_test(NAME window_lifecycle COMMAND test_video)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-video-user")
    set_tests_properties(window_lifecycle PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-video-user")
    add_executable(test_input tests/test_input.cpp)
    target_link_libraries(test_input PRIVATE lastbeer_video)
    add_test(NAME input_events COMMAND test_input)
    add_executable(test_audio tests/test_audio.cpp fileman.c)
    target_include_directories(test_audio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(test_audio PRIVATE DATADIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(test_audio PRIVATE lastbeer_audio)
    add_test(NAME original_audio COMMAND test_audio "${CMAKE_CURRENT_SOURCE_DIR}/BEER.DAT")
    set_tests_properties(original_audio PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    add_test(NAME sdl_viewer_smoke COMMAND beer-viewer "${CMAKE_CURRENT_SOURCE_DIR}/BEER.DAT" TITLE.PCX --sound TITLE.SND --frames 2)
    set_tests_properties(sdl_viewer_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy")
  endif()
endif()

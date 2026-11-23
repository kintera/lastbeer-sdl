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
  target_include_directories(beer-sdl BEFORE PRIVATE modern/compat)
  target_compile_definitions(beer-sdl PRIVATE LASTBEER_SDL
    DATADIR="${CMAKE_CURRENT_SOURCE_DIR}" LOCALSTATEDIR="${CMAKE_INSTALL_FULL_LOCALSTATEDIR}")
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
      DATADIR="${CMAKE_CURRENT_SOURCE_DIR}" LOCALSTATEDIR="${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_features(beer-sdl-automation PRIVATE c_std_99 cxx_std_20)
    target_link_libraries(beer-sdl-automation PRIVATE lastbeer_video lastbeer_audio)
    add_test(NAME sdl_projectile_cleanup COMMAND beer-sdl-automation)
    set_tests_properties(sdl_projectile_cleanup PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;LASTBEER_TEST_PROJECTILES=1"
      PASS_REGULAR_EXPRESSION "SMOKE: projectile cleanup" TIMEOUT 15)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-user" "${CMAKE_CURRENT_BINARY_DIR}/test-config")
    add_test(NAME sdl_gameplay_smoke COMMAND beer-sdl-automation)
    set_tests_properties(sdl_gameplay_smoke PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-user;LASTBEER_EXPECT_AUDIO=1"
      PASS_REGULAR_EXPRESSION "SMOKE: completed" TIMEOUT 45
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
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
    set_tests_properties(sdl_score_tables PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-user;LASTBEER_TEST_SCORE_STORAGE=1"
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
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-audio-hang" "${CMAKE_CURRENT_BINARY_DIR}/test-no-audio")
    add_test(NAME sdl_audio_hang_fallback COMMAND beer-sdl-automation)
    set_tests_properties(sdl_audio_hang_fallback PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIODRIVER=dummy;XDG_DATA_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-audio-hang;LASTBEER_TEST_AUDIO_HANG=1;LASTBEER_EXPECT_AUDIO_FALLBACK=1"
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
    foreach(version 1 2 3 4 5 6)
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

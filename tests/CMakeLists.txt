cmake_minimum_required(VERSION 3.20)
project(PicoSPCPlayerHostTests LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()
option(ENABLE_SANITIZERS "Enable address/undefined sanitizers on host" OFF)
add_executable(audio_test audio_test.c
    ../src/audio/audio_status_queue.c ../src/audio/pcm_queue.c
    ../src/audio/pcm_format.c ../src/audio/test_tone.c
    ../src/visualizer/visual_rate.c)
target_include_directories(audio_test PRIVATE ../src)
target_compile_options(audio_test PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
if(ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(audio_test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(audio_test PRIVATE -fsanitize=address,undefined)
endif()
foreach(contract packing clocks queue status tone visual-rate)
    add_test(NAME audio_${contract} COMMAND audio_test ${contract})
endforeach()

add_executable(gui_test gui_test.c
    ../src/ui/ui.c ../src/ui/ui_pages.c ../src/ui/ui_aram.c ../src/ui/ui_dsp.c ../src/ui/ui_draw.c ../src/ui/ui_font.c
    ../src/ui/ui_layout.c ../src/ui/ui_library.c ../src/ui/ui_touch.c
    ../src/ui/ui_marquee.c
    ../src/ui/ui_player.c
    ../src/ui/ui_voices.c
    ../src/visualizer/aram_view.c
    ../src/storage/spc_catalog.c
    ../src/player/player_commands.c)
target_include_directories(gui_test PRIVATE ../src)
target_compile_options(gui_test PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
foreach(contract touch commands render marquee routing)
    add_test(NAME gui_${contract} COMMAND gui_test ${contract})
endforeach()

add_executable(metadata_test metadata_test.c ../src/spc/spc_metadata.c)
target_include_directories(metadata_test PRIVATE ../src)
target_compile_options(metadata_test PRIVATE
    -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME spc_metadata COMMAND metadata_test)

add_executable(snapshot_test snapshot_test.c ../src/spc/spc_snapshot.c)
target_include_directories(snapshot_test PRIVATE ../src)
target_compile_options(snapshot_test PRIVATE
    -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME spc_snapshot COMMAND snapshot_test)

add_executable(aram_activity_test aram_activity_test.c
    ../src/visualizer/aram_activity.c ../src/visualizer/aram_view.c
    ../src/ui/ui_draw.c ../src/ui/ui_font.c)
target_include_directories(aram_activity_test PRIVATE ../src)
target_compile_options(aram_activity_test PRIVATE
    -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME aram_activity COMMAND aram_activity_test)

add_executable(storage_test storage_test.c
    ../src/storage/spc_catalog.c ../src/storage/storage_mailbox.c)
target_include_directories(storage_test PRIVATE ../src)
target_compile_options(storage_test PRIVATE
    -Wall -Wextra -Werror -Wconversion -Wshadow -UNDEBUG)
add_test(NAME storage_contract COMMAND storage_test)

add_executable(gui_preview gui_preview.c
    ../src/ui/ui.c ../src/ui/ui_pages.c ../src/ui/ui_aram.c ../src/ui/ui_dsp.c ../src/ui/ui_draw.c ../src/ui/ui_font.c
    ../src/ui/ui_layout.c ../src/ui/ui_library.c ../src/ui/ui_touch.c
    ../src/ui/ui_marquee.c
    ../src/ui/ui_player.c
    ../src/ui/ui_voices.c
    ../src/visualizer/aram_view.c
    ../src/storage/spc_catalog.c
    ../src/player/player_commands.c)
target_include_directories(gui_preview PRIVATE ../src)
target_compile_options(gui_preview PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)

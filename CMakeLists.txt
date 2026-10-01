cmake_minimum_required(VERSION 3.13)

set(PICO_PLATFORM "rp2350-arm-s" CACHE STRING "Target platform")
set(PICO_BOARD "waveshare_rp2350_touch_amoled_2.41" CACHE STRING "Target board")
list(APPEND PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/boards")

if(NOT DEFINED PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    else()
        message(FATAL_ERROR "Set PICO_SDK_PATH to the Pico SDK 2.2.0 directory")
    endif()
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(PicoSPCPlayer VERSION 0.1.2 LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
pico_sdk_init()

set(PICO_SPC_EMBEDDED_FILE "" CACHE FILEPATH
    "Optional SPC file to embed in a private firmware build")

if(PICO_SPC_EMBEDDED_FILE)
    if(NOT EXISTS "${PICO_SPC_EMBEDDED_FILE}")
        message(FATAL_ERROR
            "PICO_SPC_EMBEDDED_FILE does not exist: ${PICO_SPC_EMBEDDED_FILE}")
    endif()
    get_filename_component(EMBEDDED_SPC_FILE_ABS
        "${PICO_SPC_EMBEDDED_FILE}" ABSOLUTE)
    get_filename_component(EMBEDDED_SPC_BASENAME
        "${PICO_SPC_EMBEDDED_FILE}" NAME)
    file(TO_CMAKE_PATH "${EMBEDDED_SPC_FILE_ABS}" EMBEDDED_SPC_FILE_ASM)
    configure_file(src/spc/embedded_spc.S.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_spc.S @ONLY)
    set(EMBEDDED_SPC_SOURCE
        ${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_spc.S)
    set(PICO_SPC_HAS_EMBEDDED 1)
else()
    set(EMBEDDED_SPC_SOURCE src/spc/embedded_spc_none.S)
    set(EMBEDDED_SPC_BASENAME "EMBEDDED.SPC")
    set(PICO_SPC_HAS_EMBEDDED 0)
endif()

add_library(snes_spc STATIC
    third_party/snes_spc/SNES_SPC.cpp
    third_party/snes_spc/SNES_SPC_misc.cpp
    third_party/snes_spc/SNES_SPC_state.cpp
    third_party/snes_spc/SPC_DSP.cpp
)
target_include_directories(snes_spc PUBLIC third_party/snes_spc)
target_compile_definitions(snes_spc PUBLIC SPC_ENABLE_ARAM_VISUALIZER=1)

option(PICO_SPC_EMULATOR_IN_SRAM "Place the DSP loop and SPC interpreter in SRAM" ON)
if(PICO_SPC_EMULATOR_IN_SRAM)
    target_compile_definitions(snes_spc PRIVATE PICO_SPC_EMULATOR_IN_SRAM=1)
endif()
target_compile_options(snes_spc PRIVATE -fno-exceptions -fno-rtti)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # The upstream core intentionally addresses contiguous RAM padding.
    target_compile_options(snes_spc PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-stringop-overflow>)
endif()

add_library(software_spc_backend STATIC
    src/spc/software_spc_backend.cpp
    src/spc/spc_snapshot.c
)
target_include_directories(software_spc_backend PUBLIC src src/spc)
target_link_libraries(software_spc_backend PUBLIC snes_spc)
target_compile_options(software_spc_backend PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wall;-Wextra;-Wshadow>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wshadow;-fno-exceptions;-fno-rtti>
)

set(PICO_SPC_PROJECT_SOURCES
    src/main.c
    src/app/visual_pipeline.c
    src/app/visual_consumer.c
    src/app/ui_runtime.c
    src/audio/audio_status_queue.c
    src/audio/i2s_output.c
    src/audio/pcm_format.c
    src/audio/pcm_queue.c
    src/board/board.c
    src/board/clock_validation.c
    src/board/flash_clock.c
    src/board/psram.c
    src/board/psram_validation.c
    src/display/display.c
    src/diagnostics/uart_performance.c
    src/player/player.c
    src/player/player_commands.c
    src/spc/spc_metadata.c
    src/storage/sd_card.c
    src/storage/fatfs_diskio.c
    src/storage/spc_catalog.c
    src/storage/storage_job.c
    src/storage/storage_mailbox.c
    src/storage/storage.c
    src/ui/ui.c
    src/ui/ui_pages.c
    src/ui/ui_aram.c
    src/ui/ui_dsp.c
    src/ui/ui_draw.c
    src/ui/ui_font.c
    src/ui/ui_layout.c
    src/ui/ui_library.c
    src/ui/ui_marquee.c
    src/ui/ui_player.c
    src/ui/ui_touch.c
    src/ui/ui_voices.c
    src/visualizer/aram_activity.c
    src/visualizer/aram_data.c
    src/visualizer/aram_view.c
    src/visualizer/visual_rate.c
)

set(PICO_SPC_VENDOR_SOURCES
    third_party/waveshare/amoled/AMOLED_2in41.c
    third_party/waveshare/board/DEV_Config.c
    third_party/waveshare/qspi/qspi_pio.c
    third_party/waveshare/touch/FT6336U.c
    third_party/fatfs/ff.c
    third_party/fatfs/ffunicode.c
)

add_executable(pico_spc_player
    ${EMBEDDED_SPC_SOURCE}
    ${PICO_SPC_PROJECT_SOURCES}
    ${PICO_SPC_VENDOR_SOURCES}
)
set_target_properties(pico_spc_player PROPERTIES OUTPUT_NAME "Pico-SPC-Player")
pico_set_program_name(pico_spc_player "Pico-SPC-Player")
pico_set_program_version(pico_spc_player "${PROJECT_VERSION}")

pico_generate_pio_header(pico_spc_player
    ${CMAKE_CURRENT_LIST_DIR}/src/audio/i2s_tx.pio)
pico_generate_pio_header(pico_spc_player
    ${CMAKE_CURRENT_LIST_DIR}/third_party/waveshare/qspi/qspi.pio)

target_include_directories(pico_spc_player PRIVATE
    src src/app src/audio src/board src/display src/player src/spc src/storage src/ui
    src/visualizer
    third_party/fatfs
    third_party/waveshare/amoled
    third_party/waveshare/board
    third_party/waveshare/qspi
    third_party/waveshare/touch
)
target_compile_definitions(pico_spc_player PRIVATE
    RPD_SYS_CLOCK_KHZ=250000u
    PICO_DEFAULT_UART_BAUD_RATE=115200
    PICO_SPC_BUILD_VERSION="${PROJECT_VERSION}"
    PICO_CORE1_STACK_SIZE=4096u
    PICO_SPC_HAS_EMBEDDED=${PICO_SPC_HAS_EMBEDDED}
    EMBEDDED_SPC_BASENAME="${EMBEDDED_SPC_BASENAME}"
)
option(PICO_SPC_UART_PERFORMANCE "Report performance over UART every five seconds" ON)
if(PICO_SPC_UART_PERFORMANCE)
    target_compile_definitions(pico_spc_player PRIVATE PICO_SPC_UART_PERFORMANCE=1)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(pico_spc_player PRIVATE $<$<COMPILE_LANGUAGE:C>:-fstack-usage>)
    endif()
endif()

set_source_files_properties(${PICO_SPC_PROJECT_SOURCES} PROPERTIES
    COMPILE_OPTIONS "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wall;-Wextra;-Wshadow>"
)

target_link_libraries(pico_spc_player PRIVATE
    software_spc_backend
    pico_stdlib pico_multicore
    hardware_adc hardware_clocks hardware_dma hardware_gpio hardware_i2c hardware_irq
    hardware_pio hardware_pwm hardware_spi hardware_sync hardware_xip_cache
)
pico_enable_stdio_uart(pico_spc_player 1)
pico_enable_stdio_usb(pico_spc_player 0)
pico_add_extra_outputs(pico_spc_player)

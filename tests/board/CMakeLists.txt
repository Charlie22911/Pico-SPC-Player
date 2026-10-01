cmake_minimum_required(VERSION 3.16)
project(board_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(board_psram_test
    board_psram_test.c
    ../../src/board/psram_validation.c
)
target_include_directories(board_psram_test PRIVATE ../../src/board)
if(MSVC)
    target_compile_options(board_psram_test PRIVATE /W4 /WX)
else()
    target_compile_options(board_psram_test PRIVATE -Wall -Wextra -Werror -Wpedantic)
endif()

enable_testing()
add_test(NAME board_psram_test COMMAND board_psram_test)

add_executable(board_clock_test board_clock_test.c ../../src/board/clock_validation.c)
target_include_directories(board_clock_test PRIVATE ../../src/board)
if(MSVC)
    target_compile_options(board_clock_test PRIVATE /W4 /WX /UNDEBUG)
else()
    target_compile_options(board_clock_test PRIVATE -Wall -Wextra -Werror -Wpedantic -UNDEBUG)
endif()
add_test(NAME board_clock_test COMMAND board_clock_test)

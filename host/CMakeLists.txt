cmake_minimum_required(VERSION 3.16)
project(PicoSPCPlayerHostTools LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(software_spc STATIC
    ../src/spc/software_spc_backend.cpp
    ../src/spc/spc_snapshot.c
    ../third_party/snes_spc/SNES_SPC.cpp
    ../third_party/snes_spc/SNES_SPC_misc.cpp
    ../third_party/snes_spc/SNES_SPC_state.cpp
    ../third_party/snes_spc/SPC_DSP.cpp
)
target_include_directories(software_spc PUBLIC
    ../src/spc
    ../third_party/snes_spc
)
target_compile_definitions(software_spc PUBLIC SPC_ENABLE_ARAM_VISUALIZER=1)

add_executable(spc_render spc_render.cpp)
target_link_libraries(spc_render PRIVATE software_spc)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(software_spc PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>)
    target_compile_options(spc_render PRIVATE -Wall -Wextra -Wpedantic)
endif()

cmake_minimum_required(VERSION 3.20)
project(RetroArcade VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/scenes/*.cpp
)

add_executable(retro_arcade ${SOURCES})

target_include_directories(retro_arcade PRIVATE src)

# ---------------------------------------------------------------------------
# SDL3
# Install on Pi:  sudo apt install libsdl3-dev
# Or build from source: https://github.com/libsdl-org/SDL
# ---------------------------------------------------------------------------
find_package(SDL3 REQUIRED CONFIG)
target_link_libraries(retro_arcade PRIVATE SDL3::SDL3)

# ---------------------------------------------------------------------------
# Optional: SDL3_ttf for real text rendering
# sudo apt install libsdl3-ttf-dev  (or build from source)
# Uncomment the block below if/when you add font support
# ---------------------------------------------------------------------------
# find_package(SDL3_ttf REQUIRED CONFIG)
# target_link_libraries(retro_arcade PRIVATE SDL3_ttf::SDL3_ttf)
# target_compile_definitions(retro_arcade PRIVATE USE_SDL_TTF)

# ---------------------------------------------------------------------------
# Raspberry Pi-friendly flags
# ---------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(retro_arcade PRIVATE -O2 -march=armv8-a)
endif()

# Copy assets to build dir if the folder exists
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_command(TARGET retro_arcade POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:retro_arcade>/assets"
    )
endif()

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

# Copy assets next to the executable after every build.
# Handles both "assets" and "Assets" folder names (Windows is case-insensitive
# but CMake's EXISTS is not always, so we check both).
if(EXISTS "${CMAKE_SOURCE_DIR}/Assets")
    set(ASSETS_SRC "${CMAKE_SOURCE_DIR}/Assets")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    set(ASSETS_SRC "${CMAKE_SOURCE_DIR}/assets")
endif()

if(DEFINED ASSETS_SRC)
    add_custom_command(TARGET retro_arcade POST_BUILD
        # Copy next to the .exe (Debug/ or Release/) — used when running from there
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_SRC}"
            "$<TARGET_FILE_DIR:retro_arcade>/assets"
        # Also copy to the build root — used when running Debug\retro_arcade.exe from build-win\
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_SRC}"
            "${CMAKE_BINARY_DIR}/assets"
        COMMENT "Copying assets to output directory"
    )
endif()

# Set the Visual Studio debugger working directory to the output folder so
# assets/booth.glb is found when launching from the IDE or build-win\.
set_target_properties(retro_arcade PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:retro_arcade>"
)

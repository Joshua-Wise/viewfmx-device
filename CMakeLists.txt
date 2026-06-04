cmake_minimum_required(VERSION 3.16)
project(viewfmx_device C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------
# Provider selection
#   cmake -DUSE_MOCK=ON ..    → MockProvider (offline, default)
#   cmake -DUSE_MOCK=OFF -DVIEWFMX_BASE_URL="http://host:3000"
#         -DVIEWFMX_RESOURCE_ID="456" -DVIEWFMX_BUILDING_ID="123" ..
# -----------------------------------------------------------------------
option(USE_MOCK "Use the mock data provider (no network required)" ON)
set(VIEWFMX_BASE_URL    "http://localhost:3000" CACHE STRING "viewfmx server base URL")
set(VIEWFMX_RESOURCE_ID "0"                     CACHE STRING "GoFMX resource (room) ID")
set(VIEWFMX_BUILDING_ID "0"                     CACHE STRING "GoFMX building ID")

# -----------------------------------------------------------------------
# LVGL v9 via FetchContent
# Tell LVGL where to find our lv_conf.h before the fetch.
# -----------------------------------------------------------------------
set(LV_CONF_PATH "${CMAKE_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)

include(FetchContent)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.2.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(lvgl)

# LVGL's custom.cmake globs all *.S files including ARM Helium/NEON assembly
# that the macOS assembler cannot handle.  Strip them from the lvgl target.
get_target_property(_lvgl_srcs lvgl SOURCES)
list(FILTER _lvgl_srcs EXCLUDE REGEX ".*\\.(S|s)$")
set_property(TARGET lvgl PROPERTY SOURCES "${_lvgl_srcs}")

# LVGL's SDL driver needs SDL2 headers.
# Link SDL2::SDL2 so its INTERFACE_INCLUDE_DIRECTORIES propagate into LVGL.
target_link_libraries(lvgl PRIVATE SDL2::SDL2)

# -----------------------------------------------------------------------
# SDL2  (brew install sdl2)
# Homebrew on Apple Silicon does not ship SDL2Config.cmake, so we fall
# back to PkgConfig, which sdl2-config provides.
# -----------------------------------------------------------------------
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS})
endif()

# -----------------------------------------------------------------------
# libcurl  (brew install curl — already present on macOS)
# -----------------------------------------------------------------------
find_package(CURL REQUIRED)

# -----------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------
set(SOURCES
    main.c
    vendor/cjson/cJSON.c
    gui/gui.c
    gui/screens/room_screen.c
    data/mock_provider.c
    data/http_provider.c
    platform/sdl/display.c
    platform/sdl/input.c
    platform/sdl/net.c
)

add_executable(viewfmx_device ${SOURCES})

# -----------------------------------------------------------------------
# Preprocessor defines from CMake options
# -----------------------------------------------------------------------
target_compile_definitions(viewfmx_device PRIVATE
    VIEWFMX_USE_MOCK=$<BOOL:${USE_MOCK}>
    VIEWFMX_BASE_URL="${VIEWFMX_BASE_URL}"
    VIEWFMX_RESOURCE_ID="${VIEWFMX_RESOURCE_ID}"
    VIEWFMX_BUILDING_ID="${VIEWFMX_BUILDING_ID}"
)

# -----------------------------------------------------------------------
# Include paths
# -----------------------------------------------------------------------
target_include_directories(viewfmx_device PRIVATE
    ${CMAKE_SOURCE_DIR}           # lv_conf.h lives here
    ${CMAKE_SOURCE_DIR}/gui
    ${CMAKE_SOURCE_DIR}/data
    ${CMAKE_SOURCE_DIR}/platform
    ${CMAKE_SOURCE_DIR}/vendor
    ${SDL2_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

# -----------------------------------------------------------------------
# Link
# -----------------------------------------------------------------------
target_link_libraries(viewfmx_device PRIVATE
    lvgl
    SDL2::SDL2
    ${CURL_LIBRARIES}
)

# -----------------------------------------------------------------------
# macOS Apple Silicon: ensure we don't accidentally cross-compile.
# The default arch matches brew's SDL2 (arm64 on M-series).
# Do NOT pass -DCMAKE_OSX_ARCHITECTURES=x86_64 or linking will fail.
# -----------------------------------------------------------------------
if(APPLE)
    message(STATUS "Building for macOS — SDL2 arch must match: ${CMAKE_OSX_ARCHITECTURES}")
endif()

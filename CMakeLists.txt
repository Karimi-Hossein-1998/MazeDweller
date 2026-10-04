cmake_minimum_required(VERSION 3.16)
project(MazeDweller CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build optimized by default.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Vendored Dear ImGui.
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)

# --- SDL3 ---------------------------------------------------------------
# Desktop uses the system SDL3 by default. Android and Emscripten must build
# SDL3 for their own target, so the vendored source is added as a subdirectory.
# CI can set MAZEDWELLER_VENDORED_SDL=ON to build the vendored SDL3 on desktop
# too (avoiding a system dependency).
option(MAZEDWELLER_VENDORED_SDL "Build the vendored SDL3 instead of the system one" OFF)
if((ANDROID OR EMSCRIPTEN OR MAZEDWELLER_VENDORED_SDL) AND NOT TARGET SDL3::SDL3 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/SDL/CMakeLists.txt)
    if(EMSCRIPTEN)
        # Mount persistent storage (IndexedDB) so saves survive page reloads.
        set(SDL_EMSCRIPTEN_PERSISTENT_PATH "/storage" CACHE STRING "" FORCE)
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/SDL SDL3)
endif()
if(NOT TARGET SDL3::SDL3)
    find_package(SDL3 REQUIRED)
endif()

# --- Target -------------------------------------------------------------
# Android builds SDL apps as a shared library (loaded by SDLActivity).
set(MAZEDWELLER_SOURCES
    main.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

if(ANDROID)
    add_library(MazeDweller SHARED ${MAZEDWELLER_SOURCES})
    # SDLActivity loads "main" (see getLibraries() in SDLActivity.java).
    set_target_properties(MazeDweller PROPERTIES OUTPUT_NAME main)
else()
    add_executable(MazeDweller ${MAZEDWELLER_SOURCES})
endif()

target_include_directories(MazeDweller PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(MazeDweller PRIVATE SDL3::SDL3)

# --- Emscripten (web) ---------------------------------------------------
# Produce index.html + index.js + index.wasm, which GitHub Pages serves.
if(EMSCRIPTEN)
    set_target_properties(MazeDweller PROPERTIES
        OUTPUT_NAME "index"
        SUFFIX ".html"
    )
    target_link_options(MazeDweller PRIVATE
        "-sALLOW_MEMORY_GROWTH=1"
        "-sMAXIMUM_MEMORY=1gb"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
endif()

# --- Install (desktop Linux) --------------------------------------------
# Saves go to SDL_GetPrefPath() at runtime; this installs the binary, icon and
# .desktop entry to the system.
if(NOT ANDROID AND NOT EMSCRIPTEN AND UNIX)
    include(GNUInstallDirs)
    install(TARGETS MazeDweller RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/MazeDweller.svg
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/MazeDweller.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
endif()

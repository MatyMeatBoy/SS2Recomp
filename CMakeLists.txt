cmake_minimum_required(VERSION 3.20)
project(ShiningSoul2Recomp VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(ANDROID)
    set(_netplay_default OFF)
else()
    set(_netplay_default ON)
endif()
option(GBARECOMP_NETPLAY "Build desktop link-cable netplay (up to 4 consoles)" ${_netplay_default})

set(GBARECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp"
    CACHE PATH "Path to this game's gbarecomp worktree")
set(RECOMP_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/recomp-ui"
    CACHE PATH "Path to the shared recomp-ui checkout")
set(GBARECOMP_RUNTIME_UI_ROOT "${RECOMP_UI_ROOT}"
    CACHE PATH "recomp-ui root used by the live GBA runtime adapter")
set(GBARECOMP_SELFHEAL_RECOMPILE_DEFAULT ON CACHE BOOL
    "Enable Shining Soul II runtime self-heal sharding by default" FORCE)
set(GBARECOMP_ENABLE_MODS ON CACHE BOOL
    "Enable Shining Soul II's mod catalog and GBA mod packages" FORCE)
# BIOS-derived recompiled output stays in this build tree, never in the
# gbarecomp checkout (tools/regen_bios.ps1 writes it here).
set(GBARECOMP_GENERATED_BIOS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_bios"
    CACHE PATH "Locally generated BIOS recompilation sources")
if(NOT EXISTS "${GBARECOMP_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "GBARECOMP_ROOT is not a gbarecomp checkout: ${GBARECOMP_ROOT}")
endif()

add_subdirectory("${GBARECOMP_ROOT}" gbarecomp_build EXCLUDE_FROM_ALL)

set(GBA_GAME_GENERATED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/generated" CACHE PATH
    "Generated native corpus")
if(EXISTS "${GBA_GAME_GENERATED_ROOT}/recompiled.cpp")
    message(FATAL_ERROR
        "Stale monolithic generated/recompiled.cpp found. Run tools/regen.ps1.")
endif()
file(GLOB GAME_GENERATED_SOURCES CONFIGURE_DEPENDS
    "${GBA_GAME_GENERATED_ROOT}/*.c"
    "${GBA_GAME_GENERATED_ROOT}/*.cpp")

add_executable(ShiningSoul2Recomp
    src/main.cpp
    src/ss2_extended_view.cpp
    src/mods/ss2_adaptive_view_plugin.cpp
    ${GAME_GENERATED_SOURCES})

# Generated guest shards stay cheaper to compile than the runtime.
set(GAME_RECOMPILED_SOURCES ${GAME_GENERATED_SOURCES})
list(FILTER GAME_RECOMPILED_SOURCES INCLUDE REGEX
    "/recompiled_[0-9]+\\.cpp$")
if(GAME_RECOMPILED_SOURCES)
    set_source_files_properties(${GAME_RECOMPILED_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-O1;-g0")
endif()

target_include_directories(ShiningSoul2Recomp PRIVATE
    "${GBARECOMP_ROOT}/src/armv4t"
    "${GBARECOMP_ROOT}/src/gba"
    "${GBARECOMP_ROOT}/src/runtime"
    "${GBARECOMP_ROOT}/src/debug"
    src)

target_link_libraries(ShiningSoul2Recomp PRIVATE
    "-Wl,--start-group"
    gbarecomp_runtime
    gbarecomp_debug
    gbarecomp_gba
    gbarecomp_armv4t
    "-Wl,--end-group")

target_compile_definitions(ShiningSoul2Recomp PRIVATE
    GBARECOMP_DEFAULT_DEBUG_PORT=19901
    GBARECOMP_WINDOW_TITLE="ShiningSoul2Recomp"
    GBARECOMP_DEFAULT_GAME_CONFIG="game.toml")
if(GBARECOMP_NETPLAY)
    target_link_libraries(ShiningSoul2Recomp PRIVATE gbarecomp_netplay)
    gbarecomp_target_netplay_identity(ShiningSoul2Recomp
        "${CMAKE_CURRENT_SOURCE_DIR}" "${GBA_GAME_GENERATED_ROOT}")
    set_property(SOURCE src/main.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS GBAGAME_NETPLAY=1)
endif()

# recomp-ui stays isolated from the generated shards; only this bridge and
# main.cpp know the launcher exists.
option(GBAGAME_RECOMP_UI "Build the recomp-ui pre-boot launcher" ON)
if(GBAGAME_RECOMP_UI)
    include(${RECOMP_UI_ROOT}/recomp_ui.cmake)
    add_library(game_launcher_ui STATIC src/game_launcher_boot.cpp)
    target_include_directories(game_launcher_ui PRIVATE
        "${GBARECOMP_ROOT}/src/runtime"
        "${GBARECOMP_ROOT}/src/gba"
        src)
    target_compile_definitions(game_launcher_ui PRIVATE
        GBARECOMP_ENABLE_MODS=1)
    recomp_target_launcher_ui(game_launcher_ui
        CONSOLE gba
        BOXART ${CMAKE_CURRENT_SOURCE_DIR}/recomp/launcher/boxart.tga)
    if(GBARECOMP_NETPLAY)
        gbarecomp_target_netplay_launcher(game_launcher_ui)
    endif()
    find_package(SDL2 REQUIRED)
    target_link_libraries(game_launcher_ui PUBLIC SDL2::SDL2)
    if(WIN32)
        target_link_libraries(game_launcher_ui PUBLIC opengl32)
    endif()
    target_link_libraries(ShiningSoul2Recomp PRIVATE game_launcher_ui)
    set_property(SOURCE src/main.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS GBAGAME_RECOMP_UI)
endif()

# Ship the game-owned, disabled-by-default enhancement packages beside the
# executable so recomp-ui can expose them in the Mods tab.
set(SS2_PRELOADED_MODS "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded")
if(EXISTS "${SS2_PRELOADED_MODS}/packages")
    add_custom_command(TARGET ShiningSoul2Recomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SS2_PRELOADED_MODS}"
            "$<TARGET_FILE_DIR:ShiningSoul2Recomp>/mods"
        COMMENT "Preloading the Shining Soul II mod catalog")
endif()

if(MSVC)
    target_link_options(ShiningSoul2Recomp PRIVATE /STACK:16777216)
elseif(MINGW OR WIN32)
    target_link_options(ShiningSoul2Recomp PRIVATE -Wl,--stack,16777216)
endif()

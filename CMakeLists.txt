cmake_minimum_required(VERSION 3.16)
project(lv VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(ExternalProject)
include(FetchContent)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS regex)

if(WIN32)
    # ---- PDCursesMod (Windows Console, wide-char / UTF-8) ---------------------
    # PDCursesMod's own CMake build assumes it is the top-level project (it
    # references CMAKE_SOURCE_DIR throughout), so it breaks when pulled in via
    # FetchContent/add_subdirectory. Fetch the sources only (SOURCE_SUBDIR
    # points at a directory without a CMakeLists.txt, so MakeAvailable skips
    # its configure step) and compile the wincon backend directly, mirroring
    # the Linux GL backend setup below.
    FetchContent_Declare(
        pdcurses
        GIT_REPOSITORY https://github.com/Bill-Gray/PDCursesMod.git
        GIT_TAG        v4.4.0
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  cmake
    )
    FetchContent_MakeAvailable(pdcurses)

    file(GLOB PDC_CORE_SRCS ${pdcurses_SOURCE_DIR}/pdcurses/*.c)
    add_library(pdcurses STATIC
        ${PDC_CORE_SRCS}
        ${pdcurses_SOURCE_DIR}/wincon/pdcclip.c
        ${pdcurses_SOURCE_DIR}/wincon/pdcdisp.c
        ${pdcurses_SOURCE_DIR}/wincon/pdcgetsc.c
        ${pdcurses_SOURCE_DIR}/wincon/pdckbd.c
        ${pdcurses_SOURCE_DIR}/wincon/pdcscrn.c
        ${pdcurses_SOURCE_DIR}/wincon/pdcsetsc.c
        ${pdcurses_SOURCE_DIR}/wincon/pdcutil.c
    )
    target_include_directories(pdcurses PUBLIC
        ${pdcurses_SOURCE_DIR} ${pdcurses_SOURCE_DIR}/wincon)
    target_compile_definitions(pdcurses
        PUBLIC PDC_WIDE PDC_FORCE_UTF8 CHTYPE_32
        PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(pdcurses PUBLIC
        gdi32 winspool shell32 ole32 comdlg32 advapi32 winmm)

    set(LV_TUI_LIBS pdcurses)
    set(LV_TUI_INCLUDE_DIRS "")
    set(LV_TUI_COMPILE_OPTS "")
    if(MSVC)
        list(APPEND LV_TUI_COMPILE_OPTS /utf-8 /W4 /permissive-)
        # Silence C runtime deprecation noise for POSIX-named functions.
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    else()
        list(APPEND LV_TUI_COMPILE_OPTS -Wall -Wextra -Wpedantic)
    endif()
else()
    # ---- ncurses (built from source via ExternalProject) ----------------------
    set(NCURSES_PREFIX ${CMAKE_BINARY_DIR}/_ncurses)
    file(MAKE_DIRECTORY ${NCURSES_PREFIX}/include)

    ExternalProject_Add(ncurses_build
        URL https://ftp.gnu.org/gnu/ncurses/ncurses-6.5.tar.gz
        PREFIX ${CMAKE_BINARY_DIR}/_ncurses_build
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CONFIGURE_COMMAND <SOURCE_DIR>/configure
            --prefix=${NCURSES_PREFIX}
            --with-shared
            --without-debug
            --enable-widec
            --enable-overwrite
            --without-ada
            --without-manpages
            --without-progs
            --without-tests
        BUILD_COMMAND make -j
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS
            ${NCURSES_PREFIX}/lib/libncursesw${CMAKE_SHARED_LIBRARY_SUFFIX}
    )

    add_library(lv_ncurses SHARED IMPORTED)
    set_target_properties(lv_ncurses PROPERTIES
        IMPORTED_LOCATION ${NCURSES_PREFIX}/lib/libncursesw${CMAKE_SHARED_LIBRARY_SUFFIX}
        INTERFACE_INCLUDE_DIRECTORIES ${NCURSES_PREFIX}/include
    )
    add_dependencies(lv_ncurses ncurses_build)

    # NOTE: libformw is no longer needed after the LineEditor rewrite (Phase A
    # of the Windows port). Kept here as documentation; safe to remove.

    set(LV_TUI_LIBS lv_ncurses)
    set(LV_TUI_INCLUDE_DIRS ${NCURSES_PREFIX}/include)
    set(LV_TUI_COMPILE_OPTS -Wall -Wextra -Wpedantic)
endif()

add_library(lv_core
    src/core/bit_array.cpp
    src/core/config.cpp
    src/core/filter_engine.cpp
    src/core/filter_result.cpp
    src/core/line_index.cpp
    src/core/mmap_file.cpp
    src/core/path_util.cpp
    src/core/rule.cpp
    src/core/rule_set.cpp
)
target_include_directories(lv_core PUBLIC src)
target_link_libraries(lv_core PUBLIC Boost::regex)
if(MSVC)
    target_compile_options(lv_core PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(lv_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(lv_ui
    src/ui/app_ui.cpp
    src/ui/line_editor.cpp
    src/ui/screen.cpp
)
target_include_directories(lv_ui PUBLIC src)
if(LV_TUI_INCLUDE_DIRS)
    target_include_directories(lv_ui PRIVATE ${LV_TUI_INCLUDE_DIRS})
endif()
target_link_libraries(lv_ui PUBLIC lv_core ${LV_TUI_LIBS} Boost::regex)
if(MSVC)
    target_compile_options(lv_ui PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(lv_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lv src/main.cpp)
target_link_libraries(lv PRIVATE lv_core lv_ui)
if(MSVC)
    target_compile_options(lv PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(lv PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(test_filter_engine tests/test_filter_engine.cpp)
target_include_directories(test_filter_engine PRIVATE src tests)
target_link_libraries(test_filter_engine PRIVATE lv_core)
if(MSVC)
    target_compile_options(test_filter_engine PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(test_filter_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(test_line_editor tests/test_line_editor.cpp)
target_include_directories(test_line_editor PRIVATE src tests)
target_link_libraries(test_line_editor PRIVATE lv_ui)
if(MSVC)
    target_compile_options(test_line_editor PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(test_line_editor PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(perf_filter_engine tests/perf_filter_engine.cpp)
target_include_directories(perf_filter_engine PRIVATE src tests)
target_link_libraries(perf_filter_engine PRIVATE lv_core)
if(MSVC)
    target_compile_options(perf_filter_engine PRIVATE /utf-8 /W4 /permissive- /O2)
else()
    target_compile_options(perf_filter_engine PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()

# ----------------------------------------------------------------------------
# Optional GUI binary (glv): cross-platform SDL2 window.
# Enabled with -DLV_BUILD_GUI=ON. Vendored third-party sources live in
# third_party/ and are built as static libs.
# ----------------------------------------------------------------------------
option(LV_BUILD_GUI "Build the cross-platform GUI binary (SDL2)" OFF)

if(LV_BUILD_GUI)
    set(LV_THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

    # ---- SDL2 (static, minimal subsystems) -----------------------------------
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_POWER OFF CACHE BOOL "" FORCE)
    set(SDL_FILE OFF CACHE BOOL "" FORCE)
    set(SDL_FILESYSTEM OFF CACHE BOOL "" FORCE)
    set(SDL_CPUINFO OFF CACHE BOOL "" FORCE)
    set(SDL_DUMMYAUDIO ON CACHE BOOL "" FORCE)
    set(SDL_DISKAUDIO OFF CACHE BOOL "" FORCE)
    # OpenGL is required: PDCursesMod's gl backend uses it.
    set(SDL_OPENGL ON CACHE BOOL "" FORCE)
    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
    add_subdirectory(${LV_THIRDPARTY_DIR}/SDL2 EXCLUDE_FROM_ALL)

    # ---- SDL2_ttf (static, with vendored FreeType) ---------------------------
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${LV_THIRDPARTY_DIR}/SDL2_ttf EXCLUDE_FROM_ALL)

    # ---- PDCursesMod OpenGL backend (compiled directly as a static lib) ------
    file(GLOB PDC_CORE_SRCS ${LV_THIRDPARTY_DIR}/PDCursesMod/pdcurses/*.c)
    set(PDC_GL_SRCS
        ${LV_THIRDPARTY_DIR}/PDCursesMod/gl/pdcclip.c
        ${LV_THIRDPARTY_DIR}/PDCursesMod/gl/pdcdisp.c
        ${LV_THIRDPARTY_DIR}/PDCursesMod/gl/pdcgetsc.c
        ${LV_THIRDPARTY_DIR}/PDCursesMod/gl/pdckbd.c
        ${LV_THIRDPARTY_DIR}/PDCursesMod/gl/pdcscrn.c
        ${LV_THIRDPARTY_DIR}/PDCursesMod/gl/pdcsetsc.c
        ${LV_THIRDPARTY_DIR}/PDCursesMod/gl/pdcutil.c
    )
    add_library(pdcurses_sdl2 STATIC ${PDC_CORE_SRCS} ${PDC_GL_SRCS})
    target_include_directories(pdcurses_sdl2 PUBLIC ${LV_THIRDPARTY_DIR}/PDCursesMod)
    target_compile_definitions(pdcurses_sdl2 PUBLIC PDC_WIDE PDC_FORCE_UTF8 CHTYPE_32)
    target_link_libraries(pdcurses_sdl2 PUBLIC SDL2_ttf::SDL2_ttf-static SDL2::SDL2-static)

    # ---- lv_ui_gui: lv_ui recompiled with PDCursesMod backend ----------------
    add_library(lv_ui_gui STATIC
        src/ui/app_ui.cpp
        src/ui/line_editor.cpp
        src/ui/screen.cpp
    )
    target_include_directories(lv_ui_gui PUBLIC src)
    target_compile_definitions(lv_ui_gui PUBLIC LV_USE_PDCURSES)
    target_link_libraries(lv_ui_gui PUBLIC lv_core pdcurses_sdl2 Boost::regex)
    if(MSVC)
        target_compile_options(lv_ui_gui PRIVATE /utf-8 /W4 /permissive-)
    else()
        target_compile_options(lv_ui_gui PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    # ---- glv executable ---------------------------------------------------
    add_executable(glv
        src/gui/main_gui.cpp
    )
    target_include_directories(glv PRIVATE src)
    target_link_libraries(glv PRIVATE lv_core lv_ui_gui SDL2::SDL2main)
    find_package(OpenGL REQUIRED)
    target_link_libraries(glv PRIVATE OpenGL::GL ${CMAKE_DL_LIBS})
    if(MSVC)
        target_compile_options(glv PRIVATE /utf-8 /W4 /permissive-)
    else()
        target_compile_options(glv PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

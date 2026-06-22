cmake_minimum_required(VERSION 3.16)
project(lv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(ExternalProject)
include(FetchContent)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS regex)

if(WIN32)
    # ---- PDCursesMod (Windows Console, wide-char / UTF-8) ---------------------
    set(PDC_WIDE      Y CACHE STRING "" FORCE)
    set(PDC_UTF8      Y CACHE STRING "" FORCE)
    set(PDC_BUILD_DLL OFF CACHE STRING "" FORCE)
    set(PDC_BUILD_TESTS OFF CACHE STRING "" FORCE)
    set(PDC_BUILD_DEMOS OFF CACHE STRING "" FORCE)

    FetchContent_Declare(
        pdcurses
        GIT_REPOSITORY https://github.com/Bill-Gray/PDCursesMod.git
        GIT_TAG        v4.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(pdcurses)

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

cmake_minimum_required(VERSION 3.16)
project(lv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(ExternalProject)

set(NCURSES_PREFIX ${CMAKE_BINARY_DIR}/_ncurses)
file(MAKE_DIRECTORY ${NCURSES_PREFIX}/include)

ExternalProject_Add(ncurses_build
    URL https://ftp.gnu.org/gnu/ncurses/ncurses-6.5.tar.gz
    PREFIX ${CMAKE_BINARY_DIR}/_ncurses_build
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
        ${NCURSES_PREFIX}/lib/libformw${CMAKE_SHARED_LIBRARY_SUFFIX}
)

add_library(lv_ncurses SHARED IMPORTED)
set_target_properties(lv_ncurses PROPERTIES
    IMPORTED_LOCATION ${NCURSES_PREFIX}/lib/libncursesw${CMAKE_SHARED_LIBRARY_SUFFIX}
    INTERFACE_INCLUDE_DIRECTORIES ${NCURSES_PREFIX}/include
)
add_dependencies(lv_ncurses ncurses_build)

add_library(lv_form SHARED IMPORTED)
set_target_properties(lv_form PROPERTIES
    IMPORTED_LOCATION ${NCURSES_PREFIX}/lib/libformw${CMAKE_SHARED_LIBRARY_SUFFIX}
    INTERFACE_INCLUDE_DIRECTORIES ${NCURSES_PREFIX}/include
)
add_dependencies(lv_form ncurses_build)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS regex)

add_library(lv_core
    src/core/bit_array.cpp
    src/core/filter_engine.cpp
    src/core/filter_result.cpp
    src/core/line_index.cpp
    src/core/mmap_file.cpp
    src/core/rule.cpp
    src/core/rule_set.cpp
)
target_include_directories(lv_core PUBLIC src)
target_link_libraries(lv_core PUBLIC Boost::regex)
target_compile_options(lv_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(lv_ui
    src/ui/app_ui.cpp
    src/ui/line_editor.cpp
    src/ui/screen.cpp
)
target_include_directories(lv_ui PUBLIC src)
target_include_directories(lv_ui PRIVATE ${NCURSES_PREFIX}/include)
target_link_libraries(lv_ui PUBLIC lv_core lv_form lv_ncurses Boost::regex)
target_compile_options(lv_ui PRIVATE -Wall -Wextra -Wpedantic)

add_executable(lv src/main.cpp)
target_link_libraries(lv PRIVATE lv_core lv_ui)
target_compile_options(lv PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_filter_engine tests/test_filter_engine.cpp)
target_include_directories(test_filter_engine PRIVATE src tests)
target_link_libraries(test_filter_engine PRIVATE lv_core)
target_compile_options(test_filter_engine PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_line_editor tests/test_line_editor.cpp)
target_include_directories(test_line_editor PRIVATE src tests)
target_link_libraries(test_line_editor PRIVATE lv_ui)
target_compile_options(test_line_editor PRIVATE -Wall -Wextra -Wpedantic)

add_executable(perf_filter_engine tests/perf_filter_engine.cpp)
target_include_directories(perf_filter_engine PRIVATE src tests)
target_link_libraries(perf_filter_engine PRIVATE lv_core)
target_compile_options(perf_filter_engine PRIVATE -Wall -Wextra -Wpedantic -O2)

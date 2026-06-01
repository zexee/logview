cmake_minimum_required(VERSION 3.16)
project(lv VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Curses REQUIRED)

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
target_compile_options(lv_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(lv_ui
    src/ui/app_ui.cpp
    src/ui/line_editor.cpp
    src/ui/screen.cpp
)
target_include_directories(lv_ui PUBLIC src)
target_include_directories(lv_ui PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(lv_ui PUBLIC lv_core PRIVATE ${CURSES_LIBRARIES})
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

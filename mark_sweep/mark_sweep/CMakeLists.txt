cmake_minimum_required(VERSION 3.15)
project(MarkSweepGC VERSION 1.0.0 LANGUAGES CXX)

# ===========================
# Версия C++
# ===========================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Добавить флаги для отладки
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ===========================
# Найти зависимости
# ===========================
find_package(nlohmann_json 3.2.0 QUIET)

# ===========================
# Включаемые файлы
# ===========================
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ===========================
# Исходные файлы ядра
# ===========================
set(CORE_SOURCES
    src/mark_sweep_gc.cpp
    src/cascade_deletion_gc.cpp
)

set(CORE_HEADERS
    include/gc_interface.h
    include/heap_object.h
    include/mark_sweep_gc.h
    include/cascade_deletion_gc.h
)

# ===========================
# EXECUTABLE 1: Simulator
# ===========================
add_executable(simulator
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    src/simulator.cpp
)

target_include_directories(simulator
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(simulator PRIVATE /W4)
else()
    target_compile_options(simulator PRIVATE -Wall -Wextra -Wpedantic -g)
endif()

set_target_properties(simulator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ===========================
# EXECUTABLE 2: Performance Tests
# ===========================
add_executable(perf_test
    ${CORE_SOURCES}
    ${CORE_HEADERS}
    src/performance_test.cpp
    src/perf_main.cpp
)

target_include_directories(perf_test
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(perf_test PRIVATE /W4)
else()
    target_compile_options(perf_test PRIVATE -Wall -Wextra -Wpedantic -g)
endif()

# Линковать nlohmann_json если найден
if(nlohmann_json_FOUND)
    target_link_libraries(perf_test
        PRIVATE
        nlohmann_json::nlohmann_json
    )
else()
    message(WARNING "nlohmann_json not found. JSON output may not work.")
    message(STATUS "Install with: apt install nlohmann-json3-dev")
endif()

set_target_properties(perf_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ===========================
# Output directory
# ===========================
set_target_properties(simulator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ===========================
# Install targets (optional)
# ===========================
install(TARGETS simulator perf_test DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/scenarios DESTINATION . OPTIONAL)

cmake_minimum_required(VERSION 3.25)
project(system_level_programming LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers (clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Threading support
find_package(Threads REQUIRED)

# Optional system libraries
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBURING QUIET liburing)
    pkg_check_modules(LIBNUMA QUIET numa)
    pkg_check_modules(LIBELF QUIET libelf)
endif()

# Compiler Configuration & Strict Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -march=native
        -pthread
    )

    # Optimization flags for Release
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -fno-math-errno)
    endif()
elseif(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus /arch:AVX2)
endif()

# Sanitizer Options
option(ENABLE_ASAN "Enable Address and Undefined Behavior Sanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ENABLE_ASAN and ENABLE_TSAN are mutually exclusive. Enable only one sanitizer at a time.")
endif()

if(ENABLE_ASAN AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
    message(STATUS "Configuring AddressSanitizer and UndefinedBehaviorSanitizer (-fsanitize=address,undefined)")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

if(ENABLE_TSAN AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang"))
    message(STATUS "Configuring ThreadSanitizer (-fsanitize=thread)")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
endif()

# Interface library for common low-latency infrastructure
add_library(sys_common INTERFACE)
target_include_directories(sys_common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/common>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(sys_common INTERFACE Threads::Threads)

# Common unit test & benchmark harness target
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_common.cpp")
    add_executable(test_common test_common.cpp)
    target_link_libraries(test_common PRIVATE sys_common)
endif()

# Auto-discover and add module subdirectories (01-* through 09-*)
file(GLOB MODULE_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "[0-9][0-9]-*")
foreach(DIR ${MODULE_DIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${DIR}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/CMakeLists.txt")
        message(STATUS "Discovered module: ${DIR}")
        add_subdirectory(${DIR})
    endif()
endforeach()

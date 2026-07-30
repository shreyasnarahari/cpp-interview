cmake_minimum_required(VERSION 3.20)
project(practice-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable strict warning flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Sanitizer options
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_ASAN AND NOT MSVC)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_TSAN AND NOT MSVC)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# Main playground executable
add_executable(practice main.cpp)

# Micro-benchmarking laboratory targets
add_executable(vtable_vs_crtp benchmarks/vtable_vs_crtp.cpp)
add_executable(smart_pointer_overhead benchmarks/smart_pointer_overhead.cpp)
add_executable(container_iteration benchmarks/container_iteration.cpp)
add_executable(false_sharing_demo benchmarks/false_sharing_demo.cpp)

# false_sharing_demo needs pthread
find_package(Threads REQUIRED)
target_link_libraries(false_sharing_demo PRIVATE Threads::Threads)

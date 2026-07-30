# Intermediate Questions: Modern CMake Best Practices

### Q1: Write a modern, clean `CMakeLists.txt` for a C++20 library and test runner executable.
```cmake
cmake_minimum_required(VERSION 3.20)
project(CoreEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(engine_lib STATIC src/engine.cpp)
target_include_directories(engine_lib PUBLIC include)

add_executable(engine_test tests/main.cpp)
target_link_libraries(engine_test PRIVATE engine_lib)
```

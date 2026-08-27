# Allocators Test Suite
add_executable(test_allocators test_allocators.cpp)
target_include_directories(test_allocators PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_allocators PRIVATE sys_common)

# Allocators Benchmark Harness
add_executable(bench_allocators bench_allocators.cpp)
target_include_directories(bench_allocators PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bench_allocators PRIVATE sys_common)

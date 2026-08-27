# Module 03: CPU Microarchitecture & Cache-Conscious C++

add_executable(bench_simd exercises/aos_vs_soa_simd.cpp benchmarks/bench_simd.cpp)
target_include_directories(bench_simd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(bench_simd PRIVATE sys_common)
target_compile_options(bench_simd PRIVATE -mavx2 -mfma)

# Mini-project subdirectories
add_subdirectory(projects/vectorized_math_pipeline)

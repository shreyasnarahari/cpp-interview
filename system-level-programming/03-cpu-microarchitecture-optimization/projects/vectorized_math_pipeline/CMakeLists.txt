add_executable(test_pipeline test_pipeline.cpp)
target_include_directories(test_pipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_pipeline PRIVATE sys_common)
target_compile_options(test_pipeline PRIVATE -mavx2 -mfma)

add_executable(bench_pipeline bench_pipeline.cpp)
target_include_directories(bench_pipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bench_pipeline PRIVATE sys_common)
target_compile_options(bench_pipeline PRIVATE -mavx2 -mfma)

# Capstone 02: Embedded LSM-Tree Key-Value Database

add_executable(test_storage_engine test_storage_engine.cpp)
target_include_directories(test_storage_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_storage_engine PRIVATE sys_common)

add_executable(bench_storage_engine bench_storage_engine.cpp)
target_include_directories(bench_storage_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bench_storage_engine PRIVATE sys_common)

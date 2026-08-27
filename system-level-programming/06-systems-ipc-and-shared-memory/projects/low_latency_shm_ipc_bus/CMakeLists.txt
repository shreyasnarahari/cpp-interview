add_executable(test_shm_ipc test_shm_ipc.cpp)
target_include_directories(test_shm_ipc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_shm_ipc PRIVATE sys_common rt)

add_executable(bench_shm_ipc bench_shm_ipc.cpp)
target_include_directories(bench_shm_ipc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bench_shm_ipc PRIVATE sys_common rt)

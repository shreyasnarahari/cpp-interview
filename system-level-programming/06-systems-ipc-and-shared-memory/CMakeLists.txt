# Module 06: Systems IPC and Shared Memory

add_executable(shm_futex_sync exercises/shm_futex_sync.cpp)
target_include_directories(shm_futex_sync PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(shm_futex_sync PRIVATE sys_common rt)

add_executable(test_shm_futex tests/test_shm_futex.cpp)
target_include_directories(test_shm_futex PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(test_shm_futex PRIVATE sys_common rt)

# Mini-project subdirectories
add_subdirectory(projects/low_latency_shm_ipc_bus)

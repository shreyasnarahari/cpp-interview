# Module 04: Hardware Synchronization & Lock-Free Internals

add_executable(test_memory_orders tests/test_memory_orders.cpp)
target_include_directories(test_memory_orders PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(test_memory_orders PRIVATE sys_common)

# Mini-project subdirectories
add_subdirectory(projects/lockfree_ring_buffers)

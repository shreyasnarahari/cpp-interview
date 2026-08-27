# Module 02: Virtual Memory Subsystem & Custom Allocators

# Page fault exercises & tests
add_executable(page_fault_tracker exercises/page_fault_tracker.cpp)
target_include_directories(page_fault_tracker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(page_fault_tracker PRIVATE sys_common)

add_executable(test_page_fault_tracker tests/test_page_fault_tracker.cpp)
target_include_directories(test_page_fault_tracker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(test_page_fault_tracker PRIVATE sys_common)

# Mini-project subdirectories
add_subdirectory(projects/slab_and_buddy_allocator)

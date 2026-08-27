# Module 07: Database Storage Engine & Buffer Pool Management

add_executable(slotted_page_demo exercises/slotted_page_layout.cpp)
target_include_directories(slotted_page_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(slotted_page_demo PRIVATE sys_common)

add_executable(test_slotted_page tests/test_slotted_page.cpp)
target_include_directories(test_slotted_page PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(test_slotted_page PRIVATE sys_common)

# Mini-project subdirectories
add_subdirectory(projects/concurrent_buffer_pool_manager)

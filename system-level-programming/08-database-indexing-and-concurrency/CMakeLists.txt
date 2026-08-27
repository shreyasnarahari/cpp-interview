# Module 08: Database Indexing & Index Concurrency

add_executable(latch_crabbing_demo exercises/latch_crabbing_btree.cpp)
target_include_directories(latch_crabbing_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(latch_crabbing_demo PRIVATE sys_common)

add_executable(test_latch_crabbing tests/test_latch_crabbing.cpp)
target_include_directories(test_latch_crabbing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(test_latch_crabbing PRIVATE sys_common)

# Mini-project subdirectories
add_subdirectory(projects/concurrent_lsm_tree_engine)

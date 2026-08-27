# Module 09: Transaction Processing, MVCC, and Recovery

add_executable(mvcc_traversal_demo exercises/mvcc_undo_log_traversal.cpp)
target_include_directories(mvcc_traversal_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(mvcc_traversal_demo PRIVATE sys_common)

add_executable(test_mvcc tests/test_mvcc.cpp)
target_include_directories(test_mvcc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(test_mvcc PRIVATE sys_common)

# Mini-project subdirectories
add_subdirectory(projects/mini_transactional_engine)

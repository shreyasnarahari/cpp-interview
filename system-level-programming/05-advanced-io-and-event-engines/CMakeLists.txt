# Module 05: Advanced I/O and Event Engines

add_executable(epoll_server exercises/epoll_edge_triggered.cpp)
target_include_directories(epoll_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(epoll_server PRIVATE sys_common)

add_executable(test_epoll_server tests/test_epoll_server.cpp)
target_include_directories(test_epoll_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(test_epoll_server PRIVATE sys_common)

# Mini-project subdirectories
add_subdirectory(projects/iouring_echo_reactor)

add_executable(iouring_server server_main.cpp)
target_include_directories(iouring_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iouring_server PRIVATE sys_common)

add_executable(test_iouring test_iouring.cpp)
target_include_directories(test_iouring PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_iouring PRIVATE sys_common)

add_executable(bench_client bench_client.cpp)
target_include_directories(bench_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bench_client PRIVATE sys_common)

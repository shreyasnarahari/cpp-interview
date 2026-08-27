add_executable(elf_inspector main.cpp)
target_include_directories(elf_inspector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(elf_inspector PRIVATE sys_common)

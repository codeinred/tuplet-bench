cmake_minimum_required(VERSION 3.0.0)
project(tuplet-bench CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(tuplet-bench main.cpp)

add_subdirectory(tuplet)
add_subdirectory(arglet)
add_subdirectory(fmt)
target_link_libraries(tuplet-bench tuplet arglet fmt)


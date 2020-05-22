cmake_minimum_required(VERSION 3.17)
project(io_uring_tutorial)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=leak")

add_executable(io_uring_cat cat.cpp)
add_executable(io_uring_cp cp.cpp)

target_link_libraries(io_uring_cat uring)
target_link_libraries(io_uring_cp uring)

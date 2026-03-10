cmake_minimum_required(VERSION 3.20)
project(task2)

set(CMAKE_CXX_STANDARD 11)
find_package(OpenMP)

add_executable(dgemv src/dgemv.cpp)
add_executable(integral src/integral.cpp)
add_executable(system src/system_of_equations.cpp)

target_link_libraries(dgemv PRIVATE OpenMP::OpenMP_CXX)
target_link_libraries(integral PRIVATE OpenMP::OpenMP_CXX)
target_link_libraries(system PRIVATE OpenMP::OpenMP_CXX)

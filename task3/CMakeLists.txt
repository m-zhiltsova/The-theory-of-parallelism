cmake_minimum_required(VERSION 3.20)
project(task2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(OpenMP)

add_executable(dgemv src/dgemv.cpp)
add_executable(app src/app.cpp)

target_link_libraries(app PRIVATE OpenMP::OpenMP_CXX)
target_link_libraries(dgemv PRIVATE OpenMP::OpenMP_CXX)

cmake_minimum_required(VERSION 3.20)
project(task3)

set(CMAKE_CXX_STANDARD 11)
find_package(OpenMP)

add_executable(dgemv src/dgemv.cpp)
add_executable(app src/app.cpp)

target_link_libraries(dgemv PRIVATE OpenMP::OpenMP_CXX)
target_link_libraries(matvec_thread_async PRIVATE Threads::Threads)

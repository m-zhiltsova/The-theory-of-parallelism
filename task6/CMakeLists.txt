cmake_minimum_required(VERSION 3.20)
project(HeatEquation LANGUAGES CXX)

find_package(Boost REQUIRED COMPONENTS program_options)

set(CUDA_INC "/opt/nvidia/hpc_sdk/Linux_x86_64/23.11/cuda/12.3/targets/x86_64-linux/include")

set(COMMON_SRCS main.cpp)

add_executable(main_gpu ${COMMON_SRCS})
target_compile_options(main_gpu PRIVATE -acc -Minfo=all -std=c++17)
target_include_directories(main_gpu PRIVATE ${Boost_INCLUDE_DIRS} ${CUDA_INC})
target_link_libraries(main_gpu PRIVATE ${Boost_LIBRARIES})

add_executable(main_multicore ${COMMON_SRCS})
target_compile_options(main_multicore PRIVATE -acc=multicore -Minfo=all -std=c++17)
target_include_directories(main_multicore PRIVATE ${Boost_INCLUDE_DIRS} ${CUDA_INC})
target_link_libraries(main_multicore PRIVATE ${Boost_LIBRARIES})

add_executable(main_host ${COMMON_SRCS})
target_compile_options(main_host PRIVATE -acc=host -Minfo=all -std=c++17)
target_include_directories(main_host PRIVATE ${Boost_INCLUDE_DIRS} ${CUDA_INC})
target_link_libraries(main_host PRIVATE ${Boost_LIBRARIES})

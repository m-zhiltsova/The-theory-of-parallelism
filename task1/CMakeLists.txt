cmake_minimum_required(VERSION 3.20)
project(array_of_sin_values)

set(CMAKE_CXX_STANDARD 11)

add_executable(array_of_sin_values src/main.cpp)


option(USE_DOUBLE "Using double type" OFF)
option(USE_FLOAT "Using float type" OFF)

if (USE_FLOAT AND USE_DOUBLE)
	message(WARNING "You can't use both float and double. Float type is building.")
	add_definitions(-DUSE_FLOAT)

elseif (NOT USE_FLOAT AND USE_DOUBLE)
	add_definitions(-DUSE_DOUBLE)

elseif (USE_FLOAT AND NOT USE_DOUBLE)
	add_definitions(-DUSE_FLOAT)

elseif (NOT USE_FLOAT AND NOT USE_DOUBLE)
	message(WARNING "You didn't set type of array. Float type is building.")
	add_definitions(-DUSE_FLOAT)

endif()

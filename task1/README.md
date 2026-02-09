## Task 1
### For float type
    cmake -B build -S . -DUSE_FLOAT=ON
    cmake --build build
    ./build/array_of_sin_values

#### Output
Float sum: -0.0277862

### For double type
    cmake -B build -S . -DUSE_DOUBLE=ON
    cmake --build build
    ./build/array_of_sin_values

#### Output
Double sum: 4.89582e-11

### If both -DUSE_DOUBLE and -DUSE_FLOAT are ON:
    cmake -B build -S . -DUSE_FLOAT=ON -DUSE_DOUBLE=ON
    cmake --build build
    ./build/array_of_sin_values

#### Output
CMake Warning at CMakeLists.txt:13 (message):

You can't use both float and double.  Float type is building.

...

Float sum: -0.0277862

### If both -DUSE_DOUBLE and -DUSE_FLOAT are OFF:
    cmake -B build -S .
    cmake --build build
    ./build/array_of_sin_values

#### Output
CMake Warning at CMakeLists.txt:23 (message):

You didn't set type of array.  Float type is building.

...

Float sum: -0.0277862

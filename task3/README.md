## Task 3

### Умножение матрицы на вектор
    cmake -B build -S .
    g++ -std=c++11 -pthread -O2 dgemv.cpp -o dgemv 

### Клиент-серверное приложение
    cmake -B build -S .
    cmake --build build --target app

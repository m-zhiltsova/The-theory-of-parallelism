## Task 3

### Умножение матрицы на вектор
    cmake -B build -S .
    g++ -std=c++11 -pthread -O2 matvec_thread_only.cpp -o matvec_thread_only

### Клиент-серверное приложение
    cmake -B build -S .
    cmake --build build --target app

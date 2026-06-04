# Загрузка 
wget https://developer.download.nvidia.com/hpc-sdk/26.3/nvhpc_2026_263_Linux_x86_64_cuda_13.1.tar.gz
tar xpzf nvhpc_2026_263_Linux_x86_64_cuda_13.1.tar.gz
nvhpc_2026_263_Linux_x86_64_cuda_13.1/install

# Компиляция
cmake -B build -S . -DCMAKE_CXX_COMPILER=pgc++
cmake --build build

# Запуск
## 128*128 
./build/main_host --size 128 --tol 1e-6 --max_iter 1000000
./build/main_multicore --size 128 --tol 1e-6 --max_iter 1000000
./build/main_gpu --size 128 --tol 1e-6 --max_iter 1000000

## 256*256
./build/main_host --size 256 --tol 1e-6 --max_iter 1000000
./build/main_multicore --size 256 --tol 1e-6 --max_iter 1000000
./build/main_gpu --size 256 --tol 1e-6 --max_iter 1000000

## 512*512
./build/main_host --size 512 --tol 1e-6 --max_iter 1000000
./build/main_multicore --size 512 --tol 1e-6 --max_iter 1000000
./build/main_gpu --size 512 --tol 1e-6 --max_iter 1000000

## 1024*1024
./build/main_host --size 1024 --tol 1e-6 --max_iter 1000000
./build/main_multicore --size 1024 --tol 1e-6 --max_iter 1000000
./build/main_gpu --size 1024 --tol 1e-6 --max_iter 1000000


# Профилирование
nsys profile -t cuda,openacc -o report ./main_X --size X --max_iter 50

wget https://developer.download.nvidia.com/hpc-sdk/26.3/nvhpc_2026_263_Linux_x86_64_cuda_13.1.tar.gz
tar xpzf nvhpc_2026_263_Linux_x86_64_cuda_13.1.tar.gz
nvhpc_2026_263_Linux_x86_64_cuda_13.1/install


cmake -B build -S . -DCMAKE_CXX_COMPILER=pgc++
cmake --build build
./build/main_gpu --size 256 --tol 1e-6 --max_iter 1000000

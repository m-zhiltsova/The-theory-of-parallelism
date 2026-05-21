#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iomanip>

// Последовательное умножение
void matvec_serial(const std::vector<double>& a, const std::vector<double>& b,
                   std::vector<double>& c, size_t m, size_t n) {
    for (size_t i = 0; i < m; ++i) {
        double sum = 0.0;
        for (size_t j = 0; j < n; ++j) {
            sum += a[i * n + j] * b[j];
        }
        c[i] = sum;
    }
}

// Параллельное умножение с std::thread
void matvec_thread(const std::vector<double>& a, const std::vector<double>& b,
                   std::vector<double>& c, size_t m, size_t n, int nthreads) {
    std::vector<std::thread> threads;
    auto worker = [&](size_t lb, size_t ub) {
        for (size_t i = lb; i <= ub; ++i) {
            double sum = 0.0;
            for (size_t j = 0; j < n; ++j) {
                sum += a[i * n + j] * b[j];
            }
            c[i] = sum;
        }
    };
    size_t chunk = m / nthreads;
    size_t rem = m % nthreads;
    size_t start = 0;
    for (int t = 0; t < nthreads; ++t) {
        size_t end = start + chunk - 1 + (t < rem ? 1 : 0);
        if (end >= m) end = m - 1;
        threads.emplace_back(worker, start, end);
        start = end + 1;
    }
    for (auto& th : threads) th.join();
}

// Параллельная инициализация матрицы и вектора
void init_parallel_threads(std::vector<double>& a, std::vector<double>& b,
                           size_t m, size_t n, int nthreads) {
    std::vector<std::thread> threads;

    // Инициализация вектора b
    auto init_b = [&](size_t lb, size_t ub) {
        for (size_t j = lb; j <= ub; ++j)
            b[j] = static_cast<double>(j);
    };
    size_t chunk_b = n / nthreads;
    size_t rem_b = n % nthreads;
    size_t start = 0;
    for (int t = 0; t < nthreads; ++t) {
        size_t end = start + chunk_b - 1 + (t < rem_b ? 1 : 0);
        if (end >= n) end = n - 1;
        threads.emplace_back(init_b, start, end);
        start = end + 1;
    }
    for (auto& th : threads) th.join();
    threads.clear();

    // Инициализация матрицы a
    auto init_a = [&](size_t lb, size_t ub) {
        for (size_t i = lb; i <= ub; ++i) {
            for (size_t j = 0; j < n; ++j) {
                a[i * n + j] = static_cast<double>(i + j);
            }
        }
    };
    size_t chunk_m = m / nthreads;
    size_t rem_m = m % nthreads;
    start = 0;
    for (int t = 0; t < nthreads; ++t) {
        size_t end = start + chunk_m - 1 + (t < rem_m ? 1 : 0);
        if (end >= m) end = m - 1;
        threads.emplace_back(init_a, start, end);
        start = end + 1;
    }
    for (auto& th : threads) th.join();
}

int main() {
    const int warmup = 1;
    const int measure_runs = 5;
    std::vector<int> thread_counts = {1, 2, 4, 7, 8, 16, 20, 40};
    std::vector<size_t> sizes = {20000, 40000};

    const int init_threads = std::thread::hardware_concurrency();
    std::cout << "Using " << init_threads << " threads for parallel initialization.\n";

    std::ofstream out("matvec_thread_results.txt");
    out << std::fixed << std::setprecision(6);
    out << "Size Threads Serial Thread\n";

    for (size_t size : sizes) {
        size_t m = size, n = size;
        std::cout << "\nMatrix size: " << m << "x" << n << std::endl;

        // Выделение памяти и параллельная инициализация
        std::vector<double> a(m * n);
        std::vector<double> b(n);
        init_parallel_threads(a, b, m, n, init_threads);

        std::vector<double> c(m);

        // Замер последовательного времени
        double t_serial = 0.0;
        // Прогрев
        for (int i = 0; i < warmup; ++i) {
            std::fill(c.begin(), c.end(), 0.0);
            matvec_serial(a, b, c, m, n);
        }
        for (int i = 0; i < measure_runs; ++i) {
            std::fill(c.begin(), c.end(), 0.0);
            auto start = std::chrono::steady_clock::now();
            matvec_serial(a, b, c, m, n);
            auto end = std::chrono::steady_clock::now();
            t_serial += std::chrono::duration<double>(end - start).count();
        }
        t_serial /= measure_runs;
        std::cout << "Serial   : " << t_serial << " s" << std::endl;

        // Замеры для многопоточного варианта (std::thread)
        for (int th : thread_counts) {
            if (th == 1) {
                out << size << " " << th << " " << t_serial << " " << t_serial << "\n";
                continue;
            }
            double t_thread = 0.0;
            // Прогрев
            for (int i = 0; i < warmup; ++i) {
                std::fill(c.begin(), c.end(), 0.0);
                matvec_thread(a, b, c, m, n, th);
            }
            for (int i = 0; i < measure_runs; ++i) {
                std::fill(c.begin(), c.end(), 0.0);
                auto start = std::chrono::steady_clock::now();
                matvec_thread(a, b, c, m, n, th);
                auto end = std::chrono::steady_clock::now();
                t_thread += std::chrono::duration<double>(end - start).count();
            }
            t_thread /= measure_runs;
            std::cout << "Threads " << th << " | Time: " << t_thread << " s" << std::endl;
            out << size << " " << th << " " << t_serial << " " << t_thread << "\n";
        }
    }

    out.close();
    std::cout << "\nResults saved to matvec_thread_results.txt\n";
    return 0;
}

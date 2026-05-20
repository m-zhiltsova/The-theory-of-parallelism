#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iomanip>

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

void init_parallel_threads(std::vector<double>& a, std::vector<double>& b,
                           size_t m, size_t n, int nthreads) {
    std::vector<std::thread> threads;

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

void matvec_async(const std::vector<double>& a, const std::vector<double>& b,
                  std::vector<double>& c, size_t m, size_t n, int nthreads) {
    std::vector<std::future<void>> futures;
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
        futures.push_back(std::async(std::launch::async, worker, start, end));
        start = end + 1;
    }
    for (auto& f : futures) f.get();
}

template <typename F>
double measure_mult(F&& mult_func,
                    const std::vector<double>& a,
                    const std::vector<double>& b,
                    std::vector<double>& c,
                    size_t m, size_t n, int nthreads,
                    int warmup, int measure_runs) {
    for (int i = 0; i < warmup; ++i) {
        std::fill(c.begin(), c.end(), 0.0);
        mult_func(a, b, c, m, n, nthreads);
    }
    double total = 0.0;
    for (int i = 0; i < measure_runs; ++i) {
        std::fill(c.begin(), c.end(), 0.0);
        auto start = std::chrono::steady_clock::now();
        mult_func(a, b, c, m, n, nthreads);
        auto end = std::chrono::steady_clock::now();
        total += std::chrono::duration<double>(end - start).count();
    }
    return total / measure_runs;
}

int main() {
    const int warmup = 1;
    const int measure_runs = 5;
    std::vector<int> thread_counts = {1, 2, 4, 7, 8, 16, 20, 40};
    std::vector<size_t> sizes = {20000, 40000};

    const int init_threads = std::thread::hardware_concurrency();
    std::cout << "Using " << init_threads << " threads for parallel initialization.\n";

    std::ofstream out("matvec_thread_async_results.txt");
    out << std::fixed << std::setprecision(6);
    out << "Size Threads Serial Thread Async\n";

    for (size_t size : sizes) {
        size_t m = size, n = size;
        std::cout << "\nMatrix size: " << m << "x" << n << std::endl;

        std::vector<double> a(m * n);
        std::vector<double> b(n);
        init_parallel_threads(a, b, m, n, init_threads);

        std::vector<double> c(m);

        double t_serial = measure_mult(matvec_serial, a, b, c, m, n, 1, warmup, measure_runs);
        std::cout << "Serial   : " << t_serial << " s" << std::endl;

        for (int th : thread_counts) {
            if (th == 1) {
                out << size << " " << th << " " << t_serial << " " << t_serial << " " << t_serial << "\n";
                continue;
            }
            double t_thread = measure_mult(matvec_thread, a, b, c, m, n, th, warmup, measure_runs);
            double t_async  = measure_mult(matvec_async,  a, b, c, m, n, th, warmup, measure_runs);
            std::cout << "Threads " << th
                      << " | Thread: " << t_thread << " s"
                      << " | Async: "  << t_async  << " s" << std::endl;
            out << size << " " << th << " " << t_serial << " " << t_thread << " " << t_async << "\n";
        }
    }

    out.close();
    std::cout << "\nResults saved to matvec_thread_async_results.txt\n";
    return 0;
}

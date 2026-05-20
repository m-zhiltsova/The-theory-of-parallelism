#include <iostream>
#include <omp.h>
#include <vector>
#include <chrono>
#include <string.h>
#include <fstream>
#include <algorithm>

using namespace std;

<<<<<<< HEAD
void serial_mult(const double* a, const double* b, double* c, size_t m, size_t n) {
    for (size_t i = 0; i < m; ++i) {
        double sum = 0.0;
        for (size_t j = 0; j < n; ++j) {
            sum += a[i * n + j] * b[j];
        }
        c[i] = sum;
    }
}

void parallel_mult(const double* a, const double* b, double* c, size_t m, size_t n, int count_t) {
    #pragma omp parallel num_threads(count_t)
    {
        int nthreads = omp_get_num_threads();
        int threadid = omp_get_thread_num();
        int items_per_thread = m / nthreads;
        int lb = threadid * items_per_thread;
        int ub = (threadid == nthreads - 1) ? (m - 1) : (lb + items_per_thread - 1);

        for (int i = lb; i <= ub; ++i) {
            double sum = 0.0;
            for (size_t j = 0; j < n; ++j) {
                sum += a[i * n + j] * b[j];
            }
            c[i] = sum;
        }
    }
=======
void matrix_vector_product(size_t m, size_t n)
{
	vector<double> a(m*n);
	vector<double> b(n);
	vector<double> c(m);

	for (size_t i = 0; i < m; i++)
	{
		for (size_t j = 0; j < n; j++)
			a[i*n+j] = i + j;
	}

	for (size_t j = 0; j < n; j++)
		b[j] = j;

	for (int i = 0; i < m; i++)
	{
		c[i] = 0.0;
		for (int j = 0; j < n; j++)
			c[i] += a[i*n+j] * b[j];
	}
}

void matrix_vector_product_omp(size_t m, size_t n, size_t count_t)
{
	vector<double> a(m*n);
	vector<double> c(m);


	vector<double> b(n);

	// #pragma omp parallel for num_threads(count_t) schedule(static)
	for (int j = 0; j < n; j++)
		b[j] = j;

	#pragma omp parallel for num_threads(count_t) schedule(static)
	for (int i = 0; i < m; i++)
	{	
		c[i] = 0.0;
		for (int j = 0; j < n; j++){
			a[i*n+j] = i + j;
			c[i] += a[i*n+j] * b[j];
		}
	}
>>>>>>> refs/remotes/origin/main
}

void parallel_init(double* a, double* b, size_t m, size_t n, int count_t) {
    #pragma omp parallel num_threads(count_t)
    {
        int nthreads = omp_get_num_threads();
        int threadid = omp_get_thread_num();
        int items_per_thread = n / nthreads;
        int lb = threadid * items_per_thread;
        int ub = (threadid == nthreads - 1) ? (n - 1) : (lb + items_per_thread - 1);
        for (size_t j = lb; j <= ub; ++j) {
            b[j] = static_cast<double>(j);
        }
    }

    #pragma omp parallel num_threads(count_t)
    {
        int nthreads = omp_get_num_threads();
        int threadid = omp_get_thread_num();
        int items_per_thread = m / nthreads;
        int lb = threadid * items_per_thread;
        int ub = (threadid == nthreads - 1) ? (m - 1) : (lb + items_per_thread - 1);

        for (size_t i = lb; i <= ub; ++i) {
            for (size_t j = 0; j < n; ++j) {
                a[i * n + j] = static_cast<double>(i + j);
            }
        }
    }
}

double run_serial(size_t m, size_t n, const vector<double>& a, const vector<double>& b, vector<double>& c) {
    const auto start = chrono::steady_clock::now();
    serial_mult(a.data(), b.data(), c.data(), m, n);
    const auto end = chrono::steady_clock::now();
    return chrono::duration<double>(end - start).count();
}

<<<<<<< HEAD
double run_parallel(size_t m, size_t n, int count_t, 
                    const vector<double>& a, const vector<double>& b, vector<double>& c) {
    const auto start = chrono::steady_clock::now();
    parallel_mult(a.data(), b.data(), c.data(), m, n, count_t);
    const auto end = chrono::steady_clock::now();
    return chrono::duration<double>(end - start).count();
}
=======
int main()
{
	vector<int> threads_array = {2, 4, 7, 8, 16, 20, 40};
	vector<int> size_array = {20000, 40000};
	vector<double> res(16);
>>>>>>> refs/remotes/origin/main

int main() {
    const int warmup_runs = 1;
    const int measure_runs = 5;

    int threads_array[] = {1, 2, 4, 7, 8, 16, 20, 40};
    int num_threads_counts = sizeof(threads_array) / sizeof(threads_array[0]);
    size_t sizes[] = {20000, 40000};

<<<<<<< HEAD
    ofstream out("dgemv_results.txt");
    out.precision(6);
    out << fixed;

    for (size_t size : sizes) {
        size_t m = size, n = size;
        cout << "SIZE: " << m << endl;

        vector<double> a(m * n);
        vector<double> b(n);
        vector<double> c(m);

        parallel_init(a.data(), b.data(), m, n, omp_get_max_threads());
        double serial_time = 0.0;
        for (int r = 0; r < warmup_runs + measure_runs; ++r) {
            fill(c.begin(), c.end(), 0.0);
            double t = run_serial(m, n, a, b, c);
            if (r >= warmup_runs) serial_time += t;
        }
        serial_time /= measure_runs;
        cout << "Serial avg time: " << serial_time << " sec." << endl;
        out << serial_time << " ";

        for (int j = 0; j < num_threads_counts; ++j) {
            int count_t = threads_array[j];
            if (count_t == 1) continue;
            double parallel_time = 0.0;
            for (int r = 0; r < warmup_runs + measure_runs; ++r) {
                fill(c.begin(), c.end(), 0.0);
                double t = run_parallel(m, n, count_t, a, b, c);
                if (r >= warmup_runs) parallel_time += t;
            }
            parallel_time /= measure_runs;
            cout << "Threads: " << count_t << "  avg time: " << parallel_time << " sec." << endl;
            out << parallel_time << " ";
        }
        out << endl;
        cout << "\n\n";
    }

    out.close();
    return 0;
=======
	out.close();
	return 0;
>>>>>>> refs/remotes/origin/main
}

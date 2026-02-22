#include <iostream>
#include <omp.h>
#include <vector>
#include <chrono>

using namespace std;

void matrix_vector_product(size_t m, size_t n)
{
	vector<vector<double>> a(m, vector<double>(n));
	vector<double> b(n);
	vector<double> c(m);

	for (size_t i = 0; i < m; i++)
	{
		for (size_t j = 0; j < n; j++)
			a[i][j] = i + j;
	}

	for (size_t j = 0; j < n; j++)
		b[j] = j;

	for (int i = 0; i < m; i++)
	{
		c[i] = 0.0;
		for (int j = 0; j < n; j++)
			c[i] += a[i][j] * b[j];
	}
}

void matrix_vector_product_omp(size_t m, size_t n, size_t count_t)
{
	vector<vector<double>> a(m, vector<double>(n));
	vector<double> b(n);
	vector<double> c(m);
#pragma omp parallel num_threads(count_t)
{

	int nthreads = omp_get_num_threads();
	int threadid = omp_get_thread_num();
	int items_per_thread = m / nthreads;
	int lb = threadid * items_per_thread;
	int ub = (threadid == nthreads - 1) ? (m - 1) : (lb + items_per_thread - 1);

	for (size_t i = lb; i <= ub; i++)
	{
		for (size_t j = 0; j < n; j++)
			a[i][j] = i + j;
	}

	for (size_t j = 0; j < n; j++)
		b[j] = j;

	
	for (int i = lb; i <= ub; i++)
	{
		c[i] = 0.0;
		for (int j = 0; j < n; j++)
			c[i] += a[i][j] * b[j];
	}
}
}

void run_serial(size_t n, size_t m)
{
	const auto start{std::chrono::steady_clock::now()};
	matrix_vector_product(m, n);
	const auto end{std::chrono::steady_clock::now()};
	const std::chrono::duration<double> elapsed_seconds{end - start};

	printf("Elapsed time (serial): %.6f sec.\n", elapsed_seconds.count());
}

void run_parallel(size_t n, size_t m, size_t count_t)
{

	const auto start{std::chrono::steady_clock::now()};
	matrix_vector_product_omp(m, n, count_t);
	const auto end{std::chrono::steady_clock::now()};
	const std::chrono::duration<double> elapsed_seconds{end - start};

	printf("Elapsed time (parallel): %.6f sec.\n", elapsed_seconds.count());
}

int main()
{
	int threads_array[7] = {2, 4, 7, 8, 16, 20, 40};
	int size_array[2] = {20000, 40000};


	for (int i = 0; i < 2; i++)
	{
		size_t M = size_array[i];
		size_t N = M;
		cout << "SIZE: " << M << endl;
		run_serial(M, N);
		cout << endl;
		for (int j = 0; j < 7; j++)
		{
			int count_t = threads_array[j];
			run_parallel(M, N, count_t);
		}
		cout << "\n\n\n";
	}
	return 0;
}
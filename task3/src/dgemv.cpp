#include <iostream>
#include <omp.h>
#include <vector>
#include <chrono>
#include <string.h>
#include <fstream>
#include <future>

using namespace std;

void matrix_vector_product_omp(size_t m, size_t n)
{
	vector<double> a(m*n);
	vector<double> b(n);
	vector<double> c(m);

	vector<future<void>> futures(n*m);

	for (int i = 0; i < n; i++)
	{
		futures.push_back(async(launch::async, [](vector<double> b, int x) 
					{ b[x] = x; }, b, i));
	}


	for (int i = 0; i < m; i++)
	{
		c[i] = 0.0;
		b[i] = i;		// Инициализация b-вектора
						// Работает только для m = n

		for (int j = 0; j < n; j++) {
			futures.push_back(async(launch::async, []
						(vector<double> a, vector<double> b, vector<double> c, int i, int j, int n) 
				{
					a[i*n+j] = i + j; // Инициализация a-матрица
					c[i] += a[i*n+j] * b[j];
				}, a, b, c, i, j, n));
		}
	}
}

void matrix_vector_product(size_t m, size_t n)
{
	vector<double> a(m*n);
	vector<double> b(n);
	vector<double> c(m);

	for (int i = 0; i < m; i++)
	{
		c[i] = 0.0;
		b[i] = i;		// Инициализация b-вектора
						// Работает только для m = n

		for (int j = 0; j < n; j++) {
			a[i*n+j] = i + j; // Инициализация a-матрица
			
			c[i] += a[i*n+j] * b[j];
		}
	}
}

double run_serial(size_t n, size_t m)
{
	const auto start{std::chrono::steady_clock::now()};
	matrix_vector_product(m, n);
	const auto end{std::chrono::steady_clock::now()};
	const std::chrono::duration<double> elapsed_seconds{end - start};

	printf("Elapsed time (serial): %.6f sec.\n", elapsed_seconds.count());

	return elapsed_seconds.count();
}

double run_parallel(size_t n, size_t m, size_t count_t)
{

	const auto start{std::chrono::steady_clock::now()};
	matrix_vector_product_omp(m, n);
	const auto end{std::chrono::steady_clock::now()};
	const std::chrono::duration<double> elapsed_seconds{end - start};

	printf("Elapsed time (parallel): %.6f sec.\n", elapsed_seconds.count());

	return elapsed_seconds.count();
}

int main()
{
	std::vector<int> threads_array = {2, 4, 7, 8, 16, 20, 40};
	std::vector<int> size_array = {20000, 40000};
	std::vector<int> res(16);

	for (int i = 0; i < 2; i++)
	{
		size_t M = size_array[i];
		size_t N = M;
		cout << "SIZE: " << M << endl;
		res[i*8] = run_serial(M, N);
		cout << endl;
		for (int j = 0; j < 7; j++)
		{
			int count_t = threads_array[j];
			res[i*8 + j + 1] = run_parallel(M, N, count_t);
		}
		cout << "\n\n\n";
	}

	std::ofstream out;
	out.open("dgemv_results.txt");
	if (out.is_open())
	{
		for (int i = 0; i < 16; i++)
			{
				if (i == 8)
					out << endl;
				out << res[i] << " ";
			}
	}

	out.close();
	return 0;
}

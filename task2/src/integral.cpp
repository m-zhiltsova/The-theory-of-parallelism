#include <iostream>
#include <math.h>
#include <time.h>
#include <omp.h>
#include <chrono>
#include <vector>
#include <fstream>

const double PI = 3.14159265358979323846;
const double a = -4.0;
const double b = 4.0;
const int nsteps = 40000000;

double func(double x)
{
	return exp(-x * x);
}

double integrate(double (*func)(double), double a, double b, int n)
{
	double h = (b - a) / n;
	double sum = 0.0;

	for (int i = 0; i < n; i++)
		sum += func(a + h * (i + 0.5));
	
	sum *= h;
	
	return sum;
}


double integrate_omp(double (*func)(double), double a, double b, int n, size_t count_t)
{
	double h = (b - a) / n;
	double sum = 0.0;

#pragma omp parallel num_threads(count_t)
	{
		int nthreads = omp_get_num_threads();
		int threadid = omp_get_thread_num();
		int items_per_thread = n / nthreads;

		int lb = threadid * items_per_thread;
		int ub = (threadid == nthreads - 1) ? (n - 1) : (lb + items_per_thread - 1);
		double sumloc = 0.0;

		for (int i = lb; i <= ub; i++)
			sumloc += func(a + h * (i + 0.5));

		#pragma omp atomic
		sum += sumloc;
	}
	sum *= h;

	return sum;
}

double run_serial()
{
	const auto start{std::chrono::steady_clock::now()};
	double res = integrate(func, a, b, nsteps);
	const auto end{std::chrono::steady_clock::now()};
	const std::chrono::duration<double> elapsed_seconds{end - start};
	printf("Result (serial): %.12f; error %12f\n", res, fabs(res - sqrt(PI)));

	return elapsed_seconds.count();
}

double run_parallel(size_t count_t)
{
	const auto start{std::chrono::steady_clock::now()};
	double res = integrate_omp(func, a, b, nsteps, count_t);
	const auto end{std::chrono::steady_clock::now()};
	const std::chrono::duration<double> elapsed_seconds{end - start};

	printf("Result (parallel): %.12f, error .%12f\n", res, fabs(res - sqrt(PI)));
	return elapsed_seconds.count();
}

int main(int argc, char **argv)
{
	size_t threads_array[7] = {2, 4, 7, 8, 16, 20, 40};
	double res[8];

	printf("Integration f(x) on [%.12f, %.12f], nsteps = %d\n", a, b, nsteps);

	double tserial = run_serial();
	printf("Execution time (serial): %.6f\n", tserial);

	res[0] = tserial;

	for (int i = 0; i < 7; i++) {
		double tparallel = run_parallel(threads_array[i]);

		printf("Execution time (parallel): %.6f\n", tparallel);
		printf("Speedup: %.2f\n", tserial / tparallel);

		res[i+1] = tparallel;
	}

	std::ofstream out;
	out.open("integral_results.txt");
	if (out.is_open())
	{
		for (int i = 0; i < 8; i++)
			{
				out << res[i] << " ";
			}
	}

	out.close();
	return 0;
}


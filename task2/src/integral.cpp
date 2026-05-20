#include <iostream>
#include <cmath>
#include <omp.h>
#include <chrono>
#include <fstream>
#include <vector>
#include <iomanip>

using namespace std;

const double PI = 3.14159265358979323846;
const double a = -4.0;
const double b = 4.0;
const int nsteps = 40000000;

double func(double x) {
    return std::exp(-x * x);
}

double integrate(double (*f)(double), double a, double b, int n) {
    double h = (b - a) / n;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += f(a + h * (i + 0.5));
    }
    sum *= h;
    return sum;
}

double integrate_omp(double (*f)(double), double a, double b, int n, int count_t) {
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

        for (int i = lb; i <= ub; ++i) {
            sumloc += f(a + h * (i + 0.5));
        }

        #pragma omp atomic
        sum += sumloc;
    }
    sum *= h;
    return sum;
}

<<<<<<< HEAD
int main() {
    const int warmup_runs = 1;
    const int measure_runs = 5;
    std::vector<int> threads = {1, 2, 4, 7, 8, 16, 20, 40};
=======
double run_serial()
{
	const auto start{chrono::steady_clock::now()};
	double res = integrate(func, a, b, nsteps);
	const auto end{chrono::steady_clock::now()};
	const chrono::duration<double> elapsed_seconds{end - start};
	printf("Result (serial): %.12f; error %12f\n", res, fabs(res - sqrt(PI)));
>>>>>>> refs/remotes/origin/main

    double result_serial = integrate(func, a, b, nsteps);
    printf("Integration f(x) on [%.12f, %.12f], nsteps = %d\n", a, b, nsteps);
    printf("Result (serial): %.12f, error %.12f\n\n", result_serial, std::fabs(result_serial - std::sqrt(PI)));

    std::vector<double> avg_times;

    for (int p : threads) {
        double total_time = 0.0;
        for (int run = 0; run < warmup_runs + measure_runs; ++run) {
            double res;
            auto start = std::chrono::steady_clock::now();
            if (p == 1) {
                res = integrate(func, a, b, nsteps);
            } else {
                res = integrate_omp(func, a, b, nsteps, p);
            }
            auto end = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed = end - start;
            if (run == warmup_runs + measure_runs - 1) {
                printf("[threads=%d] result=%.12f (error %.12f)\n", p, res, std::fabs(res - std::sqrt(PI)));
            }
            if (run >= warmup_runs) {
                total_time += elapsed.count();
            }
        }
        double avg_time = total_time / measure_runs;
        avg_times.push_back(avg_time);
    }

    printf("\n%10s %15s %15s\n", "Threads", "Avg time, s", "Speedup");
    double t1 = avg_times[0];
    for (size_t i = 0; i < threads.size(); ++i) {
        printf("%10d %15.6f %15.2f\n", threads[i], avg_times[i], t1 / avg_times[i]);
    }

    std::ofstream out("integral_results.txt");
    if (out.is_open()) {
        out << std::fixed << std::setprecision(6);
        for (size_t i = 0; i < avg_times.size(); ++i) {
            out << avg_times[i];
            if (i != avg_times.size() - 1) out << " ";
        }
        out << std::endl;
    }
    out.close();

    return 0;
}
<<<<<<< HEAD
=======

double run_parallel(size_t count_t)
{
	const auto start{chrono::steady_clock::now()};
	double res = integrate_omp(func, a, b, nsteps, count_t);
	const auto end{chrono::steady_clock::now()};
	const chrono::duration<double> elapsed_seconds{end - start};

	printf("Result (parallel): %.12f, error .%12f\n", res, fabs(res - sqrt(PI)));
	return elapsed_seconds.count();
}

int main(int argc, char **argv)
{
	vector<size_t> threads_array = {2, 4, 7, 8, 16, 20, 40};
	vector<double> res(8);

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

	ofstream out;
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

>>>>>>> refs/remotes/origin/main

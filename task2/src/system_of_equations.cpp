#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <omp.h>
using namespace std;

void variant_1(size_t n, size_t count_t) {
	vector<vector<double>> a(n, vector<double>(n));
	vector<double> b(n);
	vector<double> x(n);
	vector<double> xo(n);
	double tmp;
	size_t i, j;
	double eps = 1;
	double tau = 1.0 / (n + 1);

#pragma omp parallel num_threads(count_t)
{

	int nthreads = omp_get_num_threads();
	int threadid = omp_get_thread_num();
	int items_per_thread = n / nthreads;
	int lb = threadid * items_per_thread;
	int ub = (threadid == nthreads - 1) ? (n - 1) : (lb + items_per_thread - 1);


	for (j = lb; j < ub; j++)
		b[j] = n + 1;

	for (i = lb; i < ub; i++) {
		for (size_t j = 0; j < n; j++) {
			if (i == j)
				a[i][j] = 2;
			else
				a[i][j] = 1;
		}
	}


    for(i = lb; i < ub; i++) {
        x[i] = 0;
        xo[i] = 0;
    }
}

	double mul_ax = 0;
	while (eps > 0.00001) {
		double u = 0;
		double b_2 = 0;
#pragma omp parallel num_threads(count_t) shared(a, b, xo, x, n, tau) private(i, j, mul_ax)
{
#pragma omp for reduction(+:u, b_2) nowait
	    for(i = 0; i < n; i++) {
	        mul_ax = 0;
	        for(j = 0; j < n; j++)
	            mul_ax += a[i][j] * xo[j];
	        x[i] = xo[i] - tau * (mul_ax - b[i]);
			u += pow(mul_ax - b[i], 2);
			b_2 += pow(b[i], 2);
	    }
#pragma omp for
	    for(i = 0; i < n; i++) {
	        xo[i] = x[i];
	    }
		u = sqrt(u);
		b_2 = sqrt(b_2);

	    eps = u / b_2;
	}
}
    // for(i = 0; i < n; i++)
    // 	cout << "x[" << i + 1 << "]=" << x[i] << endl;
}


void variant_2(size_t n, size_t count_t) {
	vector<vector<double>> a(n, vector<double>(n));
	vector<double> b(n);
	vector<double> x(n);
	vector<double> xo(n);
	double tmp;
	size_t i, j;
	double eps = 1;
	double tau = 1.0 / (n + 1);


	double mul_ax = 0;

	for (j = 0; j < n; j++)
		b[j] = n + 1;

	for (i = 0; i < n; i++) {
		for (size_t j = 0; j < n; j++) {
			if (i == j)
				a[i][j] = 2;
			else
				a[i][j] = 1;
		}
	}


    for(i = 0; i < n; i++) {
        x[i] = 0;
        xo[i] = 0;
    }

	while (eps > 0.00001) {
		cout << (eps);
		cout << (endl);
		double u = 0;
		double b_2 = 0;
	    for(i = 0; i < n; i++) {
	        mul_ax = 0;
	        for(j = 0; j < n; j++)
	            mul_ax += a[i][j] * xo[j];
	        x[i] = xo[i] - tau * (mul_ax - b[i]);
			u += pow(mul_ax - b[i], 2);
			b_2 += pow(b[i], 2);
	    }
		u = sqrt(u);
		b_2 = sqrt(b_2);

	    eps = u / b_2;
	    for(i = 0; i < n; i++) {
	        xo[i] = x[i];
	    }
	}

    // for(i = 0; i < n; i++)
    // 	cout << "x[" << i + 1 << "]=" << x[i] << endl;
}
	

double run_variant_1(size_t n, size_t count_t)
{

	const auto start{chrono::steady_clock::now()};
	variant_1(n, count_t);
	const auto end{chrono::steady_clock::now()};
	const chrono::duration<double> elapsed_seconds{end - start};

	printf("Elapsed time variant 1: %.6f sec.\n", elapsed_seconds.count());

	return elapsed_seconds.count();
}

double run_variant_2(size_t n, size_t count_t) {

	const auto start{chrono::steady_clock::now()};
	variant_2(n, count_t);
	const auto end{chrono::steady_clock::now()};
	const chrono::duration<double> elapsed_seconds{end - start};

	printf("Elapsed time variant 1: %.6f sec.\n", elapsed_seconds.count());

	return elapsed_seconds.count();
}


int main()
{
	run_variant_1(50, 5);
	return 0;
}

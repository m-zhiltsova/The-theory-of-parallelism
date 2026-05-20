#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <omp.h>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <string>
#include <utility>

using namespace std;

void init_system(vector<double>& a, vector<double>& b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        b[i] = n + 1.0;
        for (size_t j = 0; j < n; ++j) {
            a[i * n + j] = (i == j) ? 2.0 : 1.0;
        }
    }
}

void variant_1(const vector<double>& a, const vector<double>& b,
               vector<double>& x, vector<double>& xo,
               size_t n, int count_t, double tau, double eps_target,
               const string& schedule_type = "static", int chunk_size = 0) {
    double eps = 1.0;
    if (schedule_type == "static")
        omp_set_schedule(omp_sched_static, chunk_size);
    else if (schedule_type == "dynamic")
        omp_set_schedule(omp_sched_dynamic, chunk_size);
    else if (schedule_type == "guided")
        omp_set_schedule(omp_sched_guided, chunk_size);
    else
        omp_set_schedule(omp_sched_auto, 0);

    while (eps > eps_target) {
        double u = 0.0, b_2 = 0.0;
        #pragma omp parallel for num_threads(count_t) reduction(+:u, b_2) schedule(runtime)
        for (size_t i = 0; i < n; ++i) {
            double mul_ax = 0.0;
            for (size_t j = 0; j < n; ++j)
                mul_ax += a[i * n + j] * xo[j];
            x[i] = xo[i] - tau * (mul_ax - b[i]);
            u += (mul_ax - b[i]) * (mul_ax - b[i]);
            b_2 += b[i] * b[i];
        }

        #pragma omp parallel for num_threads(count_t) schedule(runtime)
        for (size_t i = 0; i < n; ++i)
            xo[i] = x[i];

        u = sqrt(u);
        b_2 = sqrt(b_2);
        eps = u / b_2;
    }
}

void variant_2(const vector<double>& a, const vector<double>& b,
               vector<double>& x, vector<double>& xo,
               size_t n, int count_t, double tau, double eps_target,
               const string& schedule_type = "static", int chunk_size = 0) {
    double eps = 1.0;
    if (schedule_type == "static")
        omp_set_schedule(omp_sched_static, chunk_size);
    else if (schedule_type == "dynamic")
        omp_set_schedule(omp_sched_dynamic, chunk_size);
    else if (schedule_type == "guided")
        omp_set_schedule(omp_sched_guided, chunk_size);
    else
        omp_set_schedule(omp_sched_auto, 0);

    #pragma omp parallel num_threads(count_t) shared(a, b, x, xo, n, tau, eps, eps_target)
    {
        while (eps > eps_target) {
            double u_local = 0.0, b_2_local = 0.0;
            #pragma omp for schedule(runtime) reduction(+:u_local, b_2_local) nowait
            for (size_t i = 0; i < n; ++i) {
                double mul_ax = 0.0;
                for (size_t j = 0; j < n; ++j)
                    mul_ax += a[i * n + j] * xo[j];
                x[i] = xo[i] - tau * (mul_ax - b[i]);
                u_local += (mul_ax - b[i]) * (mul_ax - b[i]);
                b_2_local += b[i] * b[i];
            }
            #pragma omp barrier

            #pragma omp single
            {
                double u = sqrt(u_local);
                double b_2 = sqrt(b_2_local);
                eps = u / b_2;
            }
            #pragma omp for schedule(runtime) nowait
            for (size_t i = 0; i < n; ++i)
                xo[i] = x[i];
            #pragma omp barrier
        }
    }
}

int main() {
    const double eps_target = 1e-5;
    const int warmup_runs = 1;
    const int measure_runs = 5;

    size_t N = 10000;
    double serial_time_v1 = 0.0;
    vector<double> a, b;
    double tau = 1.0 / (N + 1);
    a.assign(N * N, 0.0);
    b.resize(N);
    init_system(a, b, N);

    vector<int> threads = {1, 2, 4, 7, 8, 16, 20, 40};
    ofstream out("system_results.txt");
    out << fixed << setprecision(6);
    out << "N=" << N << "\nThreads Variant1_time Variant2_time\n";

    cout << "Comparing variant 1 and variant 2:" << endl;
    for (int p : threads) {
        double t1, t2;

        if (p == 1) {
            t1 = serial_time_v1;
        } else {
            vector<double> x(N), xo(N);
            for (int i = 0; i < warmup_runs; ++i) {
                fill(x.begin(), x.end(), 0.0);
                fill(xo.begin(), xo.end(), 0.0);
                variant_1(a, b, x, xo, N, p, tau, eps_target);
            }
            double total = 0.0;
            for (int i = 0; i < measure_runs; ++i) {
                fill(x.begin(), x.end(), 0.0);
                fill(xo.begin(), xo.end(), 0.0);
                auto start = chrono::steady_clock::now();
                variant_1(a, b, x, xo, N, p, tau, eps_target);
                auto end = chrono::steady_clock::now();
                total += chrono::duration<double>(end - start).count();
            }
            t1 = total / measure_runs;
        }

        {
            vector<double> x(N), xo(N);
            for (int i = 0; i < warmup_runs; ++i) {
                fill(x.begin(), x.end(), 0.0);
                fill(xo.begin(), xo.end(), 0.0);
                variant_2(a, b, x, xo, N, p, tau, eps_target);
            }
            double total = 0.0;
            for (int i = 0; i < measure_runs; ++i) {
                fill(x.begin(), x.end(), 0.0);
                fill(xo.begin(), xo.end(), 0.0);
                auto start = chrono::steady_clock::now();
                variant_2(a, b, x, xo, N, p, tau, eps_target);
                auto end = chrono::steady_clock::now();
                total += chrono::duration<double>(end - start).count();
            }
            t2 = total / measure_runs;
        }

        out << p << " " << t1 << " " << t2 << "\n";
        cout << "Threads: " << p << "  V1: " << t1 << " s  V2: " << t2 << " s" << endl;
    }

    cout << "\nSchedule study (variant_1, N=" << N << ", threads=8):" << endl;
    out << "\nSchedule_study for 8 threads\n";
    vector<pair<string, int>> schedules = {
        {"static", 0}, {"static", 1}, {"static", 16}, {"static", 64},
        {"dynamic", 1}, {"dynamic", 16}, {"dynamic", 64},
        {"guided", 1}, {"guided", 16}, {"guided", 64},
        {"auto", 0}
    };
    for (auto& sched : schedules) {
        vector<double> x(N), xo(N);
        string sched_name = sched.first;
        int chunk = sched.second;
        for (int i = 0; i < warmup_runs; ++i) {
            fill(x.begin(), x.end(), 0.0);
            fill(xo.begin(), xo.end(), 0.0);
            variant_1(a, b, x, xo, N, 8, tau, eps_target, sched_name, chunk);
        }
        double total = 0.0;
        for (int i = 0; i < measure_runs; ++i) {
            fill(x.begin(), x.end(), 0.0);
            fill(xo.begin(), xo.end(), 0.0);
            auto start = chrono::steady_clock::now();
            variant_1(a, b, x, xo, N, 8, tau, eps_target, sched_name, chunk);
            auto end = chrono::steady_clock::now();
            total += chrono::duration<double>(end - start).count();
        }
        double avg_time = total / measure_runs;
        cout << sched_name << ", chunk=" << chunk << " -> " << avg_time << " s" << endl;
        out << sched_name << "," << chunk << ": " << avg_time << "\n";
    }

    out.close();
    cout << "\nResults saved to system_results.txt" << endl;
    return 0;
}

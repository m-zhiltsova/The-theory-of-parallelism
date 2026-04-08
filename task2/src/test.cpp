#include <iostream>
#include <vector>
#include <cmath>
using namespace std;

void variant_1(size_t n) {
    // Инициализация матрицы A и вектора b
    vector<vector<double>> A(n, vector<double>(n, 1.0));
    vector<double> b(n, n + 1);
    vector<double> x(n, 0.0);      // текущее приближение
    vector<double> x_prev(n, 0.0); // предыдущее приближение

    // Заполнение диагонали
    for (size_t i = 0; i < n; ++i) {
        A[i][i] = 2.0;
    }

    // Параметр tau (можно выбрать, например, 1/(n+1))
    double tau = 1.0 / (n + 1);

    double eps = 1.0;
    const double tol = 1e-5;

    while (eps > tol) {
        // Вычисляем новое приближение
        for (size_t i = 0; i < n; ++i) {
            double Ax = 0.0;
            for (size_t j = 0; j < n; ++j) {
                Ax += A[i][j] * x_prev[j];
            }
            x[i] = x_prev[i] - tau * (Ax - b[i]);
        }

        // Вычисляем относительную норму невязки для критерия остановки
        double norm_res = 0.0;
        double norm_b = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double res = 0.0;
            for (size_t j = 0; j < n; ++j) {
                res += A[i][j] * x[j];
            }
            res -= b[i];
            norm_res += res * res;
            norm_b += b[i] * b[i];
        }
        eps = sqrt(norm_res) / sqrt(norm_b);

        // Обновляем предыдущее приближение
        x_prev = x;
    }

    // Вывод решения
    cout << "Решение:" << endl;
    for (size_t i = 0; i < n; ++i) {
        cout << "x[" << i + 1 << "] = " << x[i] << endl;
    }
}


int main() {
	variant_1(5);
}

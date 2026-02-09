#include <iostream>
#include <cmath>

using namespace std;

#define size 10000000

template <typename T>
T calculate_sum_of_sin() {
	T* array = new T[size];
	T sum = 0;

	for (int i = 0; i < size; i += 1) {
		array[i] = sin(2 * M_PI * i / size);
		sum += array[i];
	}

	delete[] array;
	return sum;
}


int main() {

#ifdef USE_DOUBLE
	double sum = calculate_sum_of_sin<double>();
	cout << "Double sum: " << sum << endl;
#elif defined(USE_FLOAT)
	float sum = calculate_sum_of_sin<float>();
	cout << "Float sum: " << sum << endl;
#endif

	return 0;
}

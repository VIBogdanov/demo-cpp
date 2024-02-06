#include <iostream>
#include <locale.h>

#include "src/sundry.h"
#include "src/assistools.h"

using namespace std;

int main()
{
	setlocale(LC_CTYPE, "");

	// Функция преобразования целого числа в набор цифр.
	cout << "assistools::inumber_to_digits(1234) -> ";
	for (int elem : assistools::inumber_to_digits(1234))
		cout << elem << " ";
	cout << endl;

	// Функция нахождения наибольшего общего делителя двух целых чисел без перебора методом Евклида.
	cout << "ndry::get_common_divisor(20, 12) -> ";
	cout << sundry::get_common_divisor(20, 12) << endl;

	return 0;
}
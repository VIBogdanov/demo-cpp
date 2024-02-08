#include <iostream>
#include <locale.h>

#include "src/assistools.h"
#include "src/sundry.h"

using namespace std;

int main()
{
	setlocale(LC_CTYPE, "");

	cout << endl << " - Функция преобразования целого числа в набор цифр." << endl;
	cout << " inumber_to_digits(1234) -> ";
	for (int elem : assistools::inumber_to_digits(1234))
		cout << elem << " ";
	cout << endl;

	cout << endl << " - Функция нахождения наибольшего общего делителя двух целых чисел без перебора методом Евклида." << endl;
	cout << " get_common_divisor(20, 12) -> ";
	cout << sundry::get_common_divisor(20, 12) << endl;

	cout << endl << " - Поиск элемента в массиве данных при помощи бинарного алгоритма." << endl;
	cout << " find_item_by_binary({ 20, 30, 40, 50 }, 30) -> ";
	cout << sundry::find_item_by_binary({ 20, 30, 40, 50 }, 30) << endl;

	return 0;
}
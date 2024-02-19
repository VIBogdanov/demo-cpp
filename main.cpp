#include <iostream>
#include <locale.h>

#include "src/assistools.h"
#include "src/sundry.h"
#include <vector>

using namespace std;

int main()
{
	setlocale(LC_CTYPE, "");

	cout << endl << " - Функция преобразования целого числа в набор цифр." << endl;
	cout << " inumber_to_digits(12340) -> ";
	for (int elem : assistools::inumber_to_digits(12340))
		cout << elem << " ";
	cout << endl;

	cout << endl << " - Функция нахождения наибольшего общего делителя двух целых чисел без перебора методом Евклида." << endl;
	cout << " get_common_divisor(20, 12) -> ";
	cout << sundry::get_common_divisor(20, 12) << endl;

	cout << endl << " - Поиск элемента в массиве данных при помощи бинарного алгоритма." << endl;
	cout << " find_item_by_binary({ -20, 30, 40, 50 }, 30) -> ";
	cout << sundry::find_item_by_binary({ -20, 30, 40, 50 }, 30) << endl;

	cout << endl << " - Поиск в списке из чисел последовательного непрерывного интервала(-ов) чисел, сумма которых равна искомому значению." << endl;
	cout << " find_intervals(vector<int>{ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0) -> ";
	auto result = sundry::find_intervals(vector<int>{ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0);
	for (const auto& p : result)
		std::cout << "(" << p.first << ", " << p.second << ") ";
	std::cout << std::endl;

	return 0;
}
#include <algorithm>
#include <clocale>
#include <iostream>
#include <vector>

import Demo;

using namespace std;

int main()
{
	setlocale(LC_CTYPE, "");

	{
		cout << endl << " - Функция преобразования целого числа в набор цифр." << endl;
		cout << " inumber_to_digits(12340) -> ";
		auto res = assistools::inumber_to_digits(12340);
		std::for_each(res.begin(), res.end(), [](const int n) { std::cout << n << ' '; });
		cout << endl;
	}

	{
		std::cout << std::endl << " - Формирует список индексов диапазонов, на которые можно разбить список заданной длины." << endl;
		std::cout << " get_ranges_index(50, 10) -> ";
		auto res = assistools::get_ranges_index(50, 10);
		for (const auto& [_first, _last] : res)
			std::cout << "[" << _first << ", " << _last << ") ";
		std::cout << std::endl;
	}
	
	cout << endl << " - Функция нахождения наибольшего общего делителя двух целых чисел без перебора методом Евклида." << endl;
	cout << " get_common_divisor(20, 12) -> ";
	cout << sundry::get_common_divisor(20, 12) << endl;

	cout << endl << " - Поиск элемента в массиве данных при помощи бинарного алгоритма." << endl;
	cout << " find_item_by_binary({ -20, 30, 40, 50 }, 30) -> ";
	cout << sundry::find_item_by_binary(std::vector<int>{ -20, 30, 40, 50 }, 30) << endl;

	cout << endl << " - Поиск элемента в массиве данных при помощи алгоритма интерполяции." << endl;
	cout << " find_item_by_interpolation({ -1, -2, 3, 4, 5 }, 4) -> ";
	cout << sundry::find_item_by_interpolation(std::vector<int>{ -1, -2, 3, 4, 5 }, 4) << endl;

	{
		cout << endl << " - Поиск в списке из чисел последовательного непрерывного интервала(-ов) чисел, сумма которых равна искомому значению." << endl;
		cout << " find_intervals(vector<int>{ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0) -> ";
		auto result = sundry::find_intervals(vector<int>{ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0);
		for (const auto& p : result)
			std::cout << "(" << p.first << ", " << p.second << ") ";
		std::cout << std::endl;
	}
	

	cout << endl << " - Поиск ближайшего целого числа, которое меньше или больше заданного и состоит из тех же цифр." << endl;
	cout << " find_nearest_number(273145) -> ";
	cout << sundry::find_nearest_number(273145) << endl;

	return 0;
}
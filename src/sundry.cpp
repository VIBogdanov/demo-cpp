/*
SUNDRY.CPP
Набор различных функций реализующие алгоритмы сортировки, поиска, фильтрации и т.п.
Каждая функция имеет подробное описание и готова к применению.
*/
#include "sundry.h"

using namespace std;
using namespace sundry;


int sundry::get_common_divisor(const int& number_a, const int& number_b)
{
	auto divisible = 0, divisor = 0;

	// Определяем делимое и делитель. Делимое - большее число. Делитель - меньшее.
	pair<int, int> minmax_values = minmax(abs(number_a), abs(number_b));

	if (minmax_values.first) // Вычисляем только если делитель не равен нулю.
	{
		divisible = minmax_values.second;
		divisor = minmax_values.first;

		while (auto _div{ divisible % divisor }) {
			divisible = divisor;
			divisor = _div;
		}
	}
	else
		divisor = minmax_values.second;

	return divisor;
}

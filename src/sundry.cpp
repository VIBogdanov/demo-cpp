/*
SUNDRY.CPP
Набор различных функций реализующие алгоритмы сортировки, поиска, фильтрации и т.п.
*/
#include <algorithm>
#include <cmath>
#include <utility>

#include <type_traits>

#include "sundry.h"


namespace sundry
{
	int get_common_divisor(const int& number_a, const int& number_b)
	{
		using TNum = std::remove_cvref_t<decltype(number_a)>;
		using TPair = std::pair<TNum, TNum>;

		// Определяем делимое и делитель. Делимое - большее число. Делитель - меньшее.
		auto [divisor, divisible] { static_cast<TPair>(std::minmax(std::abs(number_a), std::abs(number_b))) };
		
		if (divisor) // Вычисляем только если делитель не равен нулю.
			while (auto _div{ divisible % divisor }) {
				divisible = divisor;
				divisor = _div;
			}
		else
			divisor = divisible;

		return divisor;
	}
}

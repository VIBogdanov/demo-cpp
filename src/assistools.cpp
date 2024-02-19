/*
ASSISTOOLS.CPP
Набор вспомогательных функций.
*/
#include <vector>
#include <cmath>

#include "assistools.h"


namespace assistools
{
	std::vector<int> inumber_to_digits(const int& num)
	{
		std::vector<int> result;
		// Знак числа отбрасываем
		int _num = abs(num);

		if (!_num)
			result.push_back(0);
		else
		{
			// Разбиваем целое положительное число на отдельные цифры
			while (_num > 0)
			{
				result.push_back(_num % 10);
				_num /= 10;
			}
		}

		// В полученном списке цифры расположены в обратном порядке. Переворачиваем список.
		if (result.size() > 1)
		{
			auto it_left = result.begin(), it_right = result.end();
			while (it_left < it_right)
			{
				auto _tmp = *it_left;
				*it_left++ = *--it_right;
				*it_right = _tmp;
			}
		}

		return result;
	}
}

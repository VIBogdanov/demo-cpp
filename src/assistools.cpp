/*
ASSISTOOLS.CPP
Набор вспомогательных функций.
*/
#include <vector>
#include <math.h>

#include "assistools.h"


std::vector<int> assistools::inumber_to_digits(const int& num)
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
			int _modulo = _num % 10;
			result.push_back(_modulo);
			_num = _num / 10;
		}
	}
	
	// В полученном списке цифры расположены в обратном порядке. Переворачиваем список.
	if (result.size() > 1)
	{
		std::vector<int>::iterator it_left = result.begin(), it_right = --result.end();
		while (it_left < it_right)
		{
			auto _tmp = *it_left;
			*it_left = *it_right;
			*it_right = _tmp;
			++it_left;
			--it_right;
		}
	}

	return result;
}

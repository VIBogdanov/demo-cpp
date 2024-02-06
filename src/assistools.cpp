/*
ASSISTOOLS.CPP
Набор вспомогательных функций.
*/
#include <vector>
#include <math.h>

#include "assistools.h"


std::vector<int> assistools::inumber_to_digits(const int& num)
{
	std::vector<int> _vec;
	// Знак числа отбрасываем
	int _num = abs(num);

	if (!_num)
		_vec.push_back(0);
	else
	{
		// Разбиваем целое положительное число на отдельные цифры
		while (_num > 0)
		{
			int modulo = _num % 10;
			_vec.push_back(modulo);
			_num = _num / 10;
		}
	}
	
	// В полученном списке цифры расположены в обратном порядке. Переворачиваем список.
	if (_vec.size() > 1)
	{
		std::vector<int>::iterator it_left = _vec.begin(), it_right = --_vec.end();
		while (it_left < it_right)
		{
			int _tmp = *it_left;
			*it_left = *it_right;
			*it_right = _tmp;
			++it_left;
			--it_right;
		}
	}

	return _vec;
}

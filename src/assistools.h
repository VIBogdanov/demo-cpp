#pragma once

#include <vector>

namespace assistools
{
	/**
	@brief Функция преобразования целого числа в набор цифр.

	@example inumber_to_digits(124) -> vector<int>(1, 2, 4)

	@param (int&) Заданное целое число. По-умолчанию 0.

	@return (vector<int>) Массив цифр.
	*/
	std::vector<int> inumber_to_digits(const int& = 0);
}

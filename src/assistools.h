#pragma once

#include <memory>
#include <vector>

namespace assistools
{
	using TVectorInt = typename std::vector<int, std::allocator<int>>;

	/**
	@brief Функция преобразования целого числа в набор цифр.

	@example inumber_to_digits(124) -> vector<int>(1, 2, 4)

	@param (const int&) Заданное целое число. По умолчанию 0.

	@return (vector<int>) Массив цифр.
	*/
	TVectorInt inumber_to_digits(const int& = 0);
}

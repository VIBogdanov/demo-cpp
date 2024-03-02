/*
ASSISTOOLS.CPP
Набор вспомогательных функций.
*/
#include <vector>
#include <type_traits>

#include "assistools.h"



namespace assistools
{
	TVectorInt inumber_to_digits(const int& num)
	{
		using TNum = std::remove_cvref_t<decltype(num)>;
		TNum _num = (num >= 0) ? num : -num;  // Знак числа отбрасываем

		// Разбиваем целое положительное число на отдельные цифры
		TVectorInt result;
		do
			result.emplace(result.cbegin(), _num % 10);
		while ((_num /= 10) != 0);

		return result;
	}
}

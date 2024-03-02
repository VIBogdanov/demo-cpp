module;
#include <type_traits>
#include <utility>
#include <vector>
export module Demo:Assistools;

export namespace assistools
{
	/**
	@brief Функция преобразования целого числа в набор цифр.

	@example inumber_to_digits(124) -> vector<int>(1, 2, 4)

	@param number - Заданное целое число. По умолчанию 0.

	@return (vector<int>) Массив цифр.
	*/
	template <typename TNumber = int>
		requires requires {
		std::is_integral_v<TNumber>;
		std::is_arithmetic_v<TNumber>;
	}
	auto inumber_to_digits(const TNumber& number = 0)
	{
		//using TNumber = std::remove_cvref_t<decltype(number)>;
		TNumber _num = static_cast<TNumber>((number < 0) ? -number : number);  // Знак числа отбрасываем

		// Разбиваем целое положительное число на отдельные цифры
		std::vector<TNumber> result;
		do
			result.emplace(result.cbegin(), _num % 10);
		while ((_num /= 10) != 0);

		return result;
	};


	/**
	 """
	@brief Функция, формирующая список индексов диапазонов заданной длины,
	на которые можно разбить исходный список. Индексирование начинается с нуля.
	Последний индекс диапазона не включается. Действует правило [ ).

	@details Возможно задавать отрицательные значения. Если range_size < 0,
	то список индексов выводится в обратном порядке. Внимание!!! Данная функция
	рассчитана на приятую индексацию массивов и контейнеров от 0 до size().

	@example get_ranges_index(50, 10) -> [0, 10) [10, 20) [20, 30) [30, 40) [40, 50)
			 get_ranges_index(-50, 10) -> [0, -10) [-10, -20) [-20, -30) [-30, -40) [-40, -50)
			 get_ranges_index(50, -10) -> [49, 39) [39, 29) [29, 19) [19, 9) [9, -1)
			 get_ranges_index(-50, -10) -> [-49, -39) [-39, -29) [-29, -19) [-19, -9) [-9, 1)

	@param data_size (int): Размер исходного списка.

	@param range_size (int): Размер диапазона.

	@return vector<std::pair<int, int>>: Список пар с начальным и конечным индексами диапазона.
	"""
	*/
	auto get_ranges_index(const int& data_size, const int& range_size = 0)
		-> std::vector<std::pair<int, int>>
	{
		auto _abs = [](const auto& x) { return (x < 0) ? -x : x;  };

		std::vector<std::pair<int, int>> result;
		// Сохраняем знак
		int sign = (data_size < 0) ? -1 : 1;
		// Далее работаем без знака
		auto _data_size = _abs(data_size);
		auto _range_size = (range_size == 0) ? _data_size : _abs(range_size);
		int idx = 0;
		bool revers = false;
		// Если размер диапазона отрицательный, переворачиваем список диапазонов
		if (range_size < 0)
		{
			idx = _data_size - 1;
			_data_size = -1;
			_range_size = -_range_size;
			revers = true;
		}
		// Сравнение зависит от прямого или реверсного списка диапазонов.
		auto _compare = [&revers](const auto& a, const auto& b) -> bool { return (revers) ? (a > b) : (a < b); };

		do
		{
			if (_compare((idx + _range_size), _data_size))
				result.emplace_back(idx * sign, (idx + _range_size) * sign);
			else
				result.emplace_back(idx * sign, _data_size * sign);
			idx += _range_size;
		} while (_compare(idx, _data_size));

		return result;
	};
}
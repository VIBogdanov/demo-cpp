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
	template <typename TNumber = int>
		requires requires {
		std::is_integral_v<TNumber>;
		std::is_arithmetic_v<TNumber>;
	}
	auto get_ranges_index(const TNumber& data_size, const TNumber& range_size = 0)
		-> std::vector<std::pair<TNumber, TNumber>>
	{
		auto _abs = [](const auto& x) { return (x < 0) ? -x : x;  };

		std::vector<std::pair<TNumber, TNumber>> result;
		// Сохраняем знак
		TNumber sign = (data_size < 0) ? -1 : 1;
		// Далее работаем без знака
		TNumber _data_size = _abs(data_size);
		TNumber _range_size = (range_size == 0) ? _data_size : _abs(range_size);
		TNumber idx = 0;

		// Если размер диапазона отрицательный, переворачиваем список диапазонов
		if (range_size < 0)
		{
			idx = std::move(_data_size) - 1;
			_data_size = -1;
			_range_size = -_range_size;
		}
		// Сравнение зависит от прямого или реверсного списка диапазонов.
		auto _compare = [&range_size, &_data_size](const auto& x) -> bool
			{ return (range_size < 0) ? (x > _data_size) : (x < _data_size); };

		do
		{	// При формировании списка диапазонов восстанавливаем знак
			if (_compare(idx + _range_size))
				result.emplace_back(idx * sign, (idx + _range_size) * sign);
			else
				result.emplace_back(idx * sign, _data_size * sign);

			idx += _range_size;
		}
		while (_compare(idx));

		return result;
	};
}
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
	auto inumber_to_digits(const TNumber& number = 0 /*Integer number*/) noexcept -> std::vector<int>
	{
		TNumber _num{ (number < 0) ? -number : number }; // Знак числа отбрасываем
		std::vector<int> result;

		do
			result.emplace(result.cbegin(), static_cast<int>(_num % 10));
		while (_num /= 10);

		result.shrink_to_fit();
		return result;
	};

	/**
	 """
	@brief Функция, формирующая список индексов диапазонов заданной длины,
	на которые можно разбить исходный список. Индексирование начинается с нуля.
	Последний индекс диапазона не включается. Действует правило [ ).

	@details Возможно задавать отрицательные значения. Если range_size < 0,
	то список индексов выводится в обратном порядке. Внимание!!! Данная функция
	рассчитана на принятую индексацию массивов и контейнеров от 0 до size().

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
	auto get_ranges_index(const TNumber& data_size, const TNumber& range_size = 0) noexcept
		-> std::vector<std::pair<TNumber, TNumber>>
	{
		auto _abs = [](const auto& x) { return (x < 0) ? -x : x;  };

		std::vector<std::pair<TNumber, TNumber>> result;
		// Сохраняем знак
		TNumber sign{ (data_size < 0) ? -1 : 1 };
		// Далее работаем без знака
		TNumber _data_size{ _abs(data_size) };
		TNumber _range_size{ (range_size == 0) ? _data_size : _abs(range_size) };
		TNumber idx{ 0 };

		// Если размер диапазона отрицательный, переворачиваем список диапазонов
		if (range_size < 0)
		{
			idx = std::move(_data_size) - 1;
			_data_size = -1;
			_range_size = -_range_size;
		}
		// Сравнение зависит от прямого или реверсного списка диапазонов.
		auto _compare = [&range_size, &_data_size](const auto& x) -> bool
			{ return (range_size < 0) ? (_data_size < x) : (x < _data_size); };

		do
		{	
			auto rdx{ idx + _range_size };
			// При формировании списка диапазонов восстанавливаем знак
			(_compare(rdx))
				? result.emplace_back(idx * sign, rdx * sign)
				: result.emplace_back(idx * sign, _data_size * sign);

			idx = std::move(rdx);
		}
		while (_compare(idx));

		result.shrink_to_fit();
		return result;
	};


	/**
	@brief Функция возведения в степень целого числа.

	@details В дополнении к стандартной библиотеке, которая не имеет перегруженной версии
	функции std::pow(), возвращающей целочисленное значение для возводимых в степень целых чисел.
	Используется алгоритм быстрого возведения в степень по схеме "справа на лево".

	@param base: Возводимое в степень целое число.
	@param exp: Степень возведения.

	@return Целочисленный результат возведения целого числа в заданную степень.
	*/
	template<typename TInt>
		requires requires {
		std::is_integral_v<TInt>;
		std::is_arithmetic_v<TInt>;
	}
	TInt ipow(const TInt& base, const TInt& exp)
	{
		TInt _base{ base }, _exp{ exp };
		// Обрабатываем пороговые значения
		if (_base == 1 || _exp == 0) return 1;
		// Случаи с отрицательной степенью
		if (std::numeric_limits<TInt>::is_signed && _exp < 0)
			return ((_base == 0) ? std::numeric_limits<TInt>::min()
				: (_base != -1) ? 0 : (_exp & 1) ? -1 : 1);

		TInt res{ 1 };
		// Побитово считываем степень, начиная с младших разрядов (справа на лево)
		// и выполняем вычисления в зависимости от значения полученного бита.
		while (_exp) {
			if (_exp & 1) res *= _base;
			_base *= _base;
			_exp >>= 1;
		}

		return res;
	};
}
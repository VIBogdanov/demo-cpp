module;
#include <algorithm>
#include <chrono>
#include <mutex>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
export module Demo:Assistools;

export namespace assistools
{
	/**
	@brief Функция преобразования целого числа в набор цифр.

	@example inumber_to_digits(124) -> vector<int>(1, 2, 4)

	@param number - Заданное целое число. По умолчанию 0.

	@return Массив цифр.
	*/
	template <typename TNumber>
		requires std::is_integral_v<TNumber>&&
				std::is_arithmetic_v<TNumber>
	constexpr auto inumber_to_digits(TNumber number = TNumber()) noexcept
		-> std::vector<TNumber>
	{
		if (number < 0) number = -number; // Знак числа отбрасываем
		std::vector<TNumber> result;

		do
			result.emplace_back(number % 10);
		while (number /= 10);

		result.shrink_to_fit();
		// Можно было отказаться от reverse и в цикле использовать вставку в начало вектора,
		// но это каждый раз приводит к сдвигу всех элементов вектора, что медленно
		std::ranges::reverse(result);
		return result;
	};

	/**
	@brief Функция получения целого числа из набора цифр или чисел.

	@example inumber_from_digits({1,2,34}) -> 1234

	@param digits - Список цифр/чисел или начальный и конечный итераторы списка.

	@return Целое число.
	*/
	template <typename TResult = int,
		std::input_iterator TIterator,
		std::sentinel_for<TIterator> TSIterator>
	requires std::is_integral_v<std::iter_value_t<TIterator>>&&
			std::is_arithmetic_v<std::iter_value_t<TIterator>>&&
			std::convertible_to<std::iter_value_t<TIterator>, TResult>
	constexpr auto inumber_from_digits(const TIterator first, const TSIterator last) noexcept
		-> TResult
	{
		// TResult используется для указания типа возвращаемого значения отличного от int
		auto get_number = [](TResult num, const auto& dig)->TResult
			{
				if (auto _dig{ (dig < 0) ? -dig : dig }; _dig < 10)
					num = num * 10 + static_cast<TResult>(_dig);
				else
					std::ranges::for_each(assistools::inumber_to_digits(_dig),
						[&num](const auto& __dig) { num = num * 10 + static_cast<TResult>(__dig); });
				return num;
			};
		return std::accumulate(first, last, TResult(0), get_number);
	};

	template <typename TResult = int, typename TContainer = std::vector<int>>
	requires std::is_integral_v<typename std::remove_cvref_t<TContainer>::value_type>&&
			std::is_arithmetic_v<typename std::remove_cvref_t<TContainer>::value_type>&&
			std::convertible_to<typename std::remove_cvref_t<TContainer>::value_type, TResult>
	constexpr auto inumber_from_digits(TContainer&& digits) noexcept
		-> TResult
	{
 		return inumber_from_digits<TResult>(std::ranges::begin(digits), std::ranges::end(digits));
	};

	/**
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
	*/
	template <typename TNumber>
	requires std::is_integral_v<TNumber> && std::is_arithmetic_v<TNumber>
	auto get_ranges_index(const TNumber& data_size, const TNumber& range_size = TNumber()) noexcept
		-> std::vector<std::pair<TNumber, TNumber>>
	{
		std::vector<std::pair<TNumber, TNumber>> result;
		if (data_size == 0) return result;
		// Сохраняем знак
		TNumber sign{ (data_size < 0) ? -1 : 1 };
		// Далее работаем без знака
		auto _abs = [](auto& x) { return (x < 0) ? -x : x;  };
		TNumber current_index{ 0 };
		TNumber _data_size{ _abs(data_size) };
		TNumber _range_size{ (range_size == 0) ? _data_size : _abs(range_size) };
		// Если размер диапазона отрицательный, переворачиваем список диапазонов
		if (range_size < 0)
		{
			current_index = std::move(_data_size) - 1;
			_data_size = -1;
			_range_size = -_range_size;
		}
		// Сравнение зависит от прямого или реверсного списка диапазонов.
		auto _compare = [&range_size, &_data_size](const auto& _index) -> bool
			{ return (range_size < 0) ? (_data_size < _index) : (_index < _data_size); };

		for (TNumber next_index{ current_index + _range_size }; _compare(current_index); next_index += _range_size)
		{
			// При формировании списка диапазонов восстанавливаем знак
			(_compare(next_index))
				? result.emplace_back(current_index * sign, next_index * sign)
				: result.emplace_back(current_index * sign, _data_size * sign);
			current_index = next_index;
		}

		result.shrink_to_fit();
		return result;
	};

	/**
	@brief Функция возведения в степень целого числа.

	@details В дополнении к стандартной библиотеке, которая не имеет перегруженной версии
	функции std::pow(), возвращающей целочисленное значение для возводимых в степень целых чисел.
	Используется алгоритм быстрого возведения в степень по схеме "справа на лево".
	Примечание! Данная функция может возводить в степень не только целые числа. Тип возвращаемого
	результата зависит от типа первого параметра, передаваемого в функцию.

	@param base: Возводимое в степень целое число.
	@param exp: Степень возведения.

	@return Целочисленный результат возведения целого числа в заданную степень.
	*/
	template<typename TBase, typename TExp = int>
	requires std::is_arithmetic_v<TBase> && std::is_integral_v<TExp>
	constexpr TBase ipow(TBase i_base, TExp i_exp)
	{
		// Обрабатываем пороговые значения
 		if (i_base == 1 || i_exp == 0) return 1;
		// Случаи с отрицательной степенью
		if (std::numeric_limits<TExp>::is_signed && i_exp < 0)
			return ((i_base == 0) ? std::numeric_limits<TBase>::min()
				: (i_base != -1) ? 0 : (i_exp & 1) ? -1 : 1);

		TBase res{ 1 };
		// Побитово считываем степень, начиная с младших разрядов (справа на лево)
		// и выполняем вычисления в зависимости от значения полученного бита.
		while (i_exp) {
			if (i_exp & 1) res *= i_base;
			if (i_exp >>= 1) i_base *= i_base;
		}

		return res;
	};

	/**
	@brief Позволяет создавать иерархически зависимые мьютексы. В цепочки иерархических мьютексов
	уровень следующего блокируемого мьютекса не может быть больше предыдущего.

	@details Пример использования:
		hmutex high_mutex(10000);
		hmutex low_mutex(5000);

		void same_fn()
		{
			std::lock_guard<low_mutex> lm(low_mutex);
			return;
		}

		void fn()
		{   // Сначала блокируется мьютекс с высоким уровнем, затем с низким. Наоборот - ошибка
			std::lock_guard<high_mutex> hm(high_mutex);
			same_fn();
		}
	*/
	class hmutex
	{
		// Важно, чтобы переменная this_thread_level была статической в рамках текущего потока.
		// Это позволяет для каждого отдельного потока создавать свои уровни иерархий мьютексов.
		inline static thread_local unsigned long this_thread_level{ ULONG_MAX };
		std::mutex _hmutex;
		unsigned long current_level; // Задаваемый уровень иерархии
		unsigned long previous_level{ 0 }; // Для сохранения предыдущего уровня иерархии

		void check_level()
		{
			if (this_thread_level <= current_level)
				throw std::logic_error("mutex hierarchy violated");
		}

		void update_level()
		{
			previous_level = this_thread_level;
			this_thread_level = current_level;
		}

	public:
		explicit hmutex(unsigned long level) : current_level{ level } {}
		// Копирование и перемещение запрещены
		hmutex(const hmutex&) = delete;
		hmutex& operator=(const hmutex&) = delete;
		hmutex(hmutex&&) = delete;
		hmutex& operator=(hmutex&&) = delete;

		void lock()
		{
			check_level();
			_hmutex.lock();
			update_level();
		}

		void unlock()
		{
			this_thread_level = previous_level;
			_hmutex.unlock();
		}

		bool try_lock()
		{
			check_level();
			if (!_hmutex.try_lock())
				return false;
			update_level();
			return true;
		}
	};

	template <typename TNumber>
		requires std::is_integral_v<TNumber> && std::is_arithmetic_v<TNumber>
	constexpr TNumber get_day_week_index(TNumber day, TNumber month, TNumber year)
	{
		constexpr auto _abs = [](TNumber n) ->TNumber { return (n < 0) ? -n : n; };

		if (auto m{ _abs(month) % 12 }) month = m;
		else month = 12;

		year = _abs(year);
		// По древнеримскому календарю год начинается с марта.
		// Январь и февраль относятся к прошлому году
		if ((month == 1) || (month == 2))
		{
			--year;
			month += 10;
		}
		else
			month -= 2;

		TNumber century{ year / 100 }; // количество столетий
		year -= century * 100; // год в столетии

		//Original: (day + (13*month-1)/5 + year + year/4 + century/4 - 2*century + 777) % 7;
		return static_cast<TNumber>((_abs(day) + (13 * month - 1) / 5 + (5 * year - 7 * century) / 4 + 777) % 7);
	};

	template <typename TNumber = unsigned int>
	constexpr std::string get_day_week_name(TNumber&& day, TNumber&& month, TNumber&& year)
	{
		std::vector<std::string> dw{ "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday","Saturday" };
		return dw[get_day_week_index(std::forward<TNumber>(day), std::forward<TNumber>(month), std::forward<TNumber>(year))];
	};

	constexpr int get_current_year()
	{
		std::chrono::year_month_day ymd{ std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) };
		return int(ymd.year());
	};


	/**
	@brief Функция проверяет вхождение одного набора данных в другой.

	@details Сортировка не требуется, порядок не важен, дублирование элементов допускается,
	относительный размер списков не принципиален.

	@param data: Список, с которым сравнивается pattern.
	@param pattern: Проверяемый список на вхождение в data.

	@return bool: True - если все элементы из pattern присутствуют в data в не меньшем количестве.
	*/
	template <typename TContainer = std::vector<int>>
		requires std::ranges::range<TContainer>
	bool is_includes_elements(TContainer&& data, TContainer&& pattern)
	{
		using TElement = typename std::remove_cvref_t<TContainer>::value_type;
		// Словарь-счетчик элементов списков
		std::unordered_map<TElement, int> map_counter;
		// Подсчитываем количество элементов в исходном списке
		for (const auto& elm : data)
			++map_counter[elm];
		// Вычитаем элементы из проверяемого списка
		// При этом, если в исходном списке нет такого элемента,
		// то он будет добавлен со знаком минус.
		for (const auto& elm : pattern)
			// Если хотя бы один элемент становится отрицательным, в проверяемого списке есть элементы, которых нет в исходном
			if(--map_counter[elm] < 0) return false;
			
		// Если в словаре нет отрицательных значений, то проверяемый список полностью содержится в исходном
		return true;
	};
}
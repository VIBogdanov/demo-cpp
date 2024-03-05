module;
#include <algorithm>
#include <iterator>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
export module Demo:Sundry;


//****************************************** Private code *************************************************
namespace
{
	template <typename TIterator, typename TItem = typename TIterator::value_type>
	auto _find_item_by_binary(const TIterator& first, const TIterator& last, const TItem& target)
		-> std::make_signed_t<decltype(std::ranges::distance(first, last))>
	{
		// Получаем размер массива данных
		auto _size = std::ranges::distance(first, last);

		using TIndex = decltype(_size);
		using TResult = std::make_signed_t<TIndex>;  //Оставлено для возможности установить иной тип возврата вручную
		static_assert(std::is_signed_v<TResult>, "Type TResult must be signed!");  //Купируем тонкую ошибку

		// Возвращаемый индекс найденного значения
		TResult i_result{ -1 };

		switch (_size)
		{
		case 1:
			i_result = (std::equal_to<TItem>{}(*first, target)) ? 0 : -1;
			[[fallthrough]];
		case 0:
			return i_result;
		}

		// Для перемещения по данным используем итератор вместо индекса, чтобы
		// была возможность работать с контейнерами, которые не поддерживают индексацию
		auto iter_element = last;

		// Метод std::advance поддерживает контейнеры как с индексацией (operator[]), так и без
		std::advance(iter_element, -1); // Указатель на последний элемент данных

		// Определяем порядок сортировки исходного массива
		const bool is_forward = std::greater_equal<TItem>{}(*iter_element, *first);

		// Стартуем с первого и последнего индекса массива одновременно
		TIndex i_first{ 0 }, i_last{ _size - 1 };
		// i_middle - одновременно индекс середины диапазона поиска и индекс искомого значения
		TIndex i_middle{ 0 };

		iter_element = first;
		TIndex iter_diff{ 0 }; //Текущее смещение для итератора в процессе поиска (вперед / назад)

		while (i_first <= i_last && i_result < 0)
		{
			// Вычисляем смещение итератора делением текущего диапазона поиска пополам
			iter_diff = ((i_first + i_last) >> 1) - i_middle;
			i_middle = (i_first + i_last) >> 1;

			// Смещаем итератор на середину нового диапазона поиска
			std::advance(iter_element, iter_diff);

			// Сужаем диапазон поиска в зависимости от результата сравнения и от направления сортировки
			if (std::equal_to<TItem>{}(*iter_element, target))
				i_result = static_cast<TResult>(i_middle);
			else
				(std::greater<TItem>{}(*iter_element, target))
				? (is_forward) ? i_last = i_middle - 1 : i_first = i_middle + 1
				: (is_forward) ? i_first = i_middle + 1 : i_last = i_middle - 1;
		}

		return i_result;
	};


	//Объявление разно типовой переменной. Реализацию см. ниже в теле функции _find_item_by_interpolation()
	namespace {
		template <typename T>
		T forward;
	}

	template <typename TIterator, typename TItem = typename TIterator::value_type>
		requires (std::is_arithmetic_v<TItem>)
	auto _find_item_by_interpolation(const TIterator& first, const TIterator& last, const TItem& target)
		-> std::make_signed_t < std::iter_difference_t<TIterator>>
	{
		// Получаем размер массива данных
		auto _size = std::ranges::distance(first, last);

		using TIndex = std::iter_difference_t<TIterator>;
		using TResult = std::make_signed_t<TIndex>;  //Оставлено для возможности установить иной тип возврата вручную
		static_assert(std::is_signed_v<TResult>, "Type TResult must be signed!");

		// Возвращаемый индекс найденного значения
		TResult i_result{ -1 };
		// Отрабатываем крайние случаи
		switch (_size)
		{
		case 1:
			i_result = (std::equal_to<TItem>{}(*first, target)) ? 0 : -1;
			[[fallthrough]];
		case 0:
			return i_result;
		}

		// Для перемещения по данным используем итератор вместо индекса, чтобы
		// была возможность работать с контейнерами, которые не поддерживают индексацию
		auto current_element = last;

		// Метод std::advance поддерживает контейнеры как с индексацией (operator[]), так и без,
		// а также позволяет работать с однонаправленными итераторами
		std::advance(current_element, -1); // Указатель на последний элемент данных

		/* Можно объединить под одним именем переменную разного типа
		* Объявление см. перед функцией
		* Определяем порядок сортировки исходного массива. */
		forward<bool> = std::greater_equal<TItem>{}(*current_element, *first);
		forward<TItem> = (forward<bool>) ? 1 : -1;

		// Стартуем с первого и последнего индекса массива одновременно
		TIndex i_first{ 0 }, i_last{ _size - 1 };
		// Индекс искомого значения
		TIndex i_current{ 0 };

		//Инициализируем итераторы для диапазона поиска
		auto first_element = first;
		auto last_element = current_element;

		// Возвращаем текущий указатель на первый элемент данных
		current_element = first;
		TIndex diff{ 0 };

		while (i_first <= i_last && i_result < 0)
		{
			/* Если искомый элемент вне проверяемого диапазона, выходим
			*  Эта проверка необходима, чтобы избежать зацикливания при неотсортированном исходном списке
			*  При проверке учитывается направление сортировки исходных данных.  */
			if (std::less<TItem>{}(forward<TItem> * target, forward<TItem> * (*first_element)) ||
				std::greater<TItem>{}(forward<TItem> * target, forward<TItem> * (*last_element))
				)
				return i_result;

			// Исключаем деление на 0, которое может возникнуть, если в искомом диапазоне элементы данных одинаковы
			if (auto elements_diff = *last_element - *first_element)
			{
				//Сохраняем текущую позицию для будущего корректного вычисления смещения
				diff = std::move(i_current);
				// Пытаемся вычислить положение искомого элемента в списке.
				i_current = static_cast<TIndex>(i_first + ((i_last - i_first) / elements_diff) * (target - *first_element));
			}
			else
				return static_cast<TResult>((std::equal_to<TItem>{}(*first_element, target)) ? i_first : -1);

			// Смещаем указатель на вычисленный индекс
			diff = i_current - std::move(diff);
			std::advance(current_element, diff);

			// Сужаем диапазон поиска в зависимости от результата сравнения и от направления сортировки
			if (std::equal_to<TItem>{}(*current_element, target))
				i_result = static_cast<TResult>(i_current);
			else
				// Одновременно смещаем итератор и индекс
				(std::greater<TItem>{}(*current_element, target))
				? (forward<bool>) ? (std::advance(last_element, (i_current - 1) - i_last), i_last = i_current - 1)
				: (std::advance(first_element, (i_current + 1) - i_first), i_first = i_current + 1)
				: (forward<bool>) ? (std::advance(first_element, (i_current + 1) - i_first), i_first = i_current + 1)
				: (std::advance(last_element, (i_current - 1) - i_last), i_last = i_current - 1);
		}

		return i_result;
	};
}


//****************************************** Public code *************************************************
export namespace sundry
{
	//constexpr int ZeroValue = int{ 0 };

	/**
	@brief Функция нахождения наибольшего общего делителя двух целых чисел без перебора методом Евклида.

	@details Используется алгоритм Евклида.
	Например, для чисел 20 и 12:
	20 % 12 = 8
	12 % 8 = 4
	8 % 4 = 0
	Искомый делитель равен 4

	Порядок следования входных значений не важен. Допускаются как положительные, так и отрицательные
	входные значения чисел в различных комбинациях. Возможны комбинации нулевых входных значений.

	@example get_common_divisor(20, 12) -> 4

	@param number_a, number_b - Заданные целые числа. По умолчанию 0.

	@return (int) Наибольший общий делитель
	*/
	template <typename TNumber = int>
		requires requires {
		std::is_integral_v<TNumber>;
		std::is_arithmetic_v<TNumber>;
	}
	auto get_common_divisor(const TNumber& number_a = 0, const TNumber& number_b = 0)
		-> TNumber
	{
		using TPair = std::pair<TNumber, TNumber>;

		// Определяем делимое и делитель. Делимое - большее число. Делитель - меньшее.
		auto [divisor, divisible] { static_cast<TPair>(std::minmax(std::abs(number_a), std::abs(number_b))) };

		if (divisor) // Вычисляем только если делитель не равен нулю.
			while (auto && _div{ divisible % divisor }) {
				divisible = std::move(divisor);
				divisor = std::move(_div);
			}
		else
			divisor = std::move(divisible);

		return divisor;
	};


	/**
	@brief Поиск в списке из чисел последовательного непрерывного интервала(-ов) чисел,
	сумма которых равна искомому значению.

	@details Суть алгоритма выражена формулой: Sum1 - Sum2 = Target >> Sum1 - Target = Sum2
	  - Вычислить все суммы от начала до текущей позиции.
	  - Для каждой суммы вычислить Sum - Target
	  - Найти полученное значение в списке сумм
	  - Если пара найдена, извлечь индексы и составить диапазон

	@example find_intervals(std::vector<int>{ 1, -3, 4, 5 }, 9) -> std::vector<std::pair<int, int>>{ (2, 3) }
			 find_intervals(std::vector<int>{ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0) -> std::vector<std::pair<int, int>>{ (0, 1), (4, 6), (8, 9), (9, 10) }

	@param elements - Массив с данными контейнерного типа
	@param target - Искомое значение

	@return (std::vector<std::pair<int, int>>) Результирующий список диапазонов. Диапазоны задаются в виде
		пары целых чисел, обозначающих индексы элементов списка - начальный и конечный индексы включительно.
		Если ни один диапазон не найден, возвращается пустой список.
	*/
	template <typename TContainer>
		requires(!std::is_array_v<TContainer>)
	auto find_intervals(const TContainer& elements, const typename TContainer::value_type& target)
		-> std::vector<std::pair<typename TContainer::size_type, typename TContainer::size_type>>
	{
		using TIndex = typename TContainer::size_type;
		using TElement = typename TContainer::value_type;
		//using TElement = std::remove_cvref_t<decltype(target)>;  //Альтернатива

		std::vector<std::pair<TIndex, TIndex>> result_list;
		std::unordered_multimap<TElement, TIndex> sum_dict;

		TElement sum{ 0 };
		TIndex idx{ 0 };

		// Суммируем элементы списка по нарастающей
		for (const auto& e : elements)
		{
			sum = std::move(sum) + e;

			// Если на очередной итерации полученная сумма равна искомому значению,
			// заносим диапазон от 0 до текущей позиции в результирующий список.
			if (sum == target)
				result_list.emplace_back(0, idx);

			//Ищем пару из уже вычисленных ранее сумм для значения (Sum - Target).
			if (sum_dict.contains(sum - target))
			{
				// Если пара найдена, извлекаем индексы и формируем результирующие диапазоны.
				auto [first, last] = sum_dict.equal_range(sum - target);
				for (; first != last; ++first)
					result_list.emplace_back(first->second + 1, idx);
			}

			// Сохраняем очередную сумму и ее индекс в словаре, где ключ - сама сумма.
			// У одной и той же суммы возможно несколько индексов
			sum_dict.emplace(sum, idx);
			++idx;
		}

		return result_list;
	};


	/**
	@brief Поиск элемента в массиве данных при помощи бинарного алгоритма. При поиске учитывается направление сортировки массива.

	@details В качестве входных данных могут быть использованы как обычные массивы, так и контейнеры из стандартной библиотеки.

	@example find_item_by_binary(std::vector<int>{ 1, 2, 3, 4, 5 }, 3) -> 2
			 find_item_by_binary(std::list<int>{ 1, 2, 3, 4, 5 }, 5) -> 4
			 find_item_by_binary(std::deque<int>{ 1, 2, 3, 4, 5 }, 1) -> 0
			 find_item_by_binary(std::string{ "abcdefgh" }, 'a') -> 0
			 find_item_by_binary(std::array<int, 3>{ 1, 2, 3 }, 3) -> 2

	@param elements - массив с данными контейнерного типа
	@param target - искомое значение

	@return (long long) Индекс позиции в массиве искомого значения. Если не найдено, вернет -1.
	*/
	template <typename T>
	concept TypeContainer =
		!std::is_array_v<T> &&
		std::ranges::range<T>;

	template <TypeContainer TContainer>
	auto find_item_by_binary(const TContainer& elements, const typename TContainer::value_type& target)
		-> std::make_signed_t<typename TContainer::size_type>
	{
		//Максимально обобщенный вариант для контейнеров
		return _find_item_by_binary(elements.begin(), elements.end(), target); //span не поддерживает cbegin/cend
	};


	/**
	@brief Поиск элемента в массиве данных при помощи бинарного алгоритма. При поиске учитывается направление сортировки массива.

	@details В качестве входных данных могут быть использованы  c-массивы.

	@example find_item_by_binary({ 'a', 'b', 'c'}, 'c') -> 2
			 find_item_by_binary({ -10.1, -20.2, -30.3, -40.4, -50.5, -60.6 }, -40.4) -> 3
			 find_item_by_binary("abcdefgh", 'd') -> 3

	@param elements - массив с данными
	@param target - искомое значение

	@return (long long) Индекс позиции в массиве искомого значения. Если не найдено, вернет -1.
	*/
	template <typename T, std::size_t N>
		requires (std::is_array_v<T[N]>)
	auto find_item_by_binary(const T(&elements)[N], const T& target)
		-> std::make_signed_t<decltype(N)>
	{
		// Перегруженная версия функции для обработки обычных числовых и строковых c-массивов.
		if ((typeid(T) == typeid(char)) and !*(std::end(elements) - 1))
			// Если массив - это строковая константа, заканчивающаяся на 0, смещаем конечный индекс для пропуска нулевого символа
			return _find_item_by_binary(std::ranges::begin(elements), std::ranges::end(elements) - 1, target);
		else
			return _find_item_by_binary(std::ranges::begin(elements), std::ranges::end(elements), target);
	};


	/*
	@brief Функция поиска заданного значения в одномерном числовом массиве контейнерного типа. В качестве алгоритма поиска
	используется метод интерполяции.
	
	@details Алгоритм похож на бинарный поиск. Отличие в способе поиска срединного элемента.
    При интерполяции производится попытка вычислить положение искомого элемента, а не просто поделить список
    пополам.
    Внимание!!! Входной массив данных обязательно должен быть отсортирован, иначе результат будет не определён.
	Учитывается направление сортировки. Т.к. в алгоритме используются арифметические операции, список и искомое
	значение должны быть числовыми. Строковые литералы не поддерживаются.

	@example find_item_by_interpolation(std::vector<int>{ -1, -2, 3, 4, 5 }, 4) -> 3

    @param elements - массив числовых данных для поиска
    @param target - искомое значение

    @return Индекс позиции в массиве искомого значения. Если не найдено, вернет -1.
	*/
	template <typename T>
	concept TypeContainer_a = requires {
		!std::is_array_v<T>;
		std::ranges::range<T>;
		std::is_arithmetic_v<T>;
	};

	template <TypeContainer_a TContainer>
	auto find_item_by_interpolation(const TContainer& elements, const typename TContainer::value_type& target)
		-> std::make_signed_t<typename TContainer::size_type>
	{
		//Максимально обобщенный вариант для контейнеров
		return _find_item_by_interpolation(elements.begin(), elements.end(), target); //span не поддерживает cbegin/cend
	};


	/*
	@brief Функция поиска заданного значения в одномерном числовом c-массиве. В качестве алгоритма поиска используется
	метод интерполяции.

	@details Алгоритм похож на бинарный поиск. Отличие в способе поиска срединного элемента.
	При интерполяции производится попытка вычислить положение искомого элемента, а не просто поделить список
	пополам.
	Внимание!!! Входной массив данных обязательно должен быть отсортирован, иначе результат будет не определён.
	Учитывается направление сортировки. Т.к. в алгоритме используются арифметические операции, список и искомое
	значение должны быть числовыми. Строковые литералы не поддерживаются.

	@example find_item_by_interpolation({ 1, 2, 3, 4, 5 }, 2) -> 1

	@param elements - с-массив числовых данных для поиска
	@param target - искомое значение

	@return Индекс позиции в массиве искомого значения. Если не найдено, вернет -1.
	*/
	template <typename T, std::size_t N>
		requires requires {
			std::is_array_v<T[N]>;
			std::is_arithmetic_v<T>;
		}
	auto find_item_by_interpolation(const T(&elements)[N], const T& target)
		-> std::make_signed_t<decltype(N)>
	{
		// Перегруженная версия функции для обработки обычных числовых и строковых c-массивов.
		if ((typeid(T) == typeid(char)) and !*(std::end(elements) - 1))
			// Если массив - это строковая константа, заканчивающаяся на 0, смещаем конечный индекс для пропуска нулевого символа
			return _find_item_by_interpolation(std::ranges::begin(elements), std::ranges::end(elements) - 1, target);
		else
			return _find_item_by_interpolation(std::ranges::begin(elements), std::ranges::end(elements), target);
	};
}
﻿module;
#include <algorithm>
#include <iterator>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
export module Demo:Sundry;

import :Assistools;

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
		auto iter_element{ std::ranges::prev(last) }; // Указатель на последний элемент данных

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
		//Инициализируем итераторы для диапазона поиска
		auto first_element{ first };
		auto last_element{ std::ranges::prev(last) };

		/* Можно объединить под одним именем переменные разного типа
		* Объявление см. перед функцией
		* Определяем порядок сортировки исходного массива. */
		forward<bool> = std::greater_equal<TItem>{}(*last_element, *first_element);
		forward<TItem> = (forward<bool>) ? 1 : -1;

		// Стартуем с первого и последнего индекса массива одновременно
		TIndex i_first{ 0 }, i_last{ _size - 1 };
		// Индекс искомого значения
		TIndex i_current{ 0 };

		auto current_element{ first };
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


	/**
	brief Вспомогательная подпрограмма для функции find_nearest_number.
	
	details Просматривает цифры левее текущей цифры исходного числа с целью поиска большего или
    меньшего значения в зависимости от направления поиска. В случае успешного поиска, выполняет
	перестановку цифр и сортирует правую часть числа по возрастанию или убыванию в зависимости от
	направления поиска.

    @param result_type - используется только для определения корректного типа возвращаемого значения
    @param digits_list - массив цифр исходного числа
	@param digit_iter - текущая позиция цифры, относительно которой выполняется перестановка
	@param previous (bool) - Направление поиска: ближайшее большее или меньшее. True - меньшее, False - большее

    @return  Возвращает найденное число или 0 в случае безуспешного поиска
	*/
	template <typename T>
	concept TypeContainer_ia = requires {
		std::is_integral_v<T>;
		std::ranges::range<T>;
		std::is_arithmetic_v<T>;
	};

	template <typename TResult, TypeContainer_ia TContainer, typename TIterator = typename TContainer::iterator>
	auto _do_find_nearest(const TResult& result_type, const TContainer& digits_list, const TIterator& digit_iter, const bool& previous = true)
		-> TResult
	{
		// Параметр result_type нужен только для определения корректного типа возвращаемого значения

		TResult result{ 0 };

		// создаем копию передаваемого списка, дабы не влиять на оригинальный список
		TContainer _digits_list{ digits_list };
		// Текущая цифра, с которой будем сравнивать
		const auto _digit_iter{ std::ranges::next(_digits_list.rbegin(),
								std::ranges::distance(digits_list.rbegin(), digit_iter)) };
		// Сравниваем значения итераторов в зависимости от направления поиска: большего или меньшего числа
		auto _compare = [previous, _digit_iter](const auto x) -> bool
			{ return (previous) ? *x > *_digit_iter : *x < *_digit_iter; };

		// Просматриваем все цифры левее текущей позиции.
		for (auto current_iter{ std::ranges::next(_digit_iter) };
			current_iter != _digits_list.rend(); ++current_iter)
		{
			// сравниваем с цифрой текущей позиции, учитывая направление поиска
			if (_compare(current_iter))
			{
				// в случае успешного сравнения, переставляем местами найденную цифру с текущей
				std::iter_swap(current_iter, _digit_iter);
				// если первая цифра полученного числа после перестановки не равна 0,
				// выполняем сортировку правой части числа
				if (*_digits_list.begin() > 0)
				{
					// сортируем правую от найденной позиции часть числа (по возрастанию или по убыванию)
					// с учетом направления поиска
					std::sort(_digits_list.rbegin(), current_iter,
								[previous](const auto& a, const auto& b) -> bool
								{ return (previous) ? a < b : b < a; });

					// Собираем из массива цифр результирующее число
					std::ranges::for_each(std::as_const(_digits_list),
						[&result](const auto& d) -> void { result = result * 10 + d; });
				}
				return result;
			}
		}

		return result;
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
		// Определяем делимое и делитель. Делимое - большее число. Делитель - меньшее.
		auto [divisor, divisible] = std::ranges::minmax({ std::abs(number_a), std::abs(number_b) });

		if (divisor) // Вычисляем только если делитель не равен нулю.
			while (auto _div{ divisible % divisor }) {
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

	@example find_intervals(std::vector<int>{ 1, -3, 4, 5 }, 9) -> (2, 3)
			 find_intervals(std::vector<int>{ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0) -> (0, 1), (4, 6), (8, 9), (9, 10)

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

		result_list.shrink_to_fit();
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


	/**
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


	/**
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


	/**
	@brief Функция поиска ближайшего целого числа, которое меньше или больше заданного и состоит из тех же цифр.

	@example find_nearest_number(273145) -> 271543
			 find_nearest_number(273145, previous=false) -> 273154
			 find_nearest_number(-273145) -> -273154

    @param number - целое число, относительно которого осуществляется поиск. Допускаются отрицательные значения.
	@param previous (bool) - Направление поиска: ближайшее меньшее или большее. По-умолчанию True - ближайшее меньшее.

    @return Найденное число. Если поиск безуспешен, возвращается 0.
	*/
	template <typename TNumber = int>
		requires requires {
		std::is_integral_v<TNumber>;
		std::is_arithmetic_v<TNumber>;
	}
	auto find_nearest_number(const TNumber& number, const bool& previous = true)
		-> TNumber
	{
		TNumber result{ 0 };
		// Сохраняем знак числа и определяем направление поиска (больше или меньше заданного числа)
		const TNumber sign_number{ (number < 0) ? -1 : 1 };
		const bool is_previous{ (number < 0) ? !previous : previous };
		// Разбиваем заданное число на отдельные цифры
		auto digits_list = assistools::inumber_to_digits(number);
		// список для накопления результатов поиска
		std::vector<TNumber> result_list{ };
		// цикл перебора цифр заданного числа справа на лево (с хвоста к голове) кроме первой цифры
		for (auto it_digit{ digits_list.rbegin() }; it_digit != std::ranges::prev(digits_list.rend()); ++it_digit)
			// вызываем подпрограмму поиска большего или меньшего числа в зависимости от направления поиска
			// result передаем только для того, чтобы получить корректный тип возвращаемого значения
			if (auto res = _do_find_nearest(result, digits_list, it_digit, is_previous))
				result_list.emplace_back(res);
		// Если список результирующих чисел не пуст, находим наибольшее или наименьшее число
		// в зависимости от направления поиска и восстанавливаем знак числа.
		if (!result_list.empty())
		{
			auto it_result = (is_previous)
				? std::ranges::max_element(std::as_const(result_list))
				: std::ranges::min_element(std::as_const(result_list));
			result = (*it_result) * sign_number;
		}

		return result;
	};


	/**
	@brief Функция сортировки методом пузырька. Сортирует заданный список по месту.

	@details В отличии от классического метода, функция за каждую итерацию одновременно ищет как максимальное
	значение, так и минимальное. На следующей итерации диапазон поиска сокращается не на один элемент, а на два.
	Кроме того, реализована сортировка как по возрастанию, так и по убыванию.

    @param first - Итератор, указывающий на первый элемент данных.
	@param last - Итератор, указывающий за последний элемент данных.
	@param revers (bool) - Если True, то сортировка по убыванию. Defaults to False.
	*/
	template <typename TIterator>
	void sort_by_bubble(TIterator first, TIterator last, const bool& revers = false)
	{
		// Устанавливаем итераторы на первый и последний элементы данных.
		// Далее работаем с итераторами без прямого использование операторов +/- и [][
		// Все это позволяет сортировать практически любые контейнеры
		auto it_start{ first };
		auto it_end{ std::ranges::prev(last) };
		// Флаг, исключающий "пустые" циклы, когда список достигает состояния "отсортирован" на одной из итераций
		bool is_swapped{ false };
		// Сравниваем в зависимости от направления сортировки
		auto _compare = [revers](const auto& curr, const auto& next) -> bool
			{ return (revers) ? curr < next : next < curr; };

		while (it_start != it_end)
		{
			// До последнего значения не доходим. Это максимум текущего диапазона.
			for (auto it_current{ it_start }, it_next{ std::ranges::next(it_current) };
				it_current != it_end; ++it_current, ++it_next)
			{
				// Если текущий элемент больше следующего, то переставляем их местами.Это потенциальный максимум.
				if (_compare(*it_current, *it_next))
				{
					std::iter_swap(it_current, it_next);
					//Одновременно проверяем на потенциальный минимум, сравнивая с первым элементом текущего диапазона.
					if (it_current != it_start && _compare(*it_start, *it_current))
						std::iter_swap(it_start, it_current);
					// Список пока не отсортирован, т.к. потребовались перестановки
					is_swapped = true;
				}
			}
			// После каждой итерации по элементам списка, сокращаем длину проверяемого диапазона на 2,
			// т.к.на предыдущей итерации найдены одновременно минимум и максимум
			if (is_swapped)
			{
				std::advance(it_start, 1);
				if (it_start != it_end)
					std::advance(it_end, -1);
				is_swapped = false;
			}
			else
				// Если за итерацию перестановок не было, то список уже отсортирован. Выходим из цикла
				it_start = it_end;
		}
	};
}
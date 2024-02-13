#pragma once


//****************************************** Public header *************************************************
namespace sundry
{
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

	@param (int&, int&) a, b Заданные целые числа. По умолчанию 0.

	@return (int) Наибольший общий делитель
	*/
	int get_common_divisor(const int& = 0, const int& = 0);


	/**
	@brief Поиск элемента в массиве данных при помощи бинарного алгоритма. При поиске учитывается направление сортировки массива.

	@details В качестве входных данных могут быть использованы как обычные массивы, так и контейнеры из стандартной библиотеки.

	@example find_item_by_binary(std::vector<int>{ 1, 2, 3, 4, 5 }, 3) -> 2
			 find_item_by_binary(std::list<int>{ 1, 2, 3, 4, 5 }, 5) -> 4
			 find_item_by_binary(std::deque<int>{ 1, 2, 3, 4, 5 }, 1) -> 0
			 find_item_by_binary(std::string{ "abcdefgh" }, 'a') -> 0

	@param (TContainer<T>&) Массив с данными контейнерного типа
	@param (T&) Искомое значение

	@return (int) В качестве входных данных могут быть использованы контейнеры из стандартной библиотеки.
	*/
	template <template <class...> typename TContainer, typename T>
	decltype(auto) find_item_by_binary(const TContainer<T>&, const T&);


	/**
	@brief Поиск элемента в массиве данных при помощи бинарного алгоритма. При поиске учитывается направление сортировки массива.

	@details В качестве входных данных могут быть использованы  c-массивы.

	@example find_item_by_binary({ 'a', 'b', 'c'}, 'c') -> 2
			 find_item_by_binary({ -10.1, -20.2, -30.3, -40.4, -50.5, -60.6 }, -40.4) -> 3
			 find_item_by_binary("abcdefgh", 'd') -> 3

	@param (T(&)[N]) C-массив числовых и строковых данных
	@param (T&) Искомое значение

	@return (int) Индекс позиции в массиве искомого значения. Если не найдено, вернет -1.
	*/
	template <typename T, std::size_t N>
	decltype(auto) find_item_by_binary(const T(&)[N], const T&);


	/**
	@brief Поиск элемента в массиве данных при помощи бинарного алгоритма. При поиске учитывается направление сортировки массива.

	@details В качестве входных данных могут быть использованы  массивы из стандартной библиотеки.

	@example find_item_by_binary(std::array<int, 3>{ 1, 2, 3 }, 3) -> 2

	@param (TArray<T, N>&) std::array-массив данных
	@param (T&) Искомое значение

	@return (int) Индекс позиции в массиве искомого значения. Если не найдено, вернет -1.
	*/
	template<template<typename T, std::size_t N> typename TArray, typename T, std::size_t N>
	decltype(auto) find_item_by_binary(const TArray<T, N>&, const T&);
}


//****************************************** Private code *************************************************
namespace
{
	template <typename TIterator, typename T>
	decltype(auto) _find_item_by_binary(const TIterator& first, const TIterator& last, const T& target)
	{
		// Получаем размер массива данных
		auto _size = std::distance(first, last);

		// Возвращаемый индекс найденного значения
		decltype(_size) i_result = -1;

		switch (_size)
		{
		case 1:
			i_result = (std::equal_to<T>{}(*first, target)) ? 0 : -1;
			[[fallthrough]];
		case 0:
			return i_result;
		}

		// Определяем порядок сортировки исходного массива
		bool is_forward = std::greater_equal<>{}(*(last - 1), *first);

		// Стартуем с первого и последнего индекса массива одновременно
		decltype(_size) i_first = 0, i_middle = 0;
		decltype(_size) i_last = (_size - 1);

		// Для перемещения по данным используем итератор вместо индекса, чтобы
		// была возможность работать с контейнерами, которые не поддерживают индексацию
		auto iter_element = first;
		decltype(_size) iter_diff{ 0 }; //Текущее смещение для итератора в процессе поиска (вперед / назад)

		while (i_first <= i_last and i_result < 0)
		{
			// Вычисляем смещение итератора делением текущего диапазона поиска пополам
			iter_diff = ((i_first + i_last) >> 1) - i_middle;
			i_middle = (i_first + i_last) >> 1;

			// Смещаем итератор на середину нового диапазона поиска
			std::advance(iter_element, iter_diff);

			// Сужаем диапазон поиска в зависимости от результата сравнения и от направления сортировки
			if (std::equal_to<T>{}(*iter_element, target))
				i_result = i_middle;
			else
				(std::greater<T>{}(*iter_element, target))
				? (is_forward) ? i_last = i_middle - 1 : i_first = i_middle + 1
				: (is_forward) ? i_first = i_middle + 1 : i_last = i_middle - 1;
		}

		return i_result;
	}
}


//****************************************** Public code *************************************************
namespace sundry
{
	template <template <class...> typename TContainer, typename T>
	decltype(auto) find_item_by_binary(const TContainer<T>& elements, const T& target)
	{
		return _find_item_by_binary(elements.begin(), elements.end(), target);
	}


	template <typename T, std::size_t N>
	decltype(auto) find_item_by_binary(const T(&elements)[N], const T& target)
	{
		// Перегруженная версия функции для обработки обычных числовых и строковых c-массивов.
		if ((typeid(T) == typeid(char)) and !*(std::end(elements) - 1))
			// Если массив - это строковая константа, заканчивающаяся на 0, смещаем конечный индекс для пропуска нулевого символа
			return _find_item_by_binary(std::begin(elements), std::end(elements) - 1, target);
		else
			return _find_item_by_binary(std::begin(elements), std::end(elements), target);
	}


	template <template<typename T, std::size_t N> typename TArray, typename T, std::size_t N>
	decltype(auto) find_item_by_binary(const TArray<T, N>& elements, const T& target)
	{
		// Перегруженная версия функции для обработки std::array.
		return _find_item_by_binary(elements.begin(), elements.end(), target);
	}
}
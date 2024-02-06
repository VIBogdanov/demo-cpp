#pragma once

#include <vector>


namespace sundry {
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

	@param (int&) a, b Заданные целые числа

	@return (int) Наибольший общий делитель
	*/
	int get_common_divisor(const int& = 0, const int& = 0);


	/**
	@brief Поиск элемента в массиве данных при помощи бинарного алгоритма. При поиске учитывается направление сортировки массива.

	@details В качестве входных данных могут быть использованы как обычные массивы, так и контейнеры из стандартной библиотеки.

	@example find_item_by_binary(vector<int> { 1, 2, 3, 4, 5 }, 3) -> 2
			find_item_by_binary({ 'a', 'b', 'c'}, 'b') -> 1

	@param (TContainer<T>&) Массив с данными
	@param (T&) Искомое значение

	@return (int) Индекс позиции в массиве искомого значения. Если не найдено, вернет -1.
	*/
	template <template <class...> typename TContainer, typename T>
	int find_item_by_binary(const TContainer<T>& elements, const T& target)
	{
		switch (elements.size())
		{
		case 0:
			return -1;
		case 1:
			return (*(elements.begin()) == target) ? 0 : -1;
		}

		// Стартуем с первого и последнего индекса массива одновременно
		size_t i_first = 0;
		size_t i_last = elements.size() - 1;

		// Определяем порядок сортировки исходного массива
		bool is_forward = (*(--elements.end()) >= *(elements.begin()));

		// Возвращаемый индекс найденного значения
		int i_target = -1;

		while (i_first <= i_last and i_target < 0)
		{
			// Делим текущий остаток массива пополам
			size_t i_middle = (i_first + i_last) >> 1;

			// Сравниваем срединный элемент с искомым значением
			// Смещаем начальный или конечный индексы в зависимости
			// от результата сравнения и от направления сортировки
			auto it_elem = elements.begin();
			std::advance(it_elem, i_middle);

			if (*it_elem < target)
				(is_forward) ? i_first = i_middle + 1 : i_last = i_middle - 1;
			else if (*it_elem > target)
				(is_forward) ? i_last = i_middle - 1 : i_first = i_middle + 1;
			else
				i_target = static_cast<int>(i_middle);
		}

		return i_target;
	}



	template <typename T, size_t N>
	int find_item_by_binary(const T(&elements)[N], const T& target)
	{
		// Перегруженная версия функции для обработки обычных числовых и строковых массивов.
		auto _size = N;;
		// Если массив - это строковая константа, заканчивающаяся на 0, смещаем конечный индекс для пропуска нулевого символа
		if ((typeid(*("")) == typeid(*elements)) and !*(elements + _size))
			--_size;

		return sundry::find_item_by_binary(std::vector<T>(elements, elements + _size), target);
	}
}
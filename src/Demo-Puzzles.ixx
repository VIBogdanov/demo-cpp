module;
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <unordered_map>
export module Demo:Puzzles;

import :Assistools;

export namespace puzzles
{
	/**
	@brief Подсчитывает минимальное количество перестановок, которое необходимо произвести для того,
	чтобы из исходного списка (source_list) получить целевой список (target_list).

	@details При этом порядок следования и непрерывность списков не имеют значения.
	Например для списков:
	[10, 31, 15, 22, 14, 17, 16]
	[16, 22, 14, 10, 31, 15, 17]
	Требуется выполнить три перестановки для приведения списков в идентичное состояние.

	@param source_list: Исходный список.
	@param target_list: Целевой список.

	@return Минимальное количество перестановок. В случае ошибки: -1.
	*/
	template <typename TContainer = std::vector<int>>
	auto get_number_permutations(const TContainer& source_list, const TContainer& target_list)
		-> std::make_signed_t<typename TContainer::size_type>
	{
		using TIndex = typename TContainer::size_type;
		using TValue = typename TContainer::value_type;
		// Если списки не согласованы по размеру, выходим с ошибкой
		if (source_list.size() != target_list.size()) return -1;
		// Формируем список из номеров позиций для каждого значения из целевого списка
		// Само значение является ключом
		std::unordered_map<TValue, TIndex> target_indexes;
		for (TIndex index{ 0 }; const TValue& item : target_list)
			target_indexes.emplace(item, index++);
		// Попарно сравниваем целевые номера позиций для значений исходного списка.
		// Если номера позиций не по возрастанию, то требуется перестановка
		TIndex count_permutations{ 0 };
		for (TIndex prev_idx{ 0 }, target_idx{ 0 }; const TValue& source_item : source_list)
		{
			// На случай, если списки не согласованы по значениям
			try
			{
				target_idx = target_indexes.at(source_item);
			}
			catch (const std::out_of_range&)
			{
				return -1;
			}

			if (prev_idx > target_idx)
				++count_permutations;
			else
				prev_idx = target_idx;
		}

		return count_permutations;
	};


	/**
	@brief Олимпиадная задача. Необходимо определить наибольший номер страницы X книги,
    с которой нужно начать читать книгу, чтобы ровно 'count' номеров страниц, начиная
    со страницы X и до последней страницей 'pages', заканчивались на цифры из списка  'digits'.

    @details Например:
    - вызов get_pagebook_number(1000000000000, 1234, [5,6]) вернет 999999993835
    - вызов get_pagebook_number(27, 2, [8,0]) вернет 18
    - вызов get_pagebook_number(20, 5, [4,7]) вернет 0

    @param pages: Количество страниц в книге
	@param count: Количество страниц заканчивающихся на цифры из списка digits
	@param digits: Список цифр, на которые должны заканчиваться искомые страницы

    @return Номер искомой страницы или 0 в случае безуспешного поиска
	*/
	template <typename TContainer = std::vector<int>>
	auto get_pagebook_number(const typename TContainer::value_type& pages,
							const typename TContainer::value_type& count,
							const TContainer& digits)
		-> typename TContainer::value_type
	{
		// Отрабатываем некорректные параметры
		if (count <= 0 || pages < count || digits.size() == 0) return 0;
		// Индекс может быть отрицательным
		using TIndex = std::make_signed_t<typename TContainer::size_type>;
		using TValue = typename TContainer::value_type;

		std::vector<TValue> _digits{};
		_digits.reserve(digits.size());
		auto get_near_number = [pages](const auto& last_digit) -> TValue
			{
				// На всякий случай отбрасываем минус и от многозначных чисел оставляем последнюю цифру
				TValue _last_digit{ ((last_digit < 0) ? -last_digit : last_digit) % 10 };
				return (pages - _last_digit) / 10 * 10 + _last_digit;
			};
		// Формируем список с ближайшими меньшими числами, оканчивающиеся на цифры из списка digits
		std::ranges::transform(digits, std::back_inserter(_digits), get_near_number);
		// Полученный список обязательно должен быть отсортирован в обратном порядке
		std::ranges::sort(_digits, [](const auto& a, const auto& b) { return b < a; });
		// Заодно удаляем дубликаты
		_digits.erase(std::unique(_digits.begin(), _digits.end()), _digits.end());
		// Для чистоты вычислений приводим тип
		TValue _size = static_cast<TValue>(_digits.size());
		//Вычисляем позицию числа в списке digits, которое соответствует смещению count
		TIndex idx{ (count % _size) - 1 };
		// Т.к.последующая последняя цифра повторяется через 10,
		// вычисляем множитель с учетом уже вычисленных значений
		TValue multiplier{ (count - 1) / _size };

		return ((idx < 0) ? *std::ranges::next(_digits.end(), idx)
			: *std::ranges::next(_digits.begin(), idx)) - (multiplier * 10);
	};


	/**
	@brief Сформировать все возможные уникальные наборы чисел из указанных цифр.
    Цифры можно использовать не более одного раза в каждой комбинации.
    При этом числа не могут содержать в начале 0 (кроме самого нуля).

    @param digits - Список заданных цифр

    @return Список уникальных комбинаций
	*/
	template <typename TContainer = std::vector<int>>
	requires std::ranges::range<TContainer> && std::is_integral_v<typename std::remove_cvref_t<TContainer>::value_type>
	auto get_combination_numbers(TContainer&& digits)
		-> std::vector<std::vector<typename std::remove_cvref_t<TContainer>::value_type>>
	{
		using TNumber = typename std::remove_cvref_t<TContainer>::value_type;
		using TResVector = std::vector<TNumber>;
		// Сохраняем копию исходного списка для формирования комбинаций перестановок
		//std::vector<TNumber> _digits{ std::forward<TContainer>(digits) }; // Идеальный случай, но ограничен только vector
		// Т.к. неизвестно какой тип контейнера будет передан, используем максимально обобщенный вариант инициализации
		TResVector _digits{ std::make_move_iterator(std::ranges::begin(digits)),
							std::make_move_iterator(std::ranges::end(digits)) };
		auto _size = _digits.size();
		// Результат - список списков
		std::vector<TResVector> result;

		switch (_size)
		{
			case 1:
				result.emplace_back(_digits);
				[[fallthrough]];
			case 0:
				return result;
		}
		// Запускаем цикл перестановки одиночных цифр
		do
		{
			// Помещаем в результирующий список комбинацию из одиночных цифр
			result.emplace_back(_digits);
			// Сразу же формируем число из всего набора цифр
			if (*_digits.begin() != 0)
				result.push_back({ assistools::inumber_from_digits(_digits) });
			/* 
			Кроме одиночных, формируем комбинации с двух - , трех - ... N - числами.
			Максимальный N равен размеру заданного списка одиночных цифр минус 1,
			т.к. число из полного набора цифр уже сформировано.
			Начинаем с двухзначных, т.к. комбинация из одиночных цифр уже сформирована
			*/
			for (decltype(_size) N{ 2 }; N < _size; ++N)
			{
				// Формируем окно выборки цифр для формирования двух-, трех- и т.д. чисел
				auto it_first_digit{ _digits.begin() };
				auto it_last_digit{ std::ranges::next(it_first_digit, N) };
				TResVector _buff;
				_buff.reserve(_size - N + 1);
				while (it_first_digit != it_last_digit)
				{
					// Числа, начинающиеся с 0, пропускаем
					if (*it_first_digit != 0)
					{
						// Комбинируем полученное число с цифрами оставшимися вне окна выборки
						_buff.assign(_digits.begin(), it_first_digit); // Цифры слева
						// Формируем число из цифр, отобранных окном выборки
						_buff.emplace_back(assistools::inumber_from_digits(it_first_digit, it_last_digit));
						_buff.insert(_buff.end(), it_last_digit, _digits.end()); // Цифры справа
						result.emplace_back(_buff);
					}

					if (it_last_digit != _digits.end())
					{
						//Смещаем окно выборки
						++it_first_digit;
						++it_last_digit;
					}
					// Иначе, если окно выборки достигло конец списка цифр, выходим из цикла
					else it_first_digit = it_last_digit;
				}
			}
		}
		// Формируем следующую комбинацию одиночных цифр
		while (std::next_permutation(_digits.begin(), _digits.end()));
		// Сортируем для удобства восприятия (необязательно).
		std::ranges::sort(result);
		return result;
	};
} 
module;
#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <stack>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
export module Demo:Sundry;

import :Assistools;

//****************************************** Public declaration *************************************************
export namespace sundry
{
	/**
	Перечисление используется в sort_by_shell для обозначения метода,
	применяемого для разбиения исходного списка на подсписки для сортировки.
	*/
	enum class SortMethod
	{
		SHELL,
		HIBBARD,
		SEDGEWICK,
		KNUTH,
		FIBONACCI
	};
}

//****************************************** Private code *************************************************
namespace
{
	template <typename TIterator>
	auto _find_item_by_binary(const TIterator first, const TIterator last, const std::iter_value_t<TIterator>& target)
		-> std::make_signed_t <std::iter_difference_t<TIterator>>
	{
		// Получаем размер массива данных
		auto _size{ std::ranges::distance(first, last) };
		
		using TIndex = decltype(_size);
		using TResult = std::make_signed_t<TIndex>;  //Оставлено для возможности установить иной тип возврата вручную
		static_assert(std::is_signed_v<TResult>, "Type TResult must be signed!");  //Купируем тонкую ошибку

		// Возвращаемый индекс найденного значения
		TResult i_result{ -1 };

		switch (_size)
		{
		case 1:
			i_result = (*first == target) ? 0 : -1;
			[[fallthrough]];
		case 0:
			return i_result;
		}

		// Для перемещения по данным используем итератор вместо индекса, чтобы
		// была возможность работать с контейнерами, которые не поддерживают индексацию
		auto iter_element{ std::ranges::prev(last) }; // Указатель на последний элемент данных

		// Определяем порядок сортировки исходного массива
		const bool is_forward{ *first <= *iter_element };

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
			if (*iter_element == target)
				i_result = static_cast<TResult>(i_middle);
			else
				((target < *iter_element) ^ is_forward) ? i_first = i_middle + 1 : i_last = i_middle - 1;
		}

		return i_result;
	};


	//Объявление разно типовой переменной. Реализацию см. ниже в теле функции _find_item_by_interpolation()
	template <typename T> T is_forward;

	template <typename TIterator, typename TItem = std::iter_value_t<TIterator>>
		requires std::is_arithmetic_v<TItem>
	auto _find_item_by_interpolation(const TIterator first, const TIterator last, const TItem& target)
		-> std::make_signed_t <std::iter_difference_t<TIterator>>
	{
		// Получаем размер массива данных
		auto _size{ std::ranges::distance(first, last) };

		using TIndex = std::iter_difference_t<TIterator>;
		using TResult = std::make_signed_t<TIndex>;  //Оставлено для возможности установить иной тип возврата вручную
		static_assert(std::is_signed_v<TResult>, "Type TResult must be signed!");

		// Возвращаемый индекс найденного значения
		TResult i_result{ -1 };
		// Отрабатываем крайние случаи
		switch (_size)
		{
		case 1:
			i_result = (*first == target) ? 0 : -1;
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
		is_forward<bool> = (*first_element <= *last_element);
		is_forward<TItem> = (is_forward<bool>) ? 1 : -1;

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
			if ((is_forward<TItem> * target < is_forward<TItem> * (*first_element)) ||
				(is_forward<TItem> * (*last_element) < is_forward<TItem> * target)
				)
				return i_result;

			// Исключаем деление на 0, которое может возникнуть, если в искомом диапазоне элементы данных одинаковы
			if (auto elements_diff = *last_element - *first_element)
			{
				//Сохраняем текущую позицию для будущего корректного вычисления смещения
				diff = std::move(i_current);
				// Пытаемся вычислить положение искомого элемента в списке.
				i_current = static_cast<TIndex>(i_first + ((i_last - i_first) / elements_diff) * (target - (*first_element)));
			}
			else
				return static_cast<TResult>((*first_element == target) ? i_first : -1);

			// Смещаем указатель на вычисленный индекс
			diff = i_current - std::move(diff);
			std::advance(current_element, diff);

			// Сужаем диапазон поиска в зависимости от результата сравнения и от направления сортировки
			if (*current_element == target)
				i_result = static_cast<TResult>(i_current);
			else
				// Одновременно смещаем итератор и индекс
				((target < *current_element) ^ is_forward<bool>)
					? (std::advance(first_element, (i_current + 1) - i_first), i_first = i_current + 1)
					: (std::advance(last_element, (i_current - 1) - i_last), i_last = i_current - 1);
		}

		return i_result;
	};


	/**
	@brief Вспомогательная подпрограмма для функции find_nearest_number.
	
	@details Просматривает цифры левее текущей цифры исходного числа с целью поиска большего или
    меньшего значения в зависимости от направления поиска. В случае успешного поиска, выполняет
	перестановку цифр и сортирует правую часть числа по возрастанию или убыванию в зависимости от
	направления поиска.

    @param digits_list - массив цифр исходного числа
	@param digit_iter - текущая позиция цифры, относительно которой выполняется перестановка
	@param previous (bool) - Направление поиска: ближайшее большее или меньшее. True - меньшее, False - большее

    @return  Возвращает найденное число или 0 в случае безуспешного поиска
	*/

	template <typename TResult, typename TContainer, typename TIterator = typename TContainer::iterator>
		requires std::ranges::range<TContainer>&&
				std::is_integral_v<typename TContainer::value_type>&&
				std::is_arithmetic_v<typename TContainer::value_type>&&
				std::is_convertible_v<typename TContainer::value_type, TResult>
	auto _do_find_nearest(const TContainer& digits_list, const TIterator digit_iter, const bool previous = true)
		-> TResult
	{
		// Шаблон TResult нужен только для определения корректного типа возвращаемого значения
		TResult result{ 0 };

		// создаем копию передаваемого списка, дабы не влиять на оригинальный список
		TContainer _digits_list{ digits_list };
		// Текущая цифра, с которой будем сравнивать
		const auto _digit_iter{ std::ranges::next(_digits_list.rbegin(),
								std::ranges::distance(digits_list.rbegin(), digit_iter)) };
		// Сравниваем значения итераторов в зависимости от направления поиска: большего или меньшего числа
		auto _compare = [previous, _digit_iter](const auto _current_iter) -> bool
						{ return (previous) ? *_current_iter > *_digit_iter : *_current_iter < *_digit_iter; };

		// Просматриваем все цифры левее текущей позиции. Используются реверсивные итераторы
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
				if (*_digits_list.begin())
				{
					// сортируем правую (от найденной позиции) часть числа (по возрастанию или по убыванию)
					// с учетом направления поиска. Используются реверсивные итераторы
					std::sort(_digits_list.rbegin(), current_iter,
								[previous](const auto& a, const auto& b) -> bool
									{ return (previous) ? a < b : b < a; } );
					// Собираем из массива цифр результирующее число
					// Дабы избежать применения static_cast, задаем для inumber_from_digits тип возвращаемого значения
					result = assistools::inumber_from_digits<TResult>(_digits_list);
				}
				return result;
			}
		}
		return result;
	};


	/**
	@brief Вспомогательный класс для функции sort_by_shell().
	
	@details Реализует различные методы формирования диапазонов чисел для перестановки.
    Реализованы следующие методы:
    - Классический метод Shell
    - Hibbard
    - Sedgewick
    - Knuth
    - Fibonacci

	@param list_len - длина заданного списка с данными
	@param method - метод формирования списка индексов
	*/
	template <class TIndex>
	class GetIndexes
	{
		// Массив индексов, на которые будет разбит список данных для обработки
		std::vector<TIndex> indexes;

	public:
		GetIndexes(const TIndex& = TIndex(), const sundry::SortMethod = sundry::SortMethod::SHELL);
		// Методы begin() и end() обеспечат поэлементный обход: for(element : list) {}
		// Т.к. алгоритм сортировки требует обход от большего к меньшему, подменяем методы begin()/end() на rbegin()/rend()
		auto begin() const noexcept { return indexes.rbegin(); }
		auto end() const noexcept { return indexes.rend(); }
		auto size() const noexcept { return indexes.size(); }
	};

	template <class TIndex>
	GetIndexes<TIndex>::GetIndexes(const TIndex& list_len, const sundry::SortMethod method)
	{
		// Исходя из заданного метода, вычисляем на какие диапазоны можно разбить исходный список
		switch (method)
		{
		case sundry::SortMethod::HIBBARD:
			// (1 << i) = 2^i
			for (TIndex i{ 1 }, res{ (1 << i) - 1 }; res <= list_len; res = (1 << ++i) - 1)
				indexes.emplace_back(res);
			break;

		case sundry::SortMethod::SEDGEWICK:
			// (1 << i) = 2^i    (n >> 1) = n/2   (i & 1) - четное\нечетное
			auto _sedgewick = [](const TIndex & i) -> TIndex
				{
					return (i & 1) ? 8 * (1 << i) - 6 * (1 << ((i + 1) >> 1)) + 1
						: 9 * ((1 << i) - (1 << (i >> 1))) + 1;
				};

			for (TIndex i{ 0 }, res{ _sedgewick(i) }; res <= list_len; res = _sedgewick(++i))
				indexes.emplace_back(res);
			break;

		case sundry::SortMethod::KNUTH:
			auto _knuth = [](const TIndex& exp) -> TIndex
				{ return (assistools::ipow(static_cast<decltype(exp)>(3), exp) - 1) >> 1; };

			for (TIndex i{ 1 }, res{ _knuth(i) }; res <= (list_len / 3); res = _knuth(++i))
				indexes.emplace_back(res);
			break;

		case sundry::SortMethod::FIBONACCI:
			// В отличии от классической последовательности (1, 1, 2, 3, 5...) реализует (1, 2, 3, 5...)
			for (TIndex prev{ 1 }, next{ 1 }; next <= list_len; prev = std::exchange(next, (next + prev)))
				indexes.emplace_back(next);
			break;

		case sundry::SortMethod::SHELL:
		default:
			for (TIndex res{ list_len }; res >>= 1;)
				indexes.emplace_back(res);
			std::ranges::reverse(indexes);
			break;
		}
	};
}


//****************************************** Public code *************************************************
export namespace sundry
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

	@param number_a, number_b - Заданные целые числа. По умолчанию 0.

	@return (int) Наибольший общий делитель
	*/
	template <typename TNumber = int>
		requires
			std::is_integral_v<TNumber> &&
			std::is_arithmetic_v<TNumber>
	auto get_common_divisor(const TNumber& number_a = TNumber(), const TNumber& number_b = TNumber())
		-> std::optional<TNumber>
	{
		// Определяем делимое и делитель. Делимое - большее число. Делитель - меньшее.
		auto [divisor, divisible] = std::ranges::minmax({ std::abs(number_a), std::abs(number_b) });

		// Вычисляем только если делитель не равен нулю.
		while (divisor)
			divisible = std::exchange(divisor, divisible % divisor);

		return (divisible) ? std::optional<TNumber>{divisible} : std::nullopt;
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

	@param numbers - Массив чисел контейнерного типа
	@param target - Искомое значение

	@return (std::vector<std::pair<int, int>>) Результирующий список диапазонов. Диапазоны задаются в виде
		пары целых чисел, обозначающих индексы элементов списка - начальный и конечный индексы включительно.
		Если ни один диапазон не найден, возвращается пустой список.
	*/
	template <typename TContainer = std::vector<int>>
		requires std::ranges::range<TContainer> && std::is_integral_v<typename TContainer::value_type>
	auto find_intervals(const TContainer& numbers, const typename TContainer::value_type& target)
		-> std::vector<std::pair<typename TContainer::size_type, typename TContainer::size_type>>
	{
		using TIndex = typename TContainer::size_type;
		using TNumber = typename TContainer::value_type;

		struct Ranges
		{
			TNumber sum{ 0 };
			TIndex index{ 0 };
			TNumber target;

			using TResult = std::vector<std::pair<TIndex, TIndex>>;

			TResult result_list;
			std::unordered_multimap<TNumber, TIndex> sum_dict;

			Ranges(TNumber target_) : target{ target_ } { }

			void operator()(const TNumber& number)
			{
				// Суммируем элементы списка по нарастающей
				sum += number;
				// Если на очередной итерации полученная сумма равна искомому значению,
				// заносим диапазон от 0 до текущей позиции в результирующий список.
				if (sum == target)
					result_list.emplace_back(0, index);
				//Ищем пару из уже вычисленных ранее сумм для значения (Sum - Target).
				if (sum_dict.contains(sum - target))
				{
					// Если пара найдена, извлекаем индексы и формируем результирующие диапазоны.
					// У одной и той же суммы возможно несколько индексов
					auto [it_first, it_last] = sum_dict.equal_range(sum - target);
					for (; it_first != it_last; ++it_first)
						result_list.emplace_back(it_first->second + 1, index);
				}
				// Сохраняем очередную сумму и ее индекс в словаре, где ключ - сама сумма.
				sum_dict.emplace(sum, index);
				++index;
			}

			TResult& get_ranges()& { return result_list; } //Оставлено для унификации. В данном алгоритме не используется
			TResult&& get_ranges()&& { return std::move(result_list); }

		};
		// По задумке for_each возвращает временный объект Ranges как rvalue, что приводит к вызову 2-й версии get_ranges().
		// Это позволяет при возврате из функции использовать перемещение результирующего списка,
		// а не его копирование во временный список и возврат этой временной копии.
		return std::for_each(numbers.begin(), numbers.end(), Ranges(target)).get_ranges();
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

	template <typename TContainer = std::vector<int>>
		requires std::ranges::range<TContainer> && (!std::is_array_v<TContainer>)
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
		if (std::is_same_v<T, char> && !*(std::end(elements) - 1))
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

	template <typename TContainer = std::vector<int>>
		requires std::ranges::range<TContainer> &&
				(!std::is_array_v<TContainer>) &&
				std::is_arithmetic_v<typename TContainer::value_type>
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
		requires std::is_array_v<T[N]> && std::is_arithmetic_v<T>
	auto find_item_by_interpolation(const T(&elements)[N], const T& target)
		-> std::make_signed_t<decltype(N)>
	{
		// Перегруженная версия функции для обработки обычных числовых и строковых c-массивов.
		if (std::is_same_v<T, char> && !*(std::end(elements) - 1))
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
	template <typename TNumber >
		requires std::is_integral_v<TNumber> && std::is_arithmetic_v<TNumber>
	auto find_nearest_number(const TNumber& number, bool previous = true)
		-> TNumber
	{
		// Раскладываем заданное число на отдельные цифры
		auto digits_list{ assistools::inumber_to_digits(number) };
		if (digits_list.size() < 2) return 0;
		// Сохраняем знак числа и определяем направление поиска (больше или меньше заданного числа)
		const TNumber sign_number{ (number < 0) ? -1 : 1 };
		const bool is_previous{ (number < 0) ? !previous : previous };
		//Выставляем пороговые значения с учетом направления поиска
		TNumber result{ (is_previous) ? std::numeric_limits<TNumber>::min()
										: std::numeric_limits<TNumber>::max() };
		// цикл перебора цифр заданного числа справа на лево (с хвоста к голове) кроме первой цифры
		for (auto it_digit{ digits_list.rbegin() }; it_digit != std::ranges::prev(digits_list.rend()); ++it_digit)
		{
			// вызываем подпрограмму поиска большего или меньшего числа в зависимости от направления поиска
			// <TNumber> передаем только для того, чтобы получить корректный тип возвращаемого значения
			if (auto res{ _do_find_nearest<TNumber>(digits_list, it_digit, is_previous) })
				result = (is_previous ^ (result < res)) ? result : std::move(res);
		}
		// Если в процессе перебора искомое число было найдено, восстанавливаем исходный знак числа
		return ((result != std::numeric_limits<TNumber>::min()) && (result != std::numeric_limits<TNumber>::max()))
			? result * sign_number : 0;
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
	template <std::bidirectional_iterator TIterator, std::sentinel_for<TIterator> TSIterator>
		requires std::permutable<TIterator>
	void sort_by_bubble(TIterator first, TSIterator last, bool revers = false)
	{
		if (first == last) return;
		// Устанавливаем итераторы на первый и последний элементы данных.
		// Далее работаем с итераторами без прямого использования операторов +/- и []
		// Все это позволяет сортировать практически любые контейнеры
		auto it_start{ first };
		auto it_end{ std::ranges::prev(last) };
		// Флаг, исключающий "пустые" циклы, когда список достигает состояния "отсортирован" на одной из итераций
		bool is_swapped{ false };
		// Сравниваем в зависимости от направления сортировки
		auto _compare = [revers](const auto it_curr, const auto it_next) -> bool
			{ return (revers) ? *it_curr < *it_next : *it_next < *it_curr; };

		while (it_start != it_end)
		{
			// До последнего значения не доходим. Это максимум текущего диапазона.
			for (auto it_current{ it_start }, it_next{ std::ranges::next(it_current) };
				it_current != it_end; ++it_current, ++it_next)
			{
				// Если текущий элемент больше следующего, то переставляем их местами.Это потенциальный максимум.
				if (_compare(it_current, it_next))
				{
					std::iter_swap(it_current, it_next);
					//Одновременно проверяем на потенциальный минимум, сравнивая с первым элементом текущего диапазона.
					if (it_current != it_start && _compare(it_start, it_current))
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
				// Если за итерацию перестановок не было, то список уже отсортирован. Досрочно выходим из цикла
				it_start = it_end;
		}
	};

	// Перегруженный вариант
	template <typename TContainer>
		requires std::ranges::range<TContainer>
	void sort_by_bubble(TContainer&& data, bool revers = false)
	{
		sort_by_bubble(std::ranges::begin(data), std::ranges::end(data), std::move(revers));
	};

	/**
	@brief Функция сортировки методом слияния. Сортирует заданный список по месту.

	@details В отличии от классического метода не использует рекурсивные вызовы и не создает каскад списков.
	Вместо этого создается список индексов для диапазонов сортировки, по которым происходит отбор
	значений из исходного списка и их сортировка.

	@param first - Итератор, указывающий на первый элемент данных.
	@param last - Итератор, указывающий за последний элемент данных.
	@param revers (bool) - Если True, то сортировка по убыванию. Defaults to False.
	*/
	template <std::bidirectional_iterator TIterator, std::sentinel_for<TIterator> TSIterator>
		requires std::permutable<TIterator>
	void sort_by_merge(TIterator first, TSIterator last, bool revers = false)
	{
		// Получаем размер массива данных
		auto _size{ std::ranges::distance(first, last) };
		if (_size < 2) return;

		using TIndex = decltype(_size);
		using TItem = std::iter_value_t<TIterator>;
		using TTuple = std::tuple<TIndex, TIndex, TIndex>;

		// Итоговый список индексов, по которым сортируется исходный список данных
		std::stack<TTuple> query_work;

		{ // Ограничиваем область действия query_buff
			// Временный буфер для деления диапазонов индексов пополам
			std::queue<TTuple> query_buff;
			// Инициализируем буферную очередь исходным списком, деленным пополам
			query_buff.emplace(0, _size >> 1, _size);
			// Далее делим пополам обе половины до тех пор, пока в каждой половине не останется по одному элементу
			while (!query_buff.empty())
			{
				auto [i_first, i_middle, i_last] = std::move(query_buff.front());
				query_buff.pop();
				// Делим пополам левую часть
				if (auto i_offset{ (i_middle - i_first) >> 1 })
					query_buff.emplace(i_first, (i_first + i_offset), i_middle);
				// Делим пополам правую часть
				if (auto i_offset{ (i_last - i_middle) >> 1 })
					query_buff.emplace(i_middle, (i_middle + i_offset), i_last);
				// Результирующий список индексов будет содержать индексы диапазонов для каждой половины
				query_work.emplace(i_first, i_middle, i_last);
			}
		}

		// При сравнении учитываем порядок сортировки
		auto _compare = [revers](const auto& iter_a, const auto& iter_b) -> bool
			{ return revers ? *iter_b < *iter_a : *iter_a < *iter_b; };
		// Сортируем все полученные половины
		while (!query_work.empty())
		{
			// Выбираем из очереди диапазоны, начиная с меньших
			auto [i_first, i_middle, i_last] = std::move(query_work.top());
			query_work.pop();

			// Итерируем по половинчатым спискам и по исходному списку, сортируя его
			auto it_current{ std::ranges::next(first, i_first) };
			auto it_right{ std::ranges::next(first, i_middle) };
			auto it_right_end{ std::ranges::next(first, i_last) };
			// Формируем временный список с данными только  для левой половины. Правая в исходном списке
			std::vector<TItem> left_list{ it_current, it_right };
			auto it_left{ left_list.begin() };
			auto it_left_end{ left_list.end() }; //Не обязательно. Чисто для удобства.

			// Поэлементно сравниваем половины и сортируем исходный список
			while ((it_left != it_left_end) && (it_right != it_right_end))
				(_compare(it_left, it_right))
				? *it_current++ = *it_left++
				: *it_current++ = *it_right++;
			// Добавляем в результирующий список "хвост" только от левой части.
			// Правая уже присутствует в исходном списке
			if (it_left != it_left_end)
				std::ranges::move(it_left, it_left_end, it_current);
		}
	};

	// Перегруженный вариант
	template <typename TContainer>
		requires std::ranges::range<TContainer>
	void sort_by_merge(TContainer&& data, bool revers = false)
	{
		sort_by_merge(std::ranges::begin(data), std::ranges::end(data), std::move(revers));
	};

	/**
	@brief Функция сортировки методом Shell. Заданный список сортируется по месту.

	@details Кроме классического метода формирования списки индексов для перестановки,
	возможно использовать следующие методы:
	- Hibbard
	- Sedgewick
	- Knuth
	- Fibonacci

	Реализована двунаправленная сортировка.

	@param first - Итератор, указывающий на первый элемент данных.
	@param last - Итератор, указывающий за последний элемент данных.
	@param method - Метод формирования списка индексов для перестановки.
	@param revers (bool) - Если True, то сортировка по убыванию. Defaults to False.
	*/
	template <std::bidirectional_iterator TIterator, std::sentinel_for<TIterator> TSIterator>
		requires std::permutable<TIterator>
	void sort_by_shell(TIterator first, TSIterator last, SortMethod method = SortMethod::SHELL, bool revers = false)
	{
		// Получаем размер исходного массива данных
		auto _size{ std::ranges::distance(first, last) };
		if (_size < 2) return;
		// Учитываем направление сортировки
		auto _compare = [&revers](const auto& it_left, const auto& it_right) -> bool
			{ return (revers) ? *it_left < *it_right : *it_right < *it_left; };
		// Вспомогательный класс GetIndexes формирует список индексов исходя из размера исходных данных и заданного метода
		// Полученные индексы обрабатываем от большего к меньшему. Класс GetIndexes сам обеспечит нужный порядок перебора.
		for (const auto& i_index : GetIndexes{ _size, method })
			// Разбиваем исходные данные на диапазоны согласно индексу
			for (auto i_range{ i_index }; i_range < _size; ++i_range)
				// Сортируем список в заданном диапазоне
				for (auto i_current{ i_range }; i_current >= i_index; i_current -= i_index)
				{
					auto it_left{ std::ranges::next(first, i_current - i_index) };
					auto it_right{ std::ranges::next(first, i_current) };
					if (_compare(it_left, it_right)) std::iter_swap(it_left, it_right);
					else break; // Если перестановок больше не требуется, досрочно выходим из цикла
				}
	};

	// Перегруженный вариант
	template <typename TContainer>
		requires std::ranges::range<TContainer>
	void sort_by_shell(TContainer&& data, SortMethod method = SortMethod::SHELL, bool revers = false)
	{
		sort_by_shell(std::ranges::begin(data), std::ranges::end(data), std::move(method), std::move(revers));
	};

	/**
	@brief Функция сортировки методом отбора. Заданный список сортируется по месту.

	@details Это улучшенный вариант пузырьковой сортировки за счет сокращения числа
	перестановок элементов. Элементы переставляются не на каждом шаге итерации,
	а только лишь в конце текущей итерации. Дополнительно к классическому алгоритму
	добавлена возможность одновременного поиска максимального и минимального элементов
	текущего диапазона за одну итерацию. Реализована двунаправленная сортировка.

	@param first - Итератор, указывающий на первый элемент данных.
	@param last - Итератор, указывающий за последний элемент данных.
	@param revers (bool) - Если True, то сортировка по убыванию. Defaults to False.
	*/
	template <std::bidirectional_iterator TIterator, std::sentinel_for<TIterator> TSIterator>
		requires std::permutable<TIterator>
	void sort_by_selection(TIterator first, TSIterator last, bool revers = false)
	{
		if (first == last) return;
		// В качестве стартового диапазона начальный и конечные элементы исходного списка
		auto it_start{ first };
		auto it_end{ std::ranges::prev(last) };
		// Потенциальные минимум и максимум в начале и конце диапазона
		auto it_min{ it_start };
		auto it_max{ it_end };
		//Флаг, исключающий "пустые" циклы, когда список достигает состояния "отсортирован" на одной из итераций
		bool is_swapped{ false };

		auto _compare = [revers](const auto it_prev, const auto it_next) -> bool
			{ return (revers) ? *it_prev < *it_next : *it_next < *it_prev; };
		// Перебираем диапазоны, сокращая длину каждого следующего диапазона на 2
		while (it_start != it_end)
		{
			/* Т.к.до последнего элемента не доходим, необходимо перед итерацией
			   сравнить последний элемент с первым.Возможно последний элемент
			   потенциальный минимум текущего диапазона, тогда меняем местами
			*/
			if (_compare(it_start, it_end)) std::iter_swap(it_start, it_end);
			// Перебираем элементы текущего диапазона
			for (auto it_current{ it_start }; it_current != it_end; ++it_current)
			{
				// Если текущий элемент больше последнего в диапазоне, то это потенциальный максимум
				if (_compare(it_current, it_max)) { it_max = it_current; is_swapped = true; }
				// Одновременно проверяем на потенциальный минимум, сравнивая с первым элементом текущего диапазона
				else if (_compare(it_min, it_current)) { it_min = it_current; is_swapped = true; }
				// Выясняем, потребуется ли перестановка на следующей итерации
				else if (!is_swapped && _compare(it_current, std::ranges::next(it_current))) is_swapped = true;
			}

			if (is_swapped)
			{
				// Если найдены потенциальные минимум и/или максимум, выполняем перестановки
				// элементов с начальным и/или конечным элементом текущего диапазона.
				if (it_max != it_end) std::iter_swap(it_max, it_end);
				if (it_min != it_start) std::iter_swap(it_min, it_start);
				// После каждой итерации по элементам списка, сокращаем длину проверяемого диапазона на 2,
				// т.к.на предыдущей итерации найдены одновременно минимум и максимум
				std::advance(it_start, 1);
				if (it_start != it_end)
					std::advance(it_end, -1);
				it_min = it_start;
				it_max = it_end;
				is_swapped = false;
			}
			else
				// Если за итерацию перестановок не потребовалось, то список уже отсортирован. Досрочно выходим из цикла
				it_start = it_end;
		}
	};

	// Перегруженный вариант
	template <typename TContainer>
		requires std::ranges::range<TContainer>
	void sort_by_selection(TContainer&& data, bool revers = false)
	{
		sort_by_selection(std::ranges::begin(data), std::ranges::end(data), std::move(revers));
	};
}
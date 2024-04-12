module;
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <unordered_map>
#include <future>
#include <semaphore>
#include <thread>
#include  <execution>
export module Demo:Puzzles;

import :Assistools;

//****************************************** Private code *************************************************
namespace
{
	/**
	@brief Вспомогательный класс для функции get_combination_numbers_async().

	@details Получает от вызывающей функции наборы цифр, из которых формирует комбинации чисел.
	При этом как формирование комбинаций, так и получение результатов происходит в асинхронном режиме.

	@param poolsize: Для конструктора можно задать размер пула одновременно исполняемых задач. По умолчанию равен количеству ядер CPU.

	@param policy: Для методов add_digits и add_number можно указать политику запуска всех или конкретной асинхронной задачи.

	@return Список всех возможных комбинаций.
	*/
	template <class TNumber = int>
	class GetCombinations
	{
		using TDigits = std::vector<TNumber>;
		using TResult = std::vector<TDigits>;
		using TFuture = std::shared_future<TResult>;
		using TPoolSize = std::ptrdiff_t; // Тип ptrdiff_t выбран по аналогии с конструктором std::counting_semaphore
		const TPoolSize _DEFAULT_POOL_SIZE_{ 4 };

		// Итоговый список, в котором будем собирать результаты от запланированных задач
		TResult result_list{};
		// Список задач в виде фьючерсов. Фьючерсы должны быть shared_future, ибо копии понадобятся для
		// переупаковки и запуска "ленивых" задач. По сути это очередь задач.
		// Тип контейнера выбран list, т.к. планируются частые вставки и удаления по всему списку.
		std::list<TFuture> task_list{};
		// Пул задач для ограничения максимального количества одновременно запущенных задач.
		// По умолчанию устанавливается равным числу ядер процессора
		std::unique_ptr<std::counting_semaphore<>> task_pool = nullptr;
		// Флаг досрочного завершения задач
		std::atomic_flag stop_all_task = ATOMIC_FLAG_INIT;

		/*
		Функция, которая запускается как отдельная асинхронная задача.
		Получаем копию параметра digits, т.к. вызывающая функция будет менять набор цифр
		не дожидаясь пока асинхронная задача отработает. Для асинхронной задачи необходима автономность данных.
		*/
		auto make_combination(TDigits digits) -> TResult
		{
			// На всякий случай проверка пороговых значений. Это может выглядеть излишней перестраховкой,
			// т.к. вызывающая функция get_combination_numbers_async предварительно делает необходимые проверки.
			if (digits.empty() || stop_all_task.test(std::memory_order_relaxed))
				return TResult{};

			// Получаем из пула задач разрешение на запуск. Если пул полон, ждем своей очереди
			task_pool->acquire();

			// Каждая задача собирает свою автономную часть результатов, которые далее в цикле
			// асинхронного получения результатов соберутся в итоговый список
			TResult result_buff{};

			// Добавляем комбинацию из одиночных цифр
			result_buff.emplace_back(digits);
			// Сразу же добавляем число из всего набора цифр
			if ((digits.size() > 1) && (*digits.begin() != 0))
				result_buff.push_back({ assistools::inumber_from_digits(digits) });

			/*
			Кроме одиночных, формируем комбинации с двух - , трех - ... N - числами.
			Максимальный N равен размеру заданного списка одиночных цифр минус 1,
			т.к. число из полного набора цифр уже сформировано.
			Заканчиваем двухзначными, т.к. комбинация из одиночных цифр уже сформирована
			*/
			for (auto _size{ digits.size() - 1 }; (1 < _size) && !stop_all_task.test(std::memory_order_relaxed); --_size)
			{
				// Формируем окно выборки цифр для формирования двух-, трех- и т.д. чисел
				auto it_first_digit{ digits.begin() };
				auto it_last_digit{ std::ranges::next(it_first_digit, _size) };
				TDigits _buff;
				_buff.reserve(_size);
				while ((it_first_digit != it_last_digit) && !stop_all_task.test(std::memory_order_relaxed))
				{
					// Числа, начинающиеся с 0, пропускаем
					if (*it_first_digit != 0)
					{
						// Комбинируем полученное число с цифрами оставшимися вне окна выборки
						_buff.assign(digits.begin(), it_first_digit); // Цифры слева
						// Формируем число из цифр, отобранных окном выборки
						_buff.emplace_back(assistools::inumber_from_digits(it_first_digit, it_last_digit));
						_buff.insert(_buff.end(), it_last_digit, digits.end()); // Цифры справа
						result_buff.emplace_back(_buff);
					}

					if (it_last_digit != digits.end())
					{
						//Смещаем окно выборки
						++it_first_digit;
						++it_last_digit;
					}
					// Иначе, если окно выборки достигло конца списка цифр, выходим из цикла
					else it_first_digit = it_last_digit;
				}
			}
			// Освобождаем место в очереди пула задач
			task_pool->release();
			// Это еще не окончательный результат, а лишь промежуточный для конкретной асинхронной задачи
			return result_buff;
		}

	public:
		explicit GetCombinations(TPoolSize poolsize = 0) noexcept
		{
			auto _pool_size = (poolsize > 0) ? std::move(poolsize) : std::thread::hardware_concurrency();
			// На случай, если std::thread::hardware_concurrency() не сработает
			if (_pool_size == 0) _pool_size = _DEFAULT_POOL_SIZE_;
			//Инициализируем пул задач в виде счетчика-семафора
			task_pool = std::make_unique<std::counting_semaphore<>>(_pool_size);
		}

		~GetCombinations()
		{
			// Если остались незавершенные задачи, ожидаем...
			if (!task_list.empty())
			{
				stop_all_task.test_and_set(std::memory_order_relaxed);
				
				std::for_each(std::execution::par,
					task_list.cbegin(),
					task_list.cend(),
					[](auto t) { if (t.valid()) t.wait(); });

				task_list.clear();
			}
		}

		// Копирование запрещаем, т.к. пул асинхронных задач должен выполняться в рамках единственного экземпляра класса.
		GetCombinations(const GetCombinations&) = delete;
		GetCombinations& operator=(const GetCombinations&) = delete;

		void add_digits(TDigits digits, std::launch policy = std::launch::async)
		{
			// Планируем на выполнение асинхронные задачи. При этом задачи могут быть "ленивыми".
			// Зависит от заданной политики. Допускаются комбинации из асинхронных и "ленивых" задач
			task_list.emplace_back(std::async(std::move(policy), &GetCombinations::make_combination, this, std::move(digits)));
		}

		void add_digits(TNumber number, std::launch policy = std::launch::async)
		{
			// Числа распарсиваются в цифры и из них формируется асинхронная задача
			GetCombinations::add_digits(assistools::inumber_to_digits(std::move(number)), std::move(policy));
		}

		auto get_combinations() -> decltype(result_list)
		{
			/*
			По запросу из вызывающей функции начинаем собирать итоговый результат в асинхронном режиме.
			Т.е. если какая-либо задача из списка еще не отработала и не может предоставить результат своей работы,
			не ждем ее завершения, а переходим к следующей задаче из списка и проверяем ее готовность.
			При готовности задачи, добавляем результат ее работы в итоговый результат и удаляем задачу из списка.
			Повторяем цикл проверки готовности задач пока список задач не опустеет.
			P.S. Если встречается "ленивая" задача, то ее следует запустить, но при этом не ждать пока она отработает.
			Для этого переупаковываем "ленивые" задачи в асинхронные и добавляем в список задач. Старую "ленивую" задачу
			из списка удаляем. Делается это через копирование фьючерса "ленивой" задачи и вызова его метода get в рамках
			асинхронной задачи, что приводит к запуску "ленивой" задачи, но уже в качестве асинхронной
			*/

			//Запускаем цикл асинхронного получения результатов пока в очереди есть запланированные задачи
			while (!task_list.empty())
			{
				//Просматриваем список запланированных задач. При этом размер списка может динамически меняться.
				for (auto it_future = task_list.begin(); it_future != task_list.end();)
				{
					if ((*it_future).valid())
					{
						// Метод wait_for нужен только для получения статусов, потому вызываем с нулевой задержкой.
						//Для каждой задачи отрабатываем два статуса: ready и deferred
						switch ((*it_future).wait_for(std::chrono::milliseconds(0)))
						{
						case std::future_status::ready: //Задача отработала и результат готов
							//Перемещаем полученный от задачи результат в итоговый список
							std::ranges::move((*it_future).get(), std::back_inserter(result_list));
							//Удаляем задачу из списка обработки. Итератор сам сместиться на следующую задачу.
							it_future = task_list.erase(it_future);
							break;
						case std::future_status::deferred: //Это "ленивая" задача. Ее нужно запустить
							//Переупаковываем "ленивую" задачу в асинхронную и запускаем, добавив в список как новую задачу.
							task_list.emplace_back(std::async(std::launch::async, [lazy_future = (*it_future)] { return lazy_future.get(); }));
							//Удаляем "ленивую" задачу из списка задач
							it_future = task_list.erase(it_future);
							break;
						case std::future_status::timeout:
						default:
							// Если задача еще не отработала и результат не готов, пропускаем и переходим к следующей.
							// При этом задача остается в списке и будет еще раз проверена на готовность в следующем цикле
							++it_future;
							break;
						}
					}
					else
						//Если фьючерс не валиден, удаляем соответствующую ему задачу из списка без обработки
						it_future = task_list.erase(it_future);
				}
			}
			// Сортируем для удобства восприятия (не обязательно).
			std::ranges::sort(result_list);
			return result_list;
		}
	};
}

//****************************************** Public code *************************************************
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
	requires std::is_integral_v<typename std::remove_cvref_t<TContainer>::value_type>
	auto get_combination_numbers(TContainer&& digits)
		-> std::vector< std::vector<typename std::remove_cvref_t<TContainer>::value_type> >
	{
		//Приходится очищать TContainer от ссылки, т.к. lvalue параметр передается как ссылка (lvalue&)
		using TNumber = typename std::remove_cvref_t<TContainer>::value_type;
		using TDigits = std::vector<TNumber>;

		// Сохраняем копию исходного списка для формирования комбинаций перестановок
		//TDigits _digits{ std::forward<TContainer>(digits) }; // Идеальный вариант, но ограничен только vector
		// Т.к. неизвестно какой тип контейнера будет передан, используем максимально обобщенный вариант инициализации
		TDigits _digits{ std::ranges::begin(digits),
						std::ranges::end(digits) };
		auto _size = _digits.size();
		// Результат - список списков
		std::vector<TDigits> result;

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
				TDigits _buff;
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
						result.emplace_back(std::move(_buff));
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

	template <typename TNumber = int>
	requires std::is_integral_v<TNumber>
	auto get_combination_numbers(TNumber number = TNumber())
		-> std::vector< std::vector<TNumber> >
	{
		return get_combination_numbers(assistools::inumber_to_digits(std::move(number)));
	};

	/**
	@brief Асинхронная версия функции get_combination_numbers().

	@details Для данной задачи асинхронность смысла не имеет из-за значительных
	затрат на создание потоков в сравнении с синхронной версией. Асинхронность
	выполнена в качестве учебных целей и с прицелом на создание универсального класса пула задач.

	@param digits - Список заданных цифр

	@return Список уникальных комбинаций
	*/
	template <typename TContainer = std::vector<int>>
		requires std::is_integral_v<typename std::remove_cvref_t<TContainer>::value_type>
	auto get_combination_numbers_async(TContainer&& digits)
		-> std::vector< std::vector<typename std::remove_cvref_t<TContainer>::value_type> >
	{
		//Приходится очищать TContainer от ссылки, т.к. lvalue параметр передается как ссылка (lvalue&)
		using TNumber = typename std::remove_cvref_t<TContainer>::value_type;
		using TDigits = std::vector<TNumber>;

		// Сохраняем копию исходного списка для формирования комбинаций перестановок
		//TDigits _digits{ std::forward<TContainer>(digits) }; // Идеальный вариант, но ограничен только vector
		// Т.к. неизвестно какой тип контейнера будет передан, используем максимально обобщенный вариант инициализации
		TDigits _digits{ std::ranges::begin(digits),
						 std::ranges::end(digits) };

		switch (_digits.size())
		{
		case 1:
			return std::vector<TDigits>{_digits};
		case 0:
			return std::vector<TDigits>{};
		}
		// Класс для запуска асинхронных задач
		GetCombinations<TNumber> combination_numbers;
		// Формируем следующую комбинацию из одиночных цифр
		// Каждая комбинация передается в класс GetCombinations, который для их обработки
		// запускает отдельную асинхронную задачу
		do combination_numbers.add_digits(_digits);
		while (std::next_permutation(_digits.begin(), _digits.end()));
		// Запрашиваем у класса GetCombinations дождаться завершения асинхронных задач,
		// получить результаты их работы и собрать итоговый результирующий список перестановок
		return combination_numbers.get_combinations();
	};

} 
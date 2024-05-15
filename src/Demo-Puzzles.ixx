module;
#include <algorithm>
#include <execution>
#include <future>
#include <iterator>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <unordered_map>
export module Demo:Puzzles;

import :Assistools;

//****************************************** Private code *************************************************
namespace
{
	/**
	@brief Вспомогательный класс для функции get_combination_numbers_async().

	@details Получает от вызывающей функции наборы цифр, из которых формирует комбинации чисел.
	При этом как формирование комбинаций, так и получение результатов происходит в асинхронном режиме.

	@param pool_size: Для конструктора можно задать размер пула одновременно исполняемых задач.
	По умолчанию равен количеству ядер CPU.

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

		// Следует ли накапливать результат после запроса на получение полного результата или очищать буфер 
		const bool accumulate_result_flag{ true };
		// Итоговый список, в котором будем собирать результаты от запланированных асинхронных задач
		TResult result_list{};
		std::mutex result_list_mutex;
		// Список задач в виде фьючерсов. Фьючерсы должны быть shared_future, ибо копии понадобятся для
		// переупаковки и запуска "ленивых" задач. По сути это очередь задач.
		// Тип контейнера выбран list, т.к. планируются частые вставки и удаления по всему списку.
		std::list<TFuture> task_list{};
		std::mutex task_list_mutex;
		// Пул задач для ограничения максимального количества одновременно запущенных задач.
		// По умолчанию устанавливается равным числу ядер процессора
		std::unique_ptr<std::counting_semaphore<>> task_pool{ nullptr };
		//std::counting_semaphore<> task_pool2;
		// Отдельный поток для фоновой сборки результатов завершенных асинхронных задач
		std::jthread get_result_thread;
		// Ожидание готовности результатов для фонового процесса сборки результата
		std::atomic_flag result_ready_flag = ATOMIC_FLAG_INIT;
		// Счетчик завершенных задач для пробуждения get_result_thread
		// Используем счетчик, а не флаг или проверку на пустой список, т.к. присутствуют "ленивые" задачи,
		// которые хранятся в списке до момента их запуска при получении полного результата
		std::atomic_uint done_task_count{ 0 };
		// Флаг для запроса формирования полного результата
		std::atomic_flag request_full_result_flag = ATOMIC_FLAG_INIT;
		// Запрос на остановку для фонового потока предварительной сборки результата
		std::stop_source stop_get_result_thread;
		// Запрос на остановку всех запущенных асинхронных задач
		std::stop_source stop_running_task_thread;

		bool is_stop_requested()
		{
			return stop_running_task_thread.stop_requested();
		}

		void stop_running_task()
		{
			// Отправляем всем задачам запрос на останов
			stop_running_task_thread.request_stop();

			std::lock_guard<decltype(task_list_mutex)> lock_task_list(task_list_mutex);
			if (!task_list.empty())
			{
				std::for_each(std::execution::par,
					task_list.cbegin(),
					task_list.cend(),
					[](auto& t) { if (t.valid()) t.wait(); });

				task_list.clear();
				done_task_count.store(0);
			}
		}

		void stop_get_result_task()
		{
			// Оправляем в поток запрос на останов
			stop_get_result_thread.request_stop();
			// Если фоновая задача предварительной сборки результатов работает, ждем ее завершения
			if (get_result_thread.joinable()) get_result_thread.join();
		}

		void notify_get_result_task()
		{
			result_ready_flag.test_and_set();
			result_ready_flag.notify_one();
		}

		/*
		Фоновая задача, которая предварительно собирает результаты завершенных асинхронных задач
		по мере их готовности до момента вызова get_combinations.
		По отдельному запросу собирает окончательный полный результат.
		*/
		void get_result_task(std::stop_token stop_task)
		{
			TResult result_list_buff{ }; // Список для частичной сборки результата дабы уменьшить число блокировок result_list
			// При получении запроса на останов, поток сам себя пробуждает
			std::stop_callback stop_task_callback(stop_task, [this]
				{
					if (!result_ready_flag.test()) this->notify_get_result_task();
				});
			// Если поступил запрос на остановку, завершаем фоновый процесс предварительной сборки результатов
			while (!stop_task.stop_requested())
			{
				// Ожидаем, если нет уведомления о готовности результатов из асинхронных задач make_combination_task,
				// нет уведомления об остановке потока и нет запроса на на формирование полного результата
				if ((done_task_count.load() == 0)
					&& !stop_task.stop_requested()
					&& !request_full_result_flag.test()) result_ready_flag.wait(false);
				// Выясняем причину пробуждения
				if (stop_task.stop_requested())
					// Если причина пробуждения запрос на останов задачи, выходим из цикла
					break;
				else if (request_full_result_flag.test()) // Поступил запрос на формирование полного результата
				{
					// Собираем результат при полной блокировке. Это не позволит новым задачам исказить результат
					std::unique_lock<decltype(task_list_mutex)> lock_task_list(task_list_mutex);
					while (!task_list.empty() && !stop_task.stop_requested())
					{
						//Просматриваем список запланированных задач. При этом размер списка может динамически меняться.
						for (auto it_future{ task_list.begin() }; (it_future != task_list.end())
							&& !stop_task.stop_requested();) // Если поступил запрос на останов, досрочно выходим
						{
							if (auto future_task{ *it_future }; future_task.valid()) [[likely]]
							{
								// Метод wait_for нужен только для получения статусов, потому вызываем с нулевой задержкой.
								//Для каждой задачи отрабатываем два статуса: ready и deferred
								switch (future_task.wait_for(std::chrono::milliseconds(0)))
								{
								case std::future_status::ready: //Задача отработала и результат готов
									//Перемещаем полученный от задачи результат в буферный список
									std::ranges::move(future_task.get(), std::back_inserter(result_list_buff));
									//Удаляем задачу из списка обработки. Итератор сам сместиться на следующую задачу.
									it_future = task_list.erase(it_future);
									done_task_count.fetch_sub(1);
									break;
								case std::future_status::deferred: //Это "ленивая" задача. Ее нужно запустить
									//Переупаковываем "ленивую" задачу в асинхронную и запускаем, добавив в список как новую задачу.
									task_list.emplace_back(std::async(std::launch::async, [lazy_future = std::move(future_task)] { return lazy_future.get(); }));
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
					lock_task_list.unlock(); //Далее блокировка не нужна
					
					if (!result_list_buff.empty() && !stop_task.stop_requested())
					{
						// Загружаем в итоговый список под блокировкой
						std::lock_guard<decltype(result_list_mutex)> lock_result_list(result_list_mutex);
						if (accumulate_result_flag)
						{
							// Догружаем в итоговый result_list все, что осталось после частичных выгрузок
							if (auto _res_size{ result_list.size() }, _buff_size{ result_list_buff.size() }; _res_size < _buff_size)
							{
								result_list.reserve(_buff_size);
								auto it_last_loaded_result{ std::ranges::next(result_list_buff.begin(), _res_size) };
								result_list.insert(result_list.end(), it_last_loaded_result, result_list_buff.end());
							}
						}
						else //Если аккумулировать результат не нужно, выгружаем и очищаем буфер
						{
							result_list.reserve(result_list_buff.size());
							result_list = std::move(result_list_buff);
						}
					}
					
					request_full_result_flag.clear(); //Сбрасываем флаг требования получения полного результата
					request_full_result_flag.notify_one(); //Уведомляем ожидающий поток о готовности полного результата
				}
				else if (done_task_count.load() > 0) // Одна из задач готова вернуть результат
				{
					// После получения уведомления о готовности от какой-либо из задач,
					// просматриваем список task_list в поиске задач с готовыми результатами.
					std::lock_guard<decltype(task_list_mutex)> lock_task_list(task_list_mutex);
					for (auto it_future{ task_list.begin() }; (it_future != task_list.end())
						&& !stop_task.stop_requested() // Если поступил запрос на останов, досрочно выходим
						&& done_task_count.load() > 0;) // Если счетчик завершенных задач обнулился, досрочно выходим
					{
						if (auto future_task{ *it_future }; future_task.valid()) [[likely]]
						{
							if (future_task.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
							{
								//Перемещаем полученный от задачи результат в буферный список
								std::ranges::move(future_task.get(), std::back_inserter(result_list_buff));
								//Удаляем задачу из списка обработки. Итератор сам сместиться на следующую задачу.
								it_future = task_list.erase(it_future);
								done_task_count.fetch_sub(1);
							}
							else
								++it_future;
						}
						else
							it_future = task_list.erase(it_future);
					}
				}
				// Сбрасываем флаг, чтобы снова заснуть
				result_ready_flag.clear();
			}
		}

		/*
		Функция, которая запускается как отдельная асинхронная задача.
		Получаем копию параметра digits, т.к. вызывающая функция будет менять набор цифр
		не дожидаясь пока асинхронная задача отработает. Для асинхронной задачи необходима автономность данных.
		*/
		auto make_combination_task(TDigits digits, std::stop_token stop_task) -> TResult
		{
			// На всякий случай проверка пороговых значений. Это может выглядеть излишней перестраховкой,
			// т.к. вызывающая функция get_combination_numbers_async предварительно делает необходимые проверки.
			if (digits.empty() || stop_task.stop_requested())
				return TResult{};

			// Получаем из пула задач разрешение на запуск. Если пул полон, ждем своей очереди
			task_pool->acquire();

			// Каждая задача собирает свою автономную часть результатов, которые далее в цикле
			// асинхронного получения результатов соберутся в итоговый список
			TResult result_buff{};

			// Добавляем комбинацию из одиночных цифр
			result_buff.emplace_back(digits);
			// Сразу же добавляем число из всего набора цифр
			if ((digits.size() > 1) && (*digits.begin() != 0) && !stop_task.stop_requested())
				result_buff.push_back({ assistools::inumber_from_digits(digits) });

			/*
			Кроме одиночных, формируем комбинации с двух - , трех - ... N - числами.
			Максимальный N равен размеру заданного списка одиночных цифр минус 1,
			т.к. число из полного набора цифр уже сформировано.
			Заканчиваем двухзначными, т.к. комбинация из одиночных цифр уже сформирована
			*/
			for (auto _size{ digits.size() - 1 }; (1 < _size) && !stop_task.stop_requested(); --_size)
			{
				// Формируем окно выборки цифр для формирования двух-, трех- и т.д. чисел
				auto it_first_digit{ digits.begin() };
				auto it_last_digit{ std::ranges::next(it_first_digit, _size) };
				TDigits _buff;
				_buff.reserve(_size);
				while ((it_first_digit != it_last_digit) && !stop_task.stop_requested())
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
			// Отправляем уведомление сборщику результатов get_result_task о готовности результата текущей задачи
			done_task_count.fetch_add(1);
			this->notify_get_result_task();
			// Это еще не окончательный результат, а лишь промежуточный для конкретной асинхронной задачи
			return result_buff;
		}

		void init_pool_size(TPoolSize pool_size)
		{
			auto _pool_size = (pool_size > 0) ? std::move(pool_size) : std::thread::hardware_concurrency();
			//Инициализируем пул задач в виде счетчика-семафора
			task_pool = (_pool_size) ? std::make_unique<std::counting_semaphore<>>(_pool_size)
				// На случай, если std::thread::hardware_concurrency() не сработает
				: std::make_unique<std::counting_semaphore<>>(_DEFAULT_POOL_SIZE_);
			// Запускаем фоновую задачу предварительной сборки результатов
			get_result_thread = std::jthread(&GetCombinations::get_result_task, this, stop_get_result_thread.get_token());
		}

	public:
		explicit GetCombinations(TPoolSize pool_size = 0, bool accumulate_result = true) : accumulate_result_flag(accumulate_result)
		{
			this->init_pool_size(std::move(pool_size));
		}

		explicit GetCombinations(bool accumulate_result) : accumulate_result_flag(accumulate_result)
		{
			this->init_pool_size(0);
		}

		~GetCombinations()
		{
			// Останавливаем все запущенные задачи и предотвращаем запуск новых
			this->stop_running_task();
			// Останавливаем фоновую задачу предварительной сборки результатов
			this->stop_get_result_task();
		}

		// Копирование запрещаем, т.к. пул асинхронных задач должен выполняться в рамках единственного экземпляра класса.
		GetCombinations(const GetCombinations&) = delete;
		GetCombinations& operator=(const GetCombinations&) = delete;

		void add_digits(TDigits digits, std::launch policy = std::launch::async)
		{
			// Планируем на выполнение асинхронные задачи. При этом задачи могут быть "ленивыми".
			// Зависит от заданной политики. Допускаются комбинации из асинхронных и "ленивых" задач
			// Новые задачи запускаются только если не поступил запрос об остановке
			if (!this->is_stop_requested())
			{
				// Т.к. запуск асинхронной задачи процесс длительный, выполняем его вне блокировки
				auto task_future{ std::async(policy, &GetCombinations::make_combination_task, this, std::move(digits), stop_running_task_thread.get_token()) };
				// Блокируем доступ к task_list, т.к. уже запущен поток get_result_task, который тоже имеет доступ к task_list
				std::lock_guard<decltype(task_list_mutex)> lock_task_list(task_list_mutex);
				task_list.emplace_back(std::move(task_future));
			}
		}

		void add_digits(TNumber number, std::launch policy = std::launch::async)
		{
			// Числа распарсиваются в цифры и из них формируется асинхронная задача
			this->add_digits(assistools::inumber_to_digits(number), policy);
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
			request_full_result_flag.test_and_set(); // Требуем собрать полный результат
			this->notify_get_result_task(); // Отправляем требование в фоновую задачу get_result_task
			request_full_result_flag.wait(true); // Ждем готовности полного результата
			TResult return_result{}; //Временный список для возврата результата
			// Если аккумулировать результат не требуется, перемещаем result_list, что приведет к обнулению списка
			// Делаем это под блокировкой, т.к. поток get_result_task также имеет доступ к result_list
			std::unique_lock<decltype(result_list_mutex)> lock_result_list(result_list_mutex);
			return_result.reserve(result_list.size());
			return_result = (accumulate_result_flag) ? result_list : std::move(result_list);
			lock_result_list.unlock(); //Отпускаем блокировку
			// Сортируем для удобства восприятия (не обязательно).
			std::ranges::sort(return_result);
			return return_result;			
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
							TContainer digits) // Умышленно получаем копию списка
		-> typename TContainer::value_type
	{
		// Отрабатываем некорректные параметры
		if (count <= 0 || pages < count || digits.size() == 0) return 0;
		// Индекс может быть отрицательным
		using TIndex = std::make_signed_t<typename TContainer::size_type>;
		using TValue = typename TContainer::value_type;

		auto get_near_number = [pages](const auto& last_digit) -> TValue
			{
				// На всякий случай отбрасываем минус и от многозначных чисел оставляем последнюю цифру
				TValue _last_digit{ ((last_digit < 0) ? -last_digit : last_digit) % 10 };
				return (pages - _last_digit) / 10 * 10 + _last_digit;
			};
		// Формируем список с ближайшими меньшими числами, оканчивающиеся на цифры из списка digits
		std::ranges::transform(digits, digits.begin(), get_near_number);
		// Полученный список обязательно должен быть отсортирован в обратном порядке
		std::ranges::sort(digits, [](const auto& a, const auto& b) { return b < a; });
		// Заодно удаляем дубликаты
		digits.erase(std::ranges::unique(digits).begin(), digits.end());
		// Для чистоты вычислений приводим тип
		TValue _size = static_cast<TValue>(digits.size());
		//Вычисляем позицию числа в списке digits, которое соответствует смещению count
		TIndex idx{ (count % _size) - 1 };
		// Т.к.последующая последняя цифра повторяется через 10,
		// вычисляем множитель с учетом уже вычисленных значений
		TValue multiplier{ (count - 1) / _size };

		return ((idx < 0) ? *std::ranges::next(digits.end(), idx)
			: *std::ranges::next(digits.begin(), idx)) - (multiplier * 10);
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
	выполнена в качестве учебных целей.

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


		auto _size{ std::ranges::distance(digits) };
		if (_size == 0) return std::vector<TDigits>{};

		TDigits _digits;
		_digits.reserve(_size);

		// Сохраняем копию исходного списка для формирования комбинаций перестановок
		//TDigits _digits{ std::forward<TContainer>(digits) }; // Идеальный вариант, но ограничен только vector
		// Т.к. неизвестно какой тип контейнера будет передан, используем максимально обобщенный вариант инициализации
		_digits.assign(std::ranges::begin(digits), std::ranges::end(digits));

		// Класс для запуска асинхронных задач. Задаем режим без аккумулирования результата
		GetCombinations<TNumber> combination_numbers(false);
		// Формируем следующую комбинацию из одиночных цифр
		// Каждая комбинация передается в класс GetCombinations, который для их обработки
		// запускает отдельную асинхронную задачу
		do
			combination_numbers.add_digits(_digits);
		while (std::next_permutation(_digits.begin(), _digits.end()));
		// Запрашиваем у класса GetCombinations дождаться завершения асинхронных задач,
		// собрать результаты их работы и получить итоговый результирующий список перестановок
		return combination_numbers.get_combinations();
	};

} 
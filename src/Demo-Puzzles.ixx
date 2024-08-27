module;
#include <algorithm>
#include <execution>
#include <format>
#include <future>
#include <iterator>
#include <ranges>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <map>
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
		using TFuture = std::future<TResult>;
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
		std::recursive_mutex task_list_mutex;
		// Пул задач для ограничения максимального количества одновременно запущенных задач.
		// По умолчанию устанавливается равным числу ядер процессора
		std::unique_ptr<std::counting_semaphore<>> task_pool{ nullptr };
		// Отдельный поток для фоновой сборки результатов завершенных асинхронных задач
		std::jthread get_result_thread;
		// Ожидание готовности результатов для фонового процесса сборки результата
		std::atomic_flag result_ready_flag = ATOMIC_FLAG_INIT;
		// Флаг для запроса формирования полного результата
		std::atomic_flag request_full_result_flag = ATOMIC_FLAG_INIT;
		// Счетчик завершенных задач для пробуждения get_result_thread
		std::atomic_uint done_task_count{ 0 };
		// Запрос на остановку для фонового потока предварительной сборки результата
		std::stop_source stop_get_result_thread;
		// Запрос на остановку всех запущенных асинхронных задач
		std::stop_source stop_running_task_thread;
		// Списки run_task_list и lazy_task_list призваны обеспечить независимый запуск новых задач от общего списка task_list
		std::list<TFuture> run_task_list{};
		std::mutex run_task_list_mutex;
		std::vector<TDigits> lazy_task_list{};
		std::mutex lazy_task_list_mutex;


		// Минимизируем связь между запуском новых задач и работой с общим списком задач task_list для получения результатов
		void run_new_task(TDigits&& digits, std::launch&& policy)
		{
			TFuture _future;
			switch (policy)
			{
			case std::launch::async:
				// Т.к. запуск асинхронной задачи процесс длительный, выполняем его вне блокировки
				_future = std::async(std::launch::async, &GetCombinations::make_combination_task, this, std::move(digits));
				{
					std::lock_guard<decltype(run_task_list_mutex)> lock_run_task_list(run_task_list_mutex);
					run_task_list.emplace_back(std::move(_future));
				}
				break;
			case std::launch::deferred:
				{
					// Ленивые задачи вообще не запускаем. Просто сохраняем входные параметры для запуска в будущем
					// Этим сокращаем размер общего списка задач task_list, что ускоряет обработку результатов завершенных задач
					std::lock_guard<decltype(lazy_task_list_mutex)> lock_lazy_task_list(lazy_task_list_mutex);
					lazy_task_list.emplace_back(std::move(digits));
				}
				break;
			default:
				break;
			}
		}

		// Когда поступает запрос на получение полного результата, необходимо запустить все отложенные "ленивые" задачи
		void run_lazy_tasks()
		{
			// Используем временные буферные списки, дабы сократить время блокировок lazy_task_list и run_task_list
			decltype(lazy_task_list) _lazy_task_list_buff;
			decltype(run_task_list) _run_task_list_buff;
			{
				std::lock_guard<decltype(lazy_task_list_mutex)> lock_lazy_task_list(lazy_task_list_mutex);
				if (!lazy_task_list.empty())
				{
					_lazy_task_list_buff.reserve(lazy_task_list.size());
					_lazy_task_list_buff = std::move(lazy_task_list);
				}
			}

			// Запуск потоков длительная операция. Делаем это вне блокировок через буферный список.
			if(!_lazy_task_list_buff.empty())
				// Считываем сохраненные данные для параметров "ленивых" задач и запускаем их как асинхронные задачи
				for (auto& args : _lazy_task_list_buff)
					_run_task_list_buff.emplace_back(std::async(std::launch::async, &GetCombinations::make_combination_task, this, std::move(args)));

			// Максимально быстро перемещаем futures запущенных "ленивых" задач в run_task_list
			if (!_run_task_list_buff.empty())
			{
				std::lock_guard<decltype(run_task_list_mutex)> lock_run_task_list(run_task_list_mutex);
				run_task_list.splice(run_task_list.end(), _run_task_list_buff);
			}
		}

		// Единственный метод, в котором присутствует зависимость между запуском новых задач и общим списком task_list
		// Максимально быстро с помощью splice переносим новые запущенные задачи в общий список
		void load_running_tasks()
		{
			std::scoped_lock lock_task_list(task_list_mutex, run_task_list_mutex);
			if (!run_task_list.empty())
				task_list.splice(task_list.end(), run_task_list);
		}

		bool is_stop_requested() const noexcept
		{
			return stop_running_task_thread.stop_requested();
		}

		// Пробуждаем задачу предварительной сборки результатов - get_result_task()
		void notify_get_result_task() noexcept
		{
			result_ready_flag.test_and_set();
			result_ready_flag.notify_one();
		}

		/*
		Фоновая задача, которая предварительно собирает результаты завершенных асинхронных задач
		по мере их готовности до момента вызова get_combinations.
		По отдельному запросу get_combinations собирает окончательный полный результат.
		*/
		void get_result_task();

		/*
		Функция, которая запускается как отдельная асинхронная задача.
		Получаем копию параметра digits, т.к. вызывающая функция будет менять набор цифр
		не дожидаясь пока асинхронная задача отработает. Для асинхронной задачи необходима автономность данных.
		*/
		auto make_combination_task(TDigits) -> decltype(result_list);

	public:
		GetCombinations(TPoolSize pool_size = 0, bool accumulate_result = true) : accumulate_result_flag{ std::move(accumulate_result) }
		{
 			auto _pool_size = (pool_size > 0) ? std::move(pool_size) : std::thread::hardware_concurrency();
			//Инициализируем пул задач в виде счетчика-семафора
			task_pool = (_pool_size) ? std::make_unique<std::counting_semaphore<>>(_pool_size)
				// На случай, если std::thread::hardware_concurrency() не сработает
				: std::make_unique<std::counting_semaphore<>>(_DEFAULT_POOL_SIZE_);
			// Запускаем фоновую задачу предварительной сборки результатов
			get_result_thread = std::jthread(&GetCombinations::get_result_task, this);
		}

		template <typename _TPoolSize = TPoolSize> //Требуется для неявного приведения типа
		explicit GetCombinations(_TPoolSize pool_size) : GetCombinations{ std::move(static_cast<TPoolSize>(pool_size)), true } {}

		explicit GetCombinations(bool accumulate_result) : GetCombinations{ 0, std::move(accumulate_result) } {}

		// Копирование запрещаем, т.к. пул асинхронных задач должен выполняться в рамках единственного экземпляра класса.
		GetCombinations(const GetCombinations&) = delete;
		GetCombinations& operator=(const GetCombinations&) = delete;

		~GetCombinations()
		{
			// Отправляем всем запущенным задачам запрос на останов и предотвращаем запуск новых задач
			stop_running_task_thread.request_stop();
			// Останавливаем фоновую задачу предварительной сборки результатов
			stop_get_result_thread.request_stop();

			{
				std::lock_guard<decltype(task_list_mutex)> lock_task_list(task_list_mutex);
				this->load_running_tasks(); // Подгружаем в task_list уже запущенные задачи
				if (!task_list.empty())
				{
					// Пытаемся выполнить ожидание завершения задач параллельно
					std::for_each(std::execution::par,
						task_list.cbegin(),
						task_list.cend(),
						[](auto& t) { if (t.valid()) t.wait(); });  // Ждем завершения задач

					task_list.clear();
					done_task_count.store(0);
				}
			}

			// Ждем завершения фоновой задачи предварительной сборки результатов
			if (get_result_thread.joinable()) get_result_thread.join();
		}

		void add_digits(TDigits digits, std::launch policy = std::launch::async)
		{
			// Планируем на выполнение асинхронные задачи. При этом задачи могут быть "ленивыми".
			// Зависит от заданной политики. Допускаются комбинации из асинхронных и "ленивых" задач
			// Новые задачи запускаются только если не поступил запрос об остановке
			if (!this->is_stop_requested())
				this->run_new_task(std::move(digits), std::move(policy));
		}

		void add_digits(TNumber number, std::launch policy = std::launch::async)
		{
			// Числа распарсиваются в цифры и из них формируется асинхронная задача
			if (!this->is_stop_requested())
				this->run_new_task(assistools::inumber_to_digits(std::move(number)), std::move(policy));
		}

		auto get_combinations() -> decltype(result_list)
		{
			/*
			По запросу из вызывающей функции начинаем собирать итоговый результат в асинхронном режиме.
			Т.е. если какая-либо задача из списка еще не отработала и не может предоставить результат своей работы,
			не ждем ее завершения, а переходим к следующей задаче из списка и проверяем ее готовность.
			При готовности задачи, добавляем результат ее работы в итоговый результат и удаляем задачу из списка.
			Повторяем цикл проверки готовности задач пока список задач не опустеет.
			*/
			request_full_result_flag.test_and_set(); // Требуем собрать полный результат
			this->notify_get_result_task(); // Отправляем требование в фоновую задачу get_result_task
			request_full_result_flag.wait(true); // Ждем готовности полного результата
			decltype(result_list) _return_result{}; //Временный список для возвращаемого результата
			// Если аккумулировать результат не требуется, перемещаем result_list, что приведет к обнулению списка
			// Делаем это под блокировкой, т.к. поток get_result_task также имеет доступ к result_list
			std::unique_lock<decltype(result_list_mutex)> lock_result_list(result_list_mutex);
			_return_result.reserve(result_list.size());
			_return_result = (accumulate_result_flag) ? result_list : std::move(result_list);
			lock_result_list.unlock(); //Отпускаем блокировку
			// Сортируем для удобства восприятия (не обязательно).
			std::ranges::sort(_return_result);
			return _return_result;			
		}
	};

	template <class TNumber>
	void GetCombinations<TNumber>::get_result_task()
	{
		// Буфер для частичной сборки результата дабы уменьшить число блокировок result_list
		decltype(result_list) result_list_buff{ };

		auto stop_task{ stop_get_result_thread.get_token() };
		// При получении запроса на останов, поток сам себя пробуждает
		std::stop_callback stop_task_callback(stop_task, [this]
			{
				if (!result_ready_flag.test()) this->notify_get_result_task();
			});
		// Если поступил запрос на остановку, завершаем фоновый процесс предварительной сборки результатов
		while (!stop_task.stop_requested())
		{
			// Засыпаем, если нет уведомления о готовности результатов из асинхронных задач make_combination_task,
			// нет уведомления об остановке потока и нет запроса на формирование полного результата
			if ((done_task_count.load() == 0)
				&& !stop_task.stop_requested()
				&& !request_full_result_flag.test()) result_ready_flag.wait(false);
			// Сбрасываем флаг пробуждения, чтобы была возможность снова заснуть по окончании цикла обработки
			result_ready_flag.clear();
			// Выясняем причину пробуждения
			if (stop_task.stop_requested()) // Если причина пробуждения запрос на останов задачи, досрочно выходим
				return;

			if (request_full_result_flag.test()) // Поступил запрос на формирование полного результата
			{
				this->run_lazy_tasks(); // Запускаем все "ленивые" задачи
				std::unique_lock<decltype(task_list_mutex)> lock_task_list(task_list_mutex);
				this->load_running_tasks(); // Подгружаем в task_list новые уже запущенные задачи
				while (!task_list.empty() && !stop_task.stop_requested())
				{
					//Просматриваем список запущенных задач. При этом размер списка может динамически меняться.
					// Потому в каждом цикле вызываем task_list.end(), дабы получить актуальный размер списка
					for (auto future_iter{ task_list.begin() }; (future_iter != task_list.end())
						&& !stop_task.stop_requested();) // Если поступил запрос на останов, досрочно выходим
					{
						if (auto& future_task{ *future_iter }; future_task.valid()) [[likely]]
							{
								// Метод wait_for нужен только для получения статусов, потому вызываем с нулевой задержкой.
								// Для каждой задачи отрабатываем два статуса: ready и deferred
								switch (future_task.wait_for(std::chrono::milliseconds(0)))
								{
								case std::future_status::ready: //Задача отработала и результат готов
									//Перемещаем полученный от задачи результат в буферный список
									std::ranges::move(future_task.get(), std::back_inserter(result_list_buff));
									done_task_count.fetch_sub(1);
									//Удаляем задачу из списка обработки. Итератор сам сместиться на следующую задачу.
									future_iter = task_list.erase(future_iter);
									break;

								case std::future_status::timeout:
								default:
									// Если задача еще не отработала и результат не готов, пропускаем и переходим к следующей.
									// При этом задача остается в списке и будет еще раз проверена на готовность в следующем цикле
									++future_iter;
									break;
								}
							}
						else
							//Если фьючерс не валиден, удаляем соответствующую ему задачу из списка без обработки
							future_iter = task_list.erase(future_iter);
					}
				}
				lock_task_list.unlock(); //Далее блокировка списка задач не нужна

				if (!result_list_buff.empty() && !stop_task.stop_requested())
				{
					// Загружаем в итоговый список под блокировкой
					std::lock_guard<decltype(result_list_mutex)> lock_result_list(result_list_mutex);
					if (accumulate_result_flag)
					{
						// Догружаем в итоговый result_list все, что накопилось в результате частичных выгрузок
						result_list.reserve(result_list.size() + result_list_buff.size()); // Расширяем список для получения новых данных
						std::ranges::move(result_list_buff, std::back_inserter(result_list)); // Догружаем новые данные.
						result_list_buff.clear();
					}
					else
						//Если аккумулировать результат не нужно, перезаписываем result_list, одновременно перемещая и очищая буфер
						result_list = std::move(result_list_buff);
				}

				request_full_result_flag.clear(); //Сбрасываем флаг требования получения полного результата
				request_full_result_flag.notify_one(); //Уведомляем ожидающий поток о готовности полного результата
			}
			else if (done_task_count.load() > 0) // Одна из задач готова предоставить результат
			{
				// Для минимизации времени блокировки списка задач task_list, с помощью splice перемещаем фьючерсы
				// завершенных задач во временный список _done_task_list для их обработки вне блокировки
				decltype(task_list) _done_task_list{};
				// Просматриваем список task_list в поиске задач с готовыми результатами.
				std::unique_lock<decltype(task_list_mutex)> lock_task_list(task_list_mutex);
				this->load_running_tasks(); // Подгружаем в task_list новые уже запущенные задачи
				for (auto future_iter{ task_list.begin() }; (future_iter != task_list.end())
					&& !stop_task.stop_requested() // Если поступил запрос на останов, досрочно выходим
					&& done_task_count.load() > 0;) // Если счетчик завершенных задач обнулился, досрочно выходим
				{
					if (auto& future_task{ *future_iter }; future_task.valid()) [[likely]]
						{
							if (future_task.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
							{
								// В отличии от erase, метод splice не сдвигает итератор на следующий элемент списка
								// Более того, после применения splice, итератор будет указывать на _done_task_list
								// Создаем копию итератора для splice и смещаем текущий итератор на следующий элемент
								auto _future_iter{ future_iter++ };
								_done_task_list.splice(_done_task_list.end(), task_list, _future_iter);
								done_task_count.fetch_sub(1);
							}
							else
								++future_iter;
						}
					else
						future_iter = task_list.erase(future_iter);
				}
				lock_task_list.unlock();

				if (!stop_task.stop_requested())
					//Перемещаем полученные от задач результаты в буферный список
					for (auto& future_task : _done_task_list)
						std::ranges::move(future_task.get(), std::back_inserter(result_list_buff));
				else
					// Корректно завершаем заачи без получения результата
					std::for_each(std::execution::par,
						_done_task_list.cbegin(),
						_done_task_list.cend(),
						[] (auto& t) { if (t.valid()) t.wait(); });
			}
		}
	};

	
	template <class TNumber>
	auto GetCombinations<TNumber>::make_combination_task(TDigits digits) -> decltype(result_list)
	{
		// Каждая задача собирает свою автономную часть результатов, которые далее в цикле
		// асинхронного получения результатов соберутся в итоговый список
		decltype(result_list) result_buff{};
		// Каждая задача создает свой токен, через который принимает сигнал на останов
		auto stop_task{ stop_running_task_thread.get_token() };

		// На всякий случай проверка пороговых значений. Это может выглядеть излишней перестраховкой,
		// т.к. вызывающая функция get_combination_numbers_async предварительно делает необходимые проверки.
		if (digits.empty() || stop_task.stop_requested())
			return result_buff;

		// Получаем из пула задач разрешение на запуск. Если пул полон, ждем своей очереди
		task_pool->acquire();

		// Добавляем комбинацию из одиночных цифр
		result_buff.emplace_back(digits);
		// Сразу же добавляем число из всего набора цифр
		if ((digits.size() > 1) && (digits.front() != 0) && !stop_task.stop_requested())
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
			while ((it_first_digit != it_last_digit) && !stop_task.stop_requested())
			{
				// Числа, начинающиеся с 0, пропускаем
				if (*it_first_digit != 0)
				{
					// Комбинируем полученное число с цифрами оставшимися вне окна выборки
					auto combo_list = std::views::join(decltype(result_buff){
						{ digits.begin(), it_first_digit }, // Цифры слева
							// Формируем число из цифр, отобранных окном выборки
						{ assistools::inumber_from_digits(it_first_digit, it_last_digit) },
						{ it_last_digit, digits.end() } // Цифры справа
					});
					result_buff.emplace_back(combo_list.begin(), combo_list.end());
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
		if (!stop_task.stop_requested())
		{
			done_task_count.fetch_add(1);
			this->notify_get_result_task();
		}
		// Это еще не окончательный результат, а лишь промежуточный для конкретной асинхронной задачи
		return result_buff;
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
		-> const std::make_signed_t<typename TContainer::size_type>
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
		-> const typename TContainer::value_type
	{
		// Отрабатываем некорректные параметры
		if (count <= 0 || pages < count || digits.size() == 0) return 0;
		// Индекс может быть отрицательным
		using TIndex = std::make_signed_t<typename TContainer::size_type>;
		using TValue = typename TContainer::value_type;

		auto get_near_number = [&pages](const TValue& last_digit) -> TValue
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

	@param digits - Список заданных цифр или чисел

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

		// Сохраняем копию исходного списка для формирования комбинаций перестановок.
		// TDigits _digits{ std::forward<TContainer>(digits) }; - идеальный вариант, но ограничен только vector.
		// Т.к. неизвестно какой тип контейнера будет передан, используем максимально обобщенный вариант инициализации
		_digits.assign(std::ranges::begin(digits), std::ranges::end(digits));

		// Класс для запуска асинхронных задач. Задаем режим без аккумулирования результата
		GetCombinations<TNumber> combination_numbers(false);
		// Формируем следующую комбинацию из одиночных цифр
		// Каждая комбинация передается в класс GetCombinations, который для её обработки
		// запускает отдельную асинхронную задачу
		do
			combination_numbers.add_digits(_digits);
		while (std::next_permutation(_digits.begin(), _digits.end()));
		// Запрашиваем у класса GetCombinations дождаться завершения асинхронных задач,
		// собрать воедино результаты их работы и получить итоговый результирующий список перестановок
		return combination_numbers.get_combinations();
	};


	/**
	@brief Получить число, максимально близкое к числу X, из суммы неотрицательных чисел массива,
    при условии, что числа из массива могут повторяться.

	@details Внимание!!! Отрицательные числа игнорируются.

	@param numbers - Массив чисел
	@param target - Целевое число

	@return std::pair из искомого числа и списка(-ов) наборов чисел, сумма которых равна искомому числу
	*/
	template <typename TContainer = std::vector<int>, typename TNumber = typename TContainer::value_type>
	requires std::ranges::range<TContainer>
		&& std::is_integral_v<TNumber>
		&& std::is_arithmetic_v<TNumber>
	auto closest_amount(const TContainer& numbers, const TNumber& target)
		-> std::pair<TNumber, std::vector<std::vector<TNumber>>>
	{
		struct CurrentSum
		{
			TNumber curr_sum = 0;
			std::vector<TNumber> curr_numbers = { };
		};
		using TResult = std::pair<TNumber, std::vector<std::vector<TNumber>>>;
		// Искомое число и списки чисел, суммы которых равны искомому числу
		TNumber max_sum{ 0 };
		std::vector<std::vector<TNumber>> max_sum_numbers{ };
		// Очередь из промежуточных накопительных сумм и списков чисел, из которых состоят эти суммы
		std::queue<CurrentSum> query_buff;
		// Стартуем с нуля
		query_buff.emplace(CurrentSum{ 0, { } });

		while (!query_buff.empty())
		{
			// Вынимаем из очереди очередную промежуточную сумму и поочередно суммируем ее с числами из входного массива.
			auto [current_sum, current_numbers] = query_buff.front();
			query_buff.pop();
			for (const auto& number : numbers)
			{
				// Формируем очередную сумму и набора чисел, ее составляющих.
				// При этом новая сумма не должна быть больше искомого числа.
				// Отрицательные числа из входного массива игнорируем.
				if (auto next_sum{ current_sum + number }; next_sum <= target && number > 0)
				{
					auto next_numbers{ current_numbers };
					next_numbers.emplace_back(number);
					// Сортировка позволяет избежать дублирование списков
					std::ranges::sort(next_numbers);
					// Если очередная полученная сумма больше ранее вычисленной максимальной суммы,
					// обновляем максимальную сумму и перезаписываем список чисел, которые ее формируют
					if (max_sum < next_sum)
					{
						max_sum = next_sum;
						max_sum_numbers.assign({ next_numbers });
					}
					// Одна и та же сумма, может быть получена различными комбинациями чисел из входного массива
					else if (max_sum == next_sum)
					{
						max_sum_numbers.emplace_back(next_numbers);
					}
					// Добавляем в очередь очередную сумму со списком чисел для дальнейшей обработки
					query_buff.emplace(CurrentSum{ std::move(next_sum), std::move(next_numbers) });
				}
			}
		}
		// Удаляем дубли из списка списков чисел, формирующих итоговое искомое число
		if (max_sum_numbers.size() > 1)
		{
			std::ranges::sort(max_sum_numbers);
			const auto rng = std::ranges::unique(max_sum_numbers);
			max_sum_numbers.erase(rng.begin(), rng.end());
		}

		return TResult{ max_sum , max_sum_numbers };
	};


	/**
	@brief Находит две пары множителей в массиве чисел, дабы получить минимально возможное
	и максимально возможное произведение. Допускаются отрицательные значения и ноль.

	@details Попытка реализовать максимально обобщенный вариант без индексирования,
	без сортировки и без изменения исходных данных. Используется только итератор.

	@param numbers - Набор чисел.

	@return std::pair - Пара минимального и максимального значений произведения.
	*/
	template <typename TContainer = std::vector<int>, typename TNumber = typename std::remove_cvref_t<TContainer>::value_type>
		requires std::ranges::range<TContainer>
			&& std::is_arithmetic_v<TNumber>
	auto get_minmax_prod(TContainer&& numbers)
		-> std::pair<TNumber, TNumber>
	{
		using TResult = std::pair<TNumber, TNumber>;

		auto it_num{ std::ranges::cbegin(numbers) };

		// Отрабатываем пограничные случаи
		switch (std::ranges::distance(numbers))
		{
		case 0:  // Список пуст
			return TResult{ 0, 0 };
		case 1:  // Список из одного значения
			return TResult{ *it_num, *it_num };
		case 2:  // В списке два значения
			auto _prod = (*it_num) * (*std::ranges::next(it_num));
			return TResult{ _prod, _prod };
		}

		// Пары минимальных и максимальных значений инициализируем первыми числами списка
		TNumber min1{}, min2{}, max1{}, max2{};
		min1 = max1 = *it_num;
		min2 = max2 = *(++it_num);

		// Важно изначально инициализировать min и max корректными относительными значениями
		if (min2 < min1)
			min2 = std::exchange(min1, min2);
		if (max1 < max2)
			max2 = std::exchange(max1, max2);

		// Стартуем с третьего числа списка
		for (++it_num; it_num != std::ranges::cend(numbers); ++it_num)
		{
			auto num = *it_num;
			// Все время нужно помнить, что min и max парные значения. Меняя первый, меняем второй
			if (num < min1)
				min2 = std::exchange(min1, num);
			else if (num < min2)
				min2 = num;

			if (max1 < num)
				max2 = std::exchange(max1, num);
			else if (max2 < num)
				max2 = num;
		}
		// Парные произведения понадобятся далее для сравнений
		auto min_prod = min1 * min2;
		auto max_prod = max1 * max2;
		TResult result{};

		if (min1 < 0 and max1 < 0)  // Все числа отрицательные
			result = std::make_pair(max_prod, min_prod);
		else if (min1 < 0 and !(max1 < 0))  //Часть чисел отрицательные
			result = std::make_pair(min1 * max1, (max_prod < min_prod) ? min_prod : max_prod);
		else if (!(min1 < 0) and !(max1 < 0))  //Все числа неотрицательные (включая ноль)
			result = std::make_pair(min_prod, max_prod);

		return result;
	};


	/**
	@brief Из заданного набора целых чисел получить список возрастающих чисел
	за минимальное количество изменений исходного списка. Возможны как положительные,
	так и отрицательные значения, включая ноль. Сортировка не требуется.

	@example
	get_incremental_list([1, 7, 3, 3]) -> (2, [1, 2, 3, 4])
	get_incremental_list([3, 2, 1]) -> (0, [3, 2, 1])
	get_incremental_list([-2, 0, 4]) -> (1, [-2, 0, -1])

	@param numbers - Заданный список целых чисел.

	@return std::pair - Количество изменений и список возрастающих чисел.
	*/
	template <typename TContainer = std::vector<int>, typename TNumber = typename std::remove_cvref_t<TContainer>::value_type>
		requires std::ranges::range<TContainer>
				&& std::is_integral_v<TNumber>
	auto get_incremental_list(TContainer&& numbers)
		-> std::pair<typename std::remove_cvref_t<TContainer>::size_type, std::vector<TNumber>>
	{
		using TSize = typename std::remove_cvref_t<TContainer>::size_type;
		using TResult = std::pair<TSize, std::vector<TNumber>>;
		// Определяем размер исходного списка и отрабатываем вариант пустого списка
		auto size_data = std::ranges::distance(numbers);
		if (size_data == 0)
			return TResult{ 0, std::vector<TNumber>{} };
		// Находим минимальное значение, с которого начинается отсчет
		TNumber start_num = std::ranges::min(numbers);
		// Вычисляем конечное значение результирующего списка, исходя из длины заданного списка
		auto end_num = (size_data + start_num) - 1;
		// В словарь со счетчиком загружаем из исходного списка числа, которые попадают в диапазон
		// результирующего списка. Их менять не нужно.
		std::unordered_map<TNumber, TSize> used_numbers_map;
		std::ranges::for_each(numbers,
			[&end_num, &used_numbers_map](const auto& n) { if (n <= end_num) used_numbers_map.try_emplace(n, 0); });
		// Результирующий список и счетчик потребовавшихся изменений.
		std::vector<TNumber> result_list{};
		result_list.reserve(size_data);
		TSize result_count{ 0 };
		// Т.к. start_num (оно же минимальное число) гарантировано присутствует в результирующем
		// списке, пропускаем его и начинаем со следующего по порядку числа.
		for (++start_num; const auto& num : numbers)
		{
			// Отлавливаем две ситуации: числа не входящие в диапазон результирующего списка и
			// повторяющиеся числа. Если число уже попало в результирующий список, то заменяем его.
			if (num > end_num or used_numbers_map[num] > 0)
			{
				// Вычисляем следующее по порядку число, которым заменим "неподходящее" значение
				while (used_numbers_map.contains(start_num))
					++start_num;
				// Вместо "неподходящего" числа вставляем число-заменитель. Сразу же переходим
				// на следующее по порядку число-заменитель, дабы сократить цикл while (см. выше)
				result_list.emplace_back(start_num++);
				++result_count;
			}
			else
			{
				// Число, для которого не требуется замена, просто добавляем в результирующий список
				// и наращиваем счетчик в словаре использованных чисел
				result_list.emplace_back(num);
				++used_numbers_map[num];
			}
		}

		return TResult{ result_count, result_list };
	};


	/**
	@brief Из заданного набора символов сформировать палиндром.

	@param chars - Список символов.
	@param with_separator - Добавлять символ-разделитель. Default: true

	@return std::string - Палиндром. Если сформировать палиндром не удалось, возвращается пустая строка.
	*/
	auto get_word_palindrome(const std::string&& chars, bool with_separator = true)
		-> typename std::remove_cvref_t<decltype(chars)>
	{
		using TString = typename std::remove_cvref_t<decltype(chars)>;
		using TChar = TString::value_type;
		TString result{};
		// Массив для аккумулирования кандидатов символов-разделителей между половинами палиндрома
		std::vector<TChar> separator_candidate{};
		TString half_palindrome{};
		{ // Безыменный namespace ограничивает время жизни char_count_map
			// Подсчитываем количество символов в заданном наборе
			std::unordered_map<TChar, size_t> char_count_map;
			for (const auto& chr : chars)
				++char_count_map[chr];

			for (const auto& [_char, _count] : char_count_map)
			{
				// Если количество символа нечетное, то это потенциальный символ-разделитель
				if (with_separator and (_count & 1))
					separator_candidate.emplace_back(_char);
				// Формируем половину палиндрома из символов, у которых количество пар одна и более
				if (auto pair_count{ _count >> 1 }; pair_count > 0)
					half_palindrome.append(pair_count, _char);
			}
		}

		if (!half_palindrome.empty())
		{
			result.reserve((half_palindrome.size() << 1) + (separator_candidate.empty() ? 0 : 1));
			// Формируем левую половину палиндрома
			std::ranges::sort(half_palindrome);
			result = half_palindrome;
			// Определяем символ-разделитель как лексикографически минимальный
			if (with_separator and !separator_candidate.empty())
				result += std::ranges::min(std::move(separator_candidate));
			// Собираем итоговый палиндром
			result.append(half_palindrome.rbegin(), half_palindrome.rend());
		}

		return result;
	};


	/**
	@brief Алгоритм поиска в заданном списке непрерывной последовательности чисел,
    сумма которых минимальна/максимальна. Заданный список может содержать как положительные,
    так и отрицательные значения, повторяющиеся и нулевые. Предварительная сортировка не требуется.

	@details Алгоритм за один проход по заданному списку одновременно находит минимальную и максимальную
    суммы и все возможные комбинации непрерывных диапазонов чисел, формирующие эти суммы.

    Для примера на максимальной сумме, суть алгоритма в следующем:
    Числа из списка накопительно суммируются. Если очередная накопительная сумма становится
    отрицательной, то это означает, что предыдущий диапазон чисел можно отбросить.
    Например:  [1, 2, -4, 5, 3, -1]  Первые три значения дают отрицательную сумму.
    Значит максимальная сумма возможна только со следующего значения (5 и т.д.).
    Первые три числа исключаются. На третьей итерации (-4) накопительная сумма обнуляется,
    начальный индекс сдвигается на следующую позицию (5) и накопительная сумма
    формируется заново.

    Получение нулевой накопительной суммы, означает один из вариантов для максимальной суммы.
    Например, для списка [-1, 3, -2, 5, -6, 6] максимальная сумма 6 может быть получена из
    (3, -2, 5), (3, -2, 5, -6, 6) и (6). На значении -6 накопительная сумма становится равной нулю,
    что приводит к добавлению дополнительного начального индекса, указывающего на следующее число (6).
    Для каждого варианта в списке сохраняются начальные индексы, из которых формируются список пар
    начальных и конечных индексов. В данном примере список пар индексов для 6: (1, 3), (1, 5), (5, 5)

	@param numbers: Список чисел.

	@return Словарь (std::map), в качестве ключей содержащий минимальную и максимальную суммы,
	а в качестве значений список пар диапазонов чисел. Внимание!!! Конечный индекс закрытый и указывает
	на число, входящее в диапазон. Для итерации конечный индекс необходимо нарастить на единицу.
	*/
	template <typename TContainer = std::vector<int>,
		typename TNumber = typename std::remove_cvref_t<TContainer>::value_type,
		typename TIndex = typename std::remove_cvref_t<TContainer>::size_type>
		requires std::ranges::range<TContainer> && std::is_arithmetic_v<TNumber>
	auto get_minmax_ranges(TContainer&& numbers) -> std::map<std::string, std::vector<std::pair<TIndex, TIndex>>>
	{
		using TRanges = std::vector<std::pair<TIndex, TIndex>>;
		using TResult = std::map<std::string, TRanges>;
		//получаем первый элемент для инициализации
		auto iter_numbers{ std::ranges::cbegin(numbers) };

		// Отрабатываем пограничные случаи
		switch (std::ranges::distance(numbers))
		{
		case 0:  // Список пуст
			return TResult{ { "Data is empty!", {} } };
		case 1:  // Список из одного значения
			return TResult
			{
				{ std::format(" Min sum: {0} ", *iter_numbers), TRanges{std::make_pair(0, 0)} },
				{ std::format(" Max sum: {0} ", *iter_numbers), TRanges{std::make_pair(0, 0)} }
			};
		}

		enum struct SumMode
		{
			MIN,
			MAX
		};

		//Внутренний класс, вычисляющий минимальную или максимальную сумму в зависимости от заданного режима.
		struct Sum
		{
			// Искомая минимальная или максимальная сумма
			TNumber sum;
			// Список пар индексов begin/end диапазона чисел, составляющих сумму
			TRanges ranges{};

			Sum(const TNumber& init_number, SumMode init_mode) : sum{ init_number }, mode{ init_mode } { }

			void operator()(const TIndex& index, const TNumber& number)
			{
				accumulated += number;
				// Если накопленная сумма больше/меньше или сравнялась с ранее сохраненной
				if ((mode == SumMode::MAX) ? accumulated >= sum : accumulated <= sum)
				{
					// Если накопленная сумма больше / меньше
					if ((mode == SumMode::MAX) ? accumulated > sum : accumulated < sum)
					{
						sum = accumulated; // Сохраняем накопленную сумму как искомую
						ranges.clear(); // Сбрасываем список пар индексов и формаруем новый
					}
					// Если накопленная сумма больше/меньше или равна, то формируем список пар начальных и конечных индексов
					for (const auto& begin_index : begin_index_list)
						ranges.emplace_back(begin_index, index);
				}
				// Если накопленная сумма отрицательная/положительная или нулевая
				if ((mode == SumMode::MAX) ? accumulated <= 0 : accumulated >= 0)
				{
					// При отрицательной/положительной накопленной сумме
					if ((mode == SumMode::MAX) ? accumulated < 0 : accumulated > 0)
					{
						accumulated = 0; // Обнуляем накопленную сумму
						begin_index_list.clear(); // Очищаем список начальных индексов
					}
					//При отрицательной/положительной накопленной сумме формируем список начальных индексов заново.
					// При нулевой - добавляем новый начальный индекс
					begin_index_list.emplace_back(index + 1);
				}
			}

		private:
			SumMode mode;
			TNumber accumulated{ 0 }; // Накопительная сумма
			// Список начальных индексов, т.к. одна и та же сумма может быть получена из разного набора цифр
			std::vector<TIndex> begin_index_list{ 0 };
		};
		// Инициализируем первым элементом списка
		Sum min_sum{ *iter_numbers, SumMode::MIN };
		Sum max_sum{ *iter_numbers, SumMode::MAX };

		for (TIndex idx{ 0 }; iter_numbers != std::ranges::cend(numbers); ++iter_numbers, ++idx)
		{
			min_sum(idx, *iter_numbers);
			max_sum(idx, *iter_numbers);
		}
		// Для результирующего словаря в качестве ключей используем строковые значения,
		// т.к.минимальная и максимальная суммы могут быть равны.
		return TResult
		{
			{ std::format(" Min sum: {0} ", min_sum.sum), min_sum.ranges },
			{ std::format(" Max sum: {0} ", max_sum.sum), max_sum.ranges }
		};
	};
} 
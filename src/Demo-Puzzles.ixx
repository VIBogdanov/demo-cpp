module;
#include <algorithm>
#include <execution>
#include <future>
#include <iterator>
#include <ranges>
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
				for (auto& digits : _lazy_task_list_buff)
					_run_task_list_buff.emplace_back(std::async(std::launch::async, &GetCombinations::make_combination_task, this, std::move(digits)));

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

		void stop_running_task()
		{
			// Отправляем всем запущенным задачам запрос на останов и предотвращаем запуск новых задач
			stop_running_task_thread.request_stop();
			
			std::lock_guard<decltype(task_list_mutex)> lock_task_list(task_list_mutex);
			this->load_running_tasks(); // Подгружаем в task_list новые уже запущенные задачи
			if (!task_list.empty())
			{
				// Пытаемся выполнить ожидание задач параллельно
				std::for_each(std::execution::par,
					task_list.cbegin(),
					task_list.cend(),
					[](auto& t) { if (t.valid()) t.wait(); });  // Ждем завершения задач

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

		template <typename _TPoolSize = TPoolSize> //Требуется для неявного преобразования типа
		explicit GetCombinations(_TPoolSize pool_size) : GetCombinations{ std::move(pool_size), true } {}

		explicit GetCombinations(bool accumulate_result) : GetCombinations{ 0, std::move(accumulate_result) } {}

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
			P.S. Если встречается "ленивая" задача, то ее следует запустить, но при этом не ждать пока она отработает.
			Для этого переупаковываем "ленивые" задачи в асинхронные и добавляем в список задач. Старую "ленивую" задачу
			из списка удаляем. Делается это через копирование фьючерса "ленивой" задачи и вызова его метода get в рамках
			асинхронной задачи, что приводит к запуску "ленивой" задачи, но уже в качестве асинхронной
			*/
			request_full_result_flag.test_and_set(); // Требуем собрать полный результат
			this->notify_get_result_task(); // Отправляем требование в фоновую задачу get_result_task
			request_full_result_flag.wait(true); // Ждем готовности полного результата
			decltype(result_list) return_result{}; //Временный список для возврата результата
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

	template <class TNumber>
	void GetCombinations<TNumber>::get_result_task()
	{
		// Список для частичной сборки результата дабы уменьшить число блокировок result_list
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
			// Ожидаем, если нет уведомления о готовности результатов из асинхронных задач make_combination_task,
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
				// Собираем результат при полной блокировке. Это не позволит новым еще не запущенным задачам исказить результат
				std::unique_lock<decltype(task_list_mutex)> lock_task_list(task_list_mutex);
				this->load_running_tasks(); // Подгружаем в task_list новые уже запущенные задачи
				while (!task_list.empty() && !stop_task.stop_requested())
				{
					//Просматриваем список запущенных задач. При этом размер списка может динамически меняться.
					// Потому в каждом цикле вызываем task_list.end(), дабы получить актуальный размер списка
					for (auto it_future{ task_list.begin() }; (it_future != task_list.end())
						&& !stop_task.stop_requested();) // Если поступил запрос на останов, досрочно выходим
					{
						if (auto& future_task{ *it_future }; future_task.valid()) [[likely]]
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
									it_future = task_list.erase(it_future);
									break;
								case std::future_status::deferred: //Это "ленивая" задача. Ее нужно запустить
									// В данной реализации эта ветка никогда не будет выполняться, т.к. все "ленивые"
									// задачи уже были предварительно запущены с помощью метода run_lazy_tasks()
									// Оставлено как исторический артефакт
									// Переупаковываем "ленивую" задачу в асинхронную и запускаем, добавив в список как новую задачу.
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
					else //Если аккумулировать результат не нужно, перезаписываем result_list, одновременно перемещая и очищая буфер
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
				for (auto it_future{ task_list.begin() }; (it_future != task_list.end())
					&& !stop_task.stop_requested() // Если поступил запрос на останов, досрочно выходим
					&& done_task_count.load() > 0;) // Если счетчик завершенных задач обнулился, досрочно выходим
				{
					if (auto& future_task{ *it_future }; future_task.valid()) [[likely]]
						{
							if (future_task.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
							{
								// В отличии от erase, метод splice не сдвигает итератор на следующий элемент списка
								// Более того, после применения splice, итератор будет указывать на _done_task_list
								// Создаем копию итератора для splice и смещаем текущий итератор на следующий элемент
								auto _it_future{ it_future++ };
								_done_task_list.splice(_done_task_list.end(), task_list, _it_future);
								done_task_count.fetch_sub(1);
							}
							else
								++it_future;
						}
					else
						it_future = task_list.erase(it_future);
				}
				lock_task_list.unlock();
				//Перемещаем полученный от задачи результат в буферный список
				for (auto& future_task : _done_task_list)
				{
					if (!stop_task.stop_requested())
						std::ranges::move(future_task.get(), std::back_inserter(result_list_buff));
					else
						future_task.wait();
				}
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
					// обновляем максимальную сумму и список чисел, которые ее формируют
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
} 
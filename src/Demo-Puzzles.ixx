module;
#include <unordered_map>
#include <stdexcept>
export module Demo:Puzzles;

export namespace puzzles
{
	/**
	brief Подсчитывает минимальное количество перестановок, которое необходимо произвести для того,
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
	template <typename TContainer>
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
		for (TIndex idx{ 0 }; const TValue & item : target_list)
			target_indexes.emplace(item, idx++);
		// Попарно сравниваем целевые номера позиций для значений исходного списка.
		// Если номера позиций не по возрастанию, то требуется перестановка
		TIndex count_permutations{ 0 };
		for (TIndex prev_idx{ 0 }, next_idx{ 0 }; const TValue & item : source_list)
		{
			// На случай, если списки не согласованы по значениям
			try
			{
				next_idx = target_indexes.at(item);
			}
			catch (const std::out_of_range&)
			{
				return -1;
			}

			if (prev_idx > next_idx)
				++count_permutations;
			else
				prev_idx = next_idx;

		}

		return count_permutations;
	};
}
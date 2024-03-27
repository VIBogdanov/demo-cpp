#include <algorithm>
#include <clocale>
#include <iostream>
#include <vector>
#include <list>

import Demo;

using namespace std;

int main()
{
	setlocale(LC_CTYPE, "");

	{
		cout << endl << " - Функция преобразования целого числа в набор цифр." << endl;
		cout << " inumber_to_digits(12340) -> ";
		auto res = assistools::inumber_to_digits(12340);
		std::ranges::for_each(std::as_const(res), [](const auto& n) { cout << n << ' '; });
		cout << endl;
	}

	{
		cout << endl << " - Формирует список индексов диапазонов, на которые можно разбить список заданной длины." << endl;
		cout << " get_ranges_index(50, 10) -> ";
		auto res = assistools::get_ranges_index(50, 10);
		for (const auto& [_first, _last] : res)
			cout << "[" << _first << ", " << _last << ") ";
		cout << endl;
	}
	
	{
		cout << endl << " - Функция нахождения наибольшего общего делителя двух целых чисел без перебора методом Евклида." << endl;
		cout << " get_common_divisor(20, 12) -> ";
		if (auto res = sundry::get_common_divisor(20, 12))
			cout << res.value();
		else
			cout << "Невозможно вычислить общий делитель!";
		cout << endl;
	}

	cout << endl << " - Поиск элемента в массиве данных при помощи бинарного алгоритма." << endl;
	cout << " find_item_by_binary({ -20, 30, 40, 50 }, 30) -> ";
	cout << sundry::find_item_by_binary(std::vector<int>{ -20, 30, 40, 50 }, 30) << endl;

	cout << endl << " - Поиск элемента в массиве данных при помощи алгоритма интерполяции." << endl;
	cout << " find_item_by_interpolation({ -1, -2, 3, 4, 5 }, 4) -> ";
	cout << sundry::find_item_by_interpolation(std::vector<int>{ -1, -2, 3, 4, 5 }, 4) << endl;

	{
		cout << endl << " - Поиск в списке из чисел последовательного непрерывного интервала(-ов) чисел, сумма которых равна искомому значению." << endl;
		cout << " find_intervals(vector<int>{ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0) -> ";
		auto res = sundry::find_intervals(vector<int>{ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0);
		for (const auto& p : res)
			cout << "(" << p.first << ", " << p.second << ") ";
		cout << endl;
	}

	cout << endl << " - Поиск ближайшего целого числа, которое меньше или больше заданного и состоит из тех же цифр." << endl;
	cout << " find_nearest_number(273145) -> ";
	cout << sundry::find_nearest_number(273145) << endl;

	{
		cout << endl << " - Сортировки методом пузырька." << endl;
		cout << " sort_by_bubble(std::vector<int>{ 2, 3, 1, 5, 4, 7 }) -> ";
		std::list<int> vec{ 2, 3, 1, 5, 4, 7 };
		sundry::sort_by_bubble(vec);
		std::ranges::for_each(std::as_const(vec), [](const auto& n) { cout << n << ", "; });
		cout << endl;
	}

	{
		cout << endl << " - Сортировка методом слияния." << endl;
		cout << " sort_by_merge(std::vector<int>{ 2, 3, 1, 5, 4, 7 }) -> ";
		std::list<int> vec{ 2, 3, 1, 5, 4, 7 };
		sundry::sort_by_merge(vec);
		std::ranges::for_each(vec, [](const auto& n) { std::cout << n << ", "; });
		cout << endl;
	}

	{
		cout << endl << " - Сортировка методом Shell." << endl;
		cout << " sort_by_shell(std::vector<int>{ 2, 3, 1, 5, 4, 7 }) -> ";
		std::list<int> vec{ 2, 3, 1, 5, 4, 7 };
		sundry::sort_by_shell(vec, sundry::SortMethod::SHELL);
		std::ranges::for_each(vec, [](const auto& n) { std::cout << n << ", "; });
		cout << endl;
	}

	{
		cout << endl << " - Сортировка методом отбора." << endl;
		cout << " sort_by_selection(std::vector<int>{ 2, 3, 1, 5, 4, 7 }) -> ";
		std::list<int> vec{ 2, 3, 1, 5, 4, 7 };
		sundry::sort_by_selection(vec);
		std::ranges::for_each(vec, [](const auto& n) { std::cout << n << ", "; });
		cout << endl;
	}

	{
		cout << endl << " - Минимальное количество перестановок." << endl;
		cout << " get_number_permutations(\n";
		cout << "\t{ 10, 31, 15, 22, 14, 17, 16 },\n";
		cout << "\t{ 16, 22, 14, 10, 31, 15, 17 }) -> ";
		auto res = puzzles::get_number_permutations({ 10, 31, 15, 22, 14, 17, 16 }, { 16, 22, 14, 10, 31, 15, 17 });
		std::cout << res << endl;
	}

	cout << endl << " - Олимпиадная задача. См. описание в Demo-Puzzles.ixx." << endl;
	cout << " get_pagebook_number(27, 2, {8,0}) -> ";
	std::cout << puzzles::get_pagebook_number(27, 2, { 8, 0 }) << endl;

	{
		cout << endl << " - Сформировать все возможные уникальные наборы чисел из указанных цифр." << endl;
		cout << " get_combination_numbers(std::vector<int>{ 0, 2, 7 }) -> ";
		auto res = puzzles::get_combination_numbers({ 0, 2, 7 });
		std::cout << "{ ";
		for (auto& e : res)
		{
			std::cout << "{ ";
			std::for_each(e.begin(), e.end(), [](const auto& n) { std::cout << n << " "; });
			std::cout << "} ";
		}
		cout << "}" << endl;
	}

	return 0;
}
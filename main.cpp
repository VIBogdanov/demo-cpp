﻿#include <algorithm>
#include <clocale>
#include <iostream>
#include <list>
#include <vector>
#include <chrono>

import Demo;

int main()
{
	setlocale(LC_CTYPE, "");

	std::cout << std::endl << " - Функция преобразования целого числа в набор цифр." << std::endl;
	std::cout << " inumber_to_digits(12340) -> ";
	std::ranges::for_each(assistools::inumber_to_digits(12340L), [](const auto& n) { std::cout << n << ' '; });
	std::cout << std::endl;

	std::cout << std::endl << " - Функция получения целого числа из набора цифр." << std::endl;
	std::cout << " inumber_from_digits({ 2, 7, 0 }) -> ";
	std::cout << assistools::inumber_from_digits({ 2, 7, 0 }) << std::endl;
	
	std::cout << std::endl << " - Формирует список индексов диапазонов, на которые можно разбить список заданной длины." << std::endl;
	std::cout << " get_ranges_index(50, 10) -> ";
	for (const auto& [_first, _last] : assistools::get_ranges_index(50, 10))
		std::cout << std::format("[{0},{1}) ", _first, _last);
	std::cout << std::endl;
	
	std::cout << std::endl << " - Функция нахождения наибольшего общего делителя двух целых чисел без перебора методом Евклида." << std::endl;
	std::cout << " get_common_divisor(20, 12) -> ";
	if (auto res{ sundry::get_common_divisor(20L, 12L) })
		std::cout << *res;
	else
		std::cout << "Невозможно вычислить общий делитель!";
	std::cout << std::endl;

	std::cout << std::endl << " - Поиск в списке чисел последовательного непрерывного интервала(-ов) чисел, сумма которых равна искомому значению." << std::endl;
	std::cout << " find_intervals({ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0) -> ";
	for (const auto& p : sundry::find_intervals({ 1, -1, 4, 3, 2, 1, -3, 4, 5, -5, 5 }, 0))
		std::cout << std::format("[{0},{1}] ", p.first, p.second);
	std::cout << std::endl;

	std::cout << std::endl << " - Поиск элемента в массиве данных при помощи бинарного алгоритма." << std::endl;
	std::cout << " find_item_by_binary({ -20, 30, 40, 50 }, 30) -> ";
	std::cout << sundry::find_item_by_binary({ -20, 30, 40, 50 }, 30) << std::endl;
	
	std::cout << std::endl << " - Поиск элемента в массиве данных при помощи алгоритма интерполяции." << std::endl;
	std::cout << " find_item_by_interpolation({ -1, -2, 3, 4, 5 }, 4) -> ";
	std::cout << sundry::find_item_by_interpolation({ -1, -2, 3, 4, 5 }, 4) << std::endl;

	std::cout << std::endl << " - Поиск ближайшего целого числа, которое меньше или больше заданного и состоит из тех же цифр." << std::endl;
	std::cout << " find_nearest_number(273145) -> ";
	std::cout << sundry::find_nearest_number(273145L) << std::endl;
	
	{
		std::cout << std::endl << " - Сортировки методом пузырька." << std::endl;
		std::cout << " sort_by_bubble(std::vector<int>{ 2, 3, 1, 5, 4, 7 }) -> ";
		std::list<int> vec{ 2, 3, 1, 5, 4, 7 };
		sundry::sort_by_bubble(vec);
		std::ranges::for_each(std::as_const(vec), [](const auto& n) { std::cout << n << ", "; });
		std::cout << std::endl;
	}
	
	{
		std::cout << std::endl << " - Сортировка методом слияния." << std::endl;
		std::cout << " sort_by_merge(std::vector<int>{ 2, 3, 1, 5, 4, 7 }) -> ";
		std::list<int> vec{ 2, 3, 1, 5, 4, 7 };
		sundry::sort_by_merge(vec);
		std::ranges::for_each(vec, [](const auto& n) { std::cout << n << ", "; });
		std::cout << std::endl;
	}
	
	{
		std::cout << std::endl << " - Сортировка методом Shell." << std::endl;
		std::cout << " sort_by_shell(std::vector<int>{ 2, 3, 1, 5, 4, 7 }) -> ";
		std::list<int> vec{ 2, 3, 1, 5, 4, 7 };
		sundry::sort_by_shell(vec, sundry::SortMethod::FIBONACCI);
		std::ranges::for_each(vec, [](const auto& n) { std::cout << n << ", "; });
		std::cout << std::endl;
	}
	
	{
		std::cout << std::endl << " - Сортировка методом отбора." << std::endl;
		std::cout << " sort_by_selection(std::vector<int>{ 2, 3, 1, 5, 4, 7 }) -> ";
		std::list<int> vec{ 2, 3, 1, 5, 4, 7 };
		sundry::sort_by_selection(vec);
		std::ranges::for_each(vec, [](const auto& n) { std::cout << n << ", "; });
		std::cout << std::endl;
	}

	{
		std::cout << std::endl << " - Минимальное количество перестановок." << std::endl;
		std::cout << " get_number_permutations(\n";
		std::cout << "\t{ 10, 31, 15, 22, 14, 17, 16 },\n";
		std::cout << "\t{ 16, 22, 14, 10, 31, 15, 17 }) -> ";
		auto res = puzzles::get_number_permutations({ 10, 31, 15, 22, 14, 17, 16 }, { 16, 22, 14, 10, 31, 15, 17 });
		std::cout << res << std::endl;
	}
	
	std::cout << std::endl << " - Олимпиадная задача. См. описание в Demo-Puzzles.ixx." << std::endl;
	std::cout << " get_pagebook_number(27, 2, {8,0}) -> ";
	std::cout << puzzles::get_pagebook_number(27, 2, { 8, 0 }) << std::endl;
	
	std::cout << std::endl << " - Сформировать все возможные уникальные наборы чисел из указанных цифр." << std::endl;
	std::cout << " get_combination_numbers_async({ 0, 2, 7 }) -> ";
	for (std::cout << "{ "; auto& list_numbers : puzzles::get_combination_numbers_async(std::list<long>{ 0, 2, 7 }))
	{
		std::cout << "{ ";
		std::ranges::for_each(list_numbers, [](const auto& n) { std::cout << n << " "; });
		std::cout << "} ";
	}
	std::cout << "}" << std::endl;

	return 0;
}
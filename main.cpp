#include "main.h"
#include "src/sundry.h"

using namespace std;

int main()
{
	setlocale(LC_CTYPE, "");

	// Функция нахождения наибольшего общего делителя двух целых чисел без перебора методом Евклида.
	cout << sundry::get_common_divisor(20, 12) << endl;

	return 0;
}
#include <stdio.h>

int main()
{
	int ii = 0;
	double dd;

	for (ii = 0; ii < 10; ++ii){
		dd = 100 * (1<<ii);
		write(1, &dd, sizeof(dd));
	}
}

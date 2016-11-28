#include <stdlib.h>
#include "lib/stats/low.h"

double low::find(struct stock *data, long rows, int days)
{
	double sum = 0, ret = 0;
	int row, start;

	if ((days <= 0) || (rows <= days + 1))
	{
		return ret;
	}

	ret = data[0].low;

	for (row = days - 1; row >= 0; row--)
	{
		if (ret > data[row].low)
		{
			ret = data[row].low;
		}
	}

	return ret;
}

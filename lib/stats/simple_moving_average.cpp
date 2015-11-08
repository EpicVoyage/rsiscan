#include <stdlib.h>
#include "lib/stats/simple_moving_average.h"

double *simple_moving_average::generate(struct stock *data, long rows, int period, long count)
{
	double sum = 0, *ret;
	int row, start;

	if ((count <= 0) || (rows <= period + 1))
	{
		ret = (double *)malloc(sizeof(double));
		ret[0] = 0;
		return ret;
	}

	ret = (double *)malloc(sizeof(double) * (count + 1));
	ret[count] = 0;

	start = (period + count < rows) ? period + count : rows;

	for (row = start - 1; row >= 0; row--)
	{
		sum += data[row].close;
		if (row < start - period)
		{
			sum -= data[row + period].close;
			ret[row] = sum / period;
		}

	}

	return ret;
}

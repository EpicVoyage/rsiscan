#include <math.h>
#include <stdlib.h>
#include "lib/stats/bollinger.h"
#include "lib/stats/simple_moving_average.h"

double *bollinger::bands(struct stock *data, long rows, int period, int deviations, long count)
{
	simple_moving_average sma;
	double sum, *sma_data, *ret;
	int row, y, start;

	if ((count <= 0) || (rows <= period + 1))
	{
		ret = (double *)malloc(sizeof(double));
		ret[0] = 0;
		return ret;
	}

	ret = (double *)malloc(sizeof(double) * (count + 1));
	ret[count] = 0;

	start = (period + count < rows) ? count : rows - period;
	sma_data = sma.generate(data, rows, period, count);

	for (row = start - 1; row >= 0; row--)
	{
		sum = 0;
		for (y = row; y < row + period; y++)
			sum += pow(data[y].close - sma_data[row], 2);

		ret[row] = sqrt(sum / period) * deviations;

	}

	free(sma_data);

	return ret;
}

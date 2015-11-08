#include <stdlib.h>
#include "lib/stats/exponential_moving_average.h"

/* Loop through data and create a exponential moving average */
double *exponential_moving_average::generate(struct stock *data, long rows, int period, long count)
{
	double alpha, ema, *ret;
	int row, start;

	if ((count <= 0) || (rows <= period + 1))
	{
		ret = (double *)malloc(sizeof(double));
		ret[0] = 0;
		return ret;
	}

	ret = (double *)malloc(sizeof(double) * (count + 1));
	ret[count] = 0;

	alpha = 2.0 / (period + 1);
	start = (period + count < rows) ? period + count : rows - 1;

	ema = data[start].close;
	for (row = start - 1; row >= 0; row--)
	{
		ema = alpha * data[row].close + (1 - alpha) * ema;
		if (row < start - period)
			ret[row] = ema;

	}

	return ret;
}

/* Loop through data and create a exponential moving average */
double *exponential_moving_average::generate_d(double *data, long rows, int period, long count)
{
	double alpha, ema, *ret;
	int row, start;

	if ((count <= 0) || (rows <= period + 1))
	{
		ret = (double *)malloc(sizeof(double));
		ret[0] = 0;
		return ret;
	}

	ret = (double *)malloc(sizeof(double) * (count + 1));
	ret[count] = 0;

	for (row = 0; row <= count; row++)
		ret[row] = 0;

	alpha = 2.0 / (period + 1);
	start = (period + count < rows) ? period + count : rows;

	ema = data[start];
	for (row = start - 1; row >= 0; row--)
	{
		ema = alpha * data[row] + (1 - alpha) * ema;
		if (row < start - period)
			ret[row] = ema;
	}

	return ret;
}

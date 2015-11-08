#include <stdlib.h>
#include "lib/stats/moving_average_convergence_divergence.h"
#include "lib/stats/exponential_moving_average.h"

/* Loop through data and create Moving Average Convergence/Divergence data from EMAs */
double *moving_average_convergence_divergence::generate(struct stock *data, long rows, int fast, int slow, long count)
{
	exponential_moving_average ema;
	double *ema12, *ema26, *ret;
	int row, start;

	if ((count <= 0) || (rows <= slow + 1))
	{
		ret = (double *)malloc(sizeof(double));
		ret[0] = 0;
		return ret;
	}

	ret = (double *)malloc(sizeof(double) * (count + 1));
	ret[count] = 0;

	start = (slow + count < rows) ? count : rows - slow;
	ema12 = ema.generate(data, rows, fast, count);
	ema26 = ema.generate(data, rows, slow, count);

	for (row = start - 1; row >= 0; row--)
		ret[row] = ema12[row] - ema26[row];

	free(ema12);
	free(ema26);

	return ret;
}

/* Loop through data and create Moving Average Convergence/Divergence data from EMAs */
double *moving_average_convergence_divergence::histogram(struct stock *data, long rows, int fast, int slow, int avg, long count)
{
	exponential_moving_average ema;
	double *macd, *ema_data, *ret;
	int row, start;

	if ((count <= 0) || (rows <= slow + 1))
	{
		ret = (double *)malloc(sizeof(double));
		ret[0] = 0;
		return ret;
	}

	ret = (double *)malloc(sizeof(double) * (count + 1));
	ret[count] = 0;

	start = (slow + count + avg < rows) ? count : rows - slow;
	macd = this->generate(data, rows, fast, slow, count + avg);
	ema_data = ema.generate_d(macd, count + avg, avg, count);

	for (row = start - 1; row >= 0; row--)
		ret[row] = macd[row] - ema_data[row];

	free(macd);
	free(ema_data);

	return ret;
}

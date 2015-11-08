#include <stdlib.h>
#include "lib/stats/relative_strength_index.h"

double *relative_strength_index::generate(struct stock *data, long rows, int period, long count)
{
	double change, ag, al, up, down, gains = 0, losses = 0;
	double prev_gain = 0, prev_loss = 0, rs, rsi = 0, *ret;
	int row, start;

	if ((count <= 0) || (rows <= period + 1))
	{
		ret = (double *)malloc(sizeof(double));
		ret[0] = 0;
		return ret;
	}

	ret = (double *)malloc(sizeof(double) * (count + 1));
	ret[count] = 0;

	start = ((2 * period) + count < rows - 1) ? (2 * period) + count : rows - 1;

	for (row = start - 1; row >= 0; row--)
	{
		change = data[row].close - data[row + 1].close;
		if (change < 0)
		{
			down = -change;
			losses += down;
			up = 0;
		}
		else
		{
			up = change;
			gains += up;
			down = 0;
		}

		if (row < start - period)
		{
			ag = (prev_gain * (period - 1) + up) / period;
			al = (prev_loss * (period - 1) + down) / period;
		}
		else
		{
			ag = gains / period;
			al = losses / period;
		}

		rs = ag / al;
		rsi = 100 - (100 / (1 + rs));

		if (row <= start - period)
		{
			change = data[row + period - 1].close - data[row + period].close;
			if (change < 0)
				losses += change;
			else
				gains -= change;
		}

		if (row < count)
			ret[row] = rsi;

		prev_gain = ag;
		prev_loss = al;
	}

	ret[0] = rsi;

	return ret;
}

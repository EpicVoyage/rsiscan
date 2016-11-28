#include <stdlib.h>
#include "lib/stats/high.h"

double high::find(struct stock *data, long rows, int days)
{
	double sum = 0, ret = 0;
	int row, start;

	if ((days <= 0) || (rows <= days + 1))
	{
		return ret;
	}

	ret = data[0].high;

	for (row = days - 1; row >= 0; row--)
	{
		if (ret < data[row].high)
		{
			ret = data[row].high;
		}
	}

	return ret;
}

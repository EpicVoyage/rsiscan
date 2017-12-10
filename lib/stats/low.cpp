#include <stdlib.h>
#include "lib/stats/low.h"

double low::find(const stockinfo &data, int days)
{
	long rows = data.length();
	double ret = 0;
	int row;

	if ((days <= 0) || (rows <= days + 1))
	{
		return ret;
	}

	ret = data[0]->low;

	for (row = days - 1; row >= 0; row--)
	{
		if (ret > data[row]->low)
		{
			ret = data[row]->low;
		}
	}

	return ret;
}

#include <stdlib.h>
#include "lib/stats/high.h"

double high::find(const stockinfo &data, int days)
{
	long rows = data.length();
	double ret = 0;
	int row;

	if ((days <= 0) || (rows <= days + 1))
	{
		return ret;
	}

	ret = data[0]->high;

	for (row = days - 1; row >= 0; row--)
	{
		if (ret < data[row]->high)
		{
			ret = data[row]->high;
		}
	}

	return ret;
}

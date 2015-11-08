#include "lib/stock.h"

class bollinger
{
	public:
		double *bands(struct stock *data, long rows, int period = 20, int deviations = 2, long count = 0);
};

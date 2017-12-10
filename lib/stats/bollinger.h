#include "lib/stock.h"

class bollinger
{
	public:
		double *bands(const stockinfo &data, int period = 20, int deviations = 2, long count = 0);
};

#include "lib/stock.h"

class simple_moving_average
{
	public:
		double *generate(const stockinfo &data, int period = 20, long count = 0);
};

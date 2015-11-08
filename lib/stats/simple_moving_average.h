#include "lib/stock.h"

class simple_moving_average
{
	public:
		double *generate(struct stock *data, long rows, int period = 20, long count = 0);
};

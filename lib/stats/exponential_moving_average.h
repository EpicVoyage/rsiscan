#include "lib/stock.h"

class exponential_moving_average
{
	public:
		double *generate(struct stock *data, long rows, int period, long count);
		double *generate_d(double *data, long rows, int period, long count);
};

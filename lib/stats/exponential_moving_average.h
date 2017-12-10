#include "lib/stock.h"

class exponential_moving_average
{
	public:
		double *generate(const stockinfo &data, int period, long count);
		double *generate_d(const double *data, long rows, int period, long count);
};

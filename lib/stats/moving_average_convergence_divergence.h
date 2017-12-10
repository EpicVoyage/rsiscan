#include "lib/stock.h"

class moving_average_convergence_divergence
{
	public:
		double *generate(const stockinfo &data, int fast, int slow, long count);
		double *histogram(const stockinfo &data, int fast, int slow, int avg, long count);
};

#include "lib/stock.h"

class moving_average_convergence_divergence
{
	public:
		double *generate(struct stock *data, long rows, int fast, int slow, long count);
		double *histogram(struct stock *data, long rows, int fast, int slow, int avg, long count);
};

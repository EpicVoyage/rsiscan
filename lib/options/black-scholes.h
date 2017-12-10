#include "lib/stock.h"

class black_scholes
{
	public:
		double cumulative_normal_distribution(double X);
		double calculate(char CallPutFlag, double S, double X, double T, double r, double v);
};

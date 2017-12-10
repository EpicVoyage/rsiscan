/**
 * @link http://www.espenhaug.com/black_scholes.html
 *
 * Rough approximation: call = put = StockPrice * 0.4 * volatility * Sqrt(Time)
 *
 * @todo Interest rates are included in this. Look at dividends.
 */
#include "black-scholes.h"
#include <math.h>

#ifndef Pi
	#define Pi 3.141592653589793238462643
#endif

// The cumulative normal distribution function
double black_scholes::cumulative_normal_distribution(double X)
{
	double const a1 = 0.31938153, a2 = -0.356563782, a3 = 1.781477937;
	double const a4 = -1.821255978, a5 = 1.330274429;
	double L, K, w;

	L = fabs(X);
	K = 1.0 / (1.0 + 0.2316419 * L);
	w = 1.0 - 1.0 / sqrt(2 * Pi) * exp(-L * L / 2) * (a1 * K + a2 * K * K + a3 * pow(K, 3) + a4 * pow(K,4) + a5 * pow(K,5));

	if (X < 0) {
		w = 1.0 - w;
	}

	return w;
}

/**
 * The Black and Scholes (1973) Stock option formula
 *
 * S = Stock price
 * X = Strike price
 * T = Years to maturity
 * r = Risk-free rate
 * v = Volatility
 */
double black_scholes::calculate(char CallPutFlag, double S, double X, double T, double r, double v)
{
	double d1, d2;

	d1 = (log(S / X) + (r + v * v / 2) * T) / (v * sqrt(T));
	d2 = d1 - v * sqrt(T);

	if (CallPutFlag == 'p')
		return X * exp(-r * T) * cumulative_normal_distribution(-d2) - S * cumulative_normal_distribution(-d1);
	else // if (CallPutFlag == 'c')
		return S * cumulative_normal_distribution(d1) - X * exp(-r * T) * cumulative_normal_distribution(d2);
}

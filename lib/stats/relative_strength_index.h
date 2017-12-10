#include "lib/stock.h"

class relative_strength_index
{
public:
	double *generate(const stockinfo &data, int period = 14, long count = 1);
};

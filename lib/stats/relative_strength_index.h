#include "lib/stock.h"

class relative_strength_index
{
public:
	double *generate(struct stock *data, long rows, int period = 14, long count = 1);
};

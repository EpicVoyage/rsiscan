#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <boost/log/trivial.hpp>
#include "lib/comma_separated_values.h"

/**
 * Split CSV data into a multi-dimensional array
 *
 * @note Rows must currently be separated by "\r\n".
 * @note 6 columns expected: date, open, high, low, close, volume.
 *
 * @return stock** and sets rows
 */
struct stock *comma_separated_values::parse(const char *block, long *rows)
{
	char *data, *line, *col, *ptr, *ptr_orig;
	struct stock *ret;
	int cols;
	long x;

	ptr_orig = ptr = (char *)malloc(strlen(block) + 1);
	strcpy(ptr, block);

	cols = *rows = 0;
	col = line = strsep(&ptr, "\r\n");
	while (*col != '\0')
		cols += (*col++ == ',');

	if (cols < 5)
	{
		BOOST_LOG_TRIVIAL(error) << "Required: 5 columns. Found: " << cols;
		fprintf(stderr, "Error: Not enough columns!\n");
		free(ptr_orig);
		return nullptr;
	}

	do
	{
		if ((isdigit(*line)) && (strchr(line, ',')))
			(*rows)++;
	} while ((line = strsep(&ptr, "\r\n")));
	free(ptr_orig);

	BOOST_LOG_TRIVIAL(trace) << "CSV rows found: " << *rows;

	data = (char *)malloc(strlen(block) + 1);
	strcpy(data, block);

	ret = (struct stock *)malloc((*rows) * sizeof(struct stock));
	for (x = 0; x < *rows; x++)
	{
		ret[x].date = nullptr;
		ret[x].open = ret[x].high = ret[x].low = ret[x].close = 0;
		ret[x].volume = 0;
	}

	x = 0;
	while ((line = strsep(&data, "\r\n")))
	{
		if ((!isdigit(*line)) || (!strchr(line, ',')))
			continue;

		ptr = (char *)malloc(strlen(line) + 1);
		strcpy(ptr, line);

		// Hack: We are going to use this ptr as our "date" column.
		ret[x].date = strsep(&ptr, ",");
		ret[x].open = atof(strsep(&ptr, ","));
		ret[x].high = atof(strsep(&ptr, ","));
		ret[x].low = atof(strsep(&ptr, ","));
		ret[x].close = atof(strsep(&ptr, ","));
		ret[x].volume = strtol(strsep(&ptr, ","), NULL, 10);

		x++;
	}

	free(data);
	return ret;
}

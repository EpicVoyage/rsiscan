#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "lib/comma_separated_values.h"

/**
 * Split CSV data into a multi-dimensional array
 * Returns stock** and sets rows/cols
 */
struct stock *comma_separated_values::parse(char *block, long *rows)
{
	char *line, *row, *col, *ptr;
	struct stock *ret;
	int cols;
	long x;

	ptr = (char *)malloc(strlen(block) + 1);
	strcpy(ptr, block);
	cols = *rows = 0;

	col = line = strsep(&ptr, "\r\n");
	while (*col != '\0')
		cols += (*col++ == ',');

	if (cols < 5)
	{
		fprintf(stderr, "Error: Not enough columns!\n");
		free(block);
		free(ptr);
		return NULL;
	}

	do
	{
		if ((isdigit(*line)) && (strchr(line, ',')))
			(*rows)++;
	} while (line = strsep(&ptr, "\r\n"));
	free(ptr);

	ret = (struct stock *)malloc((*rows) * sizeof(struct stock));
	for (x = 0; x < *rows; x++)
	{
		ret[x].date = NULL;
		ret[x].open = ret[x].high = ret[x].low = ret[x].close = 0;
		ret[x].volume = 0;
	}

	x = 0;
	while (line = strsep(&block, "\r\n"))
	{
		if ((!isdigit(*line)) || (!strchr(line, ',')))
			continue;

		ptr = (char *)malloc(strlen(line) + 1);
		strcpy(ptr, line);

		ret[x].date = strsep(&ptr, ",");
		ret[x].open = atof(strsep(&ptr, ","));
		ret[x].high = atof(strsep(&ptr, ","));
		ret[x].low = atof(strsep(&ptr, ","));
		ret[x].close = atof(strsep(&ptr, ","));
		ret[x].volume = strtol(strsep(&ptr, ","), NULL, 10);

		x++;
	}

	return ret;
}

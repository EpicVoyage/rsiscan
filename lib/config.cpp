#include <sys/stat.h>
#include <boost/log/trivial.hpp>
#include "lib/config.h"

config::config(): save_config(true) {
	home = getenv("HOME");
	create_dir(home);

	config_dir = (char *)malloc(strlen(home) + 10);
	old_dir = (char *)malloc(strlen(home) + 14);
	list_dir = (char *)malloc(strlen(home) + 16);
	stock_dir = (char *)malloc(strlen(home) + 15);

	sprintf(config_dir, "%s/.rsiscan", (char *)home);
	create_dir(config_dir);

	sprintf(old_dir, "%s/old", (char *)config_dir);
	create_dir(old_dir);

	sprintf(list_dir, "%s/lists", (char *)config_dir);
	create_dir(list_dir);

	sprintf(stock_dir, "%s/data", (char *)config_dir);
	create_dir(stock_dir);

	return;
}

config::~config() {
	long x;

	free(config_dir);
	free(old_dir);
	free(list_dir);
	free(stock_dir);

	for (x = 0; x < tickers.size(); x++)
		free(tickers[x]);
}

/* Attempt to create a directory. If we fail, don't try to save data */
void config::create_dir(const char *path)
{
	struct stat buf;

	if ((save_config) && (stat(path, &buf)))
	{
		if (mkdir(path, S_IRWXU))
		{
			fprintf(stderr, "Error creating directory: %s\n", path);
			save_config = false;
		}
	}

	return;
}

void config::delist(const char *identifier) {
	char *filename, *tmp;
	struct stat buf;

	if (!save_config)
		return;

	filename = get_filename(identifier);

	tmp = (char *)malloc(strlen(old_dir) + strlen(filename) + 6);
	sprintf(tmp, "%s/%s.csv", (char *)old_dir, filename);


	BOOST_LOG_TRIVIAL(trace) << "Removing stock data: " << filename;

	if (stat(filename, &buf) == 0)
		if (rename(filename, tmp) == -1)
			fprintf(stderr, "Error moving %s to %s: %s\n", filename, tmp, strerror(errno));

	free(filename);
	free(tmp);

	return;
}

char *config::get_filename(const char *identifier) {
	char *tkr, *tmp, *filename;

	tkr = (char *)malloc(strlen(identifier) + 1);
	strcpy(tkr, identifier);
	tmp = tkr;
	while (*tmp)
	{
		*tmp = tolower(*tmp);
		tmp++;
	}
	tmp = NULL;

	filename = (char *)malloc(strlen(stock_dir) + strlen(tkr) + 6);
	sprintf(filename, "%s/%s.csv", (char *)stock_dir, tkr);

	return filename;
}

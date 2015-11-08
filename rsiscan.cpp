/***********************************************************************
 * Programmer:	Daga <daga@epicvoyage.org>
 * Started:	June 19, 2007
 *
 ***********************************************************************
 * Sample runs:	$ ./rsiscan YHOO GOOG
 * 		GOOG:
 *			Daily RSI: 78.03 > 70
 *
 * 		$ ./rsiscan --up=1 MRVL BRCM VTSS INTC STM CNXT PMCS MXIM
 *		+1.17: MRVL
 *
 ***********************************************************************
 * Unless you want to use "intraday" values, you will have to run this
 * program after midnight in order to permanently store the previous
 * day's closing stock information. It's a safety feature.
 *
 * Released under the terms of the GNU GPL v2, and I'd be interested in
 * seeing what you base off of this.
 ***********************************************************************
 */

/* Unlock some more functions. */
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#define _XOPEN_SOURCE

/* Universal headers */
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>
#include "lib/http.h"
#include "lib/comma_separated_values.h"
#include "lib/stock.h"
#include "lib/stats/moving_average_convergence_divergence.h"
#include "lib/stats/relative_strength_index.h"
#include "lib/stats/simple_moving_average.h"
#include "lib/stats/bollinger.h"

/* Linux-only headers */
#ifndef WIN32
#       include <sys/types.h>
#       include <sys/stat.h>
#       include <dirent.h>
#       include <stdlib.h>
#       include <string.h>
#       include <unistd.h>
#       include <stdio.h>
#       include <time.h>
#endif

#define HIGHERHIGH 1
#define LOWERHIGH 2
#define HIGHERLOW 3
#define LOWERLOW 4

/* "Custom" data types */
enum server {yahoo = 0, google = 1, invest = 2};
//enum bool {false = 0, true = 1};

/* Servers to query for CSV data */
const char *servers[] = { "ichart.finance.yahoo.com", "download.finance.yahoo.com",
			  "finance.google.com",
			  "dunno"
			};

const char *i_servers[] = { "download.finance.yahoo.com",
			    "",
			    ""
			  };

/* 1.1 - Basic program functionality */
void create_config();
void create_dir(char *path);
void read_args(int argc, char **argv);
void print_help(char *prog);
void cleanup();
void nosig();
void sig();

/* 1.2 - Get/Save/Read stock data (cache) */
void save_to_file(char *ticker, struct stock *data, long rows);
char *load_from_file(char *ticker);
void delist(char *ticker);
char *download_eod_data(char *ticker, time_t from);
char *download_intraday_data();
void intraday_request(FILE *out);
char *get_path(char *ticker, time_t from);
char *retrieve_data(FILE *in, char *ticker);
void recursive_free(struct stock *data, long rows);

/* 1.3 - Functions which extrapolate from data */
void update_tickers();
void load_ticker(char *ticker);
time_t get_last_date(struct stock *data, long rows);
long average_volume(struct stock *data, long rows, long n = 10);
struct stock *make_weekly(struct stock *data, long rows, long *w_rows);
void diverge(char *ticker, struct stock *data, long rows);
void tails(char *ticker, struct stock *data, long rows);
void analyze(char *ticker, struct stock *data, long rows);
int divergence(double *values, long rows, long reset_high, long reset_low, long *pos);
bool chart_patterns(struct stock *data, long rows, bool print, const char *period);

/* Global variables */
bool verbose, save_config, intraday, find_divergence, find_tails;
char *home, *old_dir, *list_dir, *config_dir, **tickers;
int percent, seconds;
long ticker_count;
enum server source;

/*****************************
 * 1.0 - Program entry point
 *****************************/
int main(int argc, char **argv)
{
	verbose = intraday = false;

	ticker_count = -1;
	percent = 0;

	config_dir = NULL;
	list_dir = NULL;
	old_dir = NULL;
	tickers = (char **)malloc(sizeof(char *));
	tickers[0] = NULL;
	find_tails = false;
	find_divergence = false;

	source = yahoo;

	create_config();
	read_args(argc, argv);
	update_tickers();

	cleanup();

	return 0;
}

/*************************************************
 * 1.1 - Basic program functionality
 *************************************************
 * Create save directory for downloaded CSV data */
void create_config()
{
	save_config = true;

	home = getenv("HOME");
	create_dir(home);

	old_dir = (char *)malloc(strlen(home) + 19);
	list_dir = (char *)malloc(strlen(home) + 21);
	config_dir = (char *)malloc(strlen(home) + 20);

	sprintf(config_dir, "%s/.daga", home);
	create_dir(config_dir);

	sprintf(config_dir, "%s/.daga/rsiscan", home);
	create_dir(config_dir);

	sprintf(old_dir, "%s/.daga/rsiscan/old", home);
	create_dir(old_dir);

	sprintf(list_dir, "%s/.daga/rsiscan/lists", home);
	create_dir(list_dir);

	sprintf(config_dir, "%s/.daga/rsiscan/data", home);
	create_dir(config_dir);

	return;
}

/* Attempt to create a directory. If we fail, don't try to save data */
void create_dir(char *path)
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

/* Parse command line options */
void read_args(int argc, char **argv)
{
	int x;

	for (x = 1; x < argc; x++)
	{
		if ((strcmp(argv[x], "-v") == 0) || (strcmp(argv[x], "--verbose") == 0))
			verbose = true;
		else if (strcmp(argv[x], "--yahoo") == 0)
			source = yahoo;
		else if (strcmp(argv[x], "--google") == 0)
			source = google;
		/*else if (strcmp(argv[x], "--invest") == 0)
			source = invest;*/
		else if ((strncmp(argv[x], "--up=", 5) == 0) && (strlen(argv[x]) > 5))
			percent = atoi(argv[x] + 5);
		else if ((strncmp(argv[x], "--sleep=", 8) == 0) && (strlen(argv[x]) > 8))
			seconds = atoi(argv[x] + 8);
		else if (strcmp(argv[x], "--intraday") == 0)
			intraday = true;
		else if (strcmp(argv[x], "--divergence") == 0)
			find_divergence = true;
		else if (strcmp(argv[x], "--tails") == 0)
			find_tails = true;
		else if (*argv[x] != '-')
		{
			ticker_count++;
			tickers = (char **)realloc(tickers, (ticker_count + 1) * sizeof(char *));
			tickers[ticker_count] = (char *)malloc(strlen(argv[x]) + 1);
			strcpy(tickers[ticker_count], argv[x]);
		}
		else
			print_help(argv[0]);
	}

	if ((intraday) && (source != yahoo))
	{
		fprintf(stderr, "Warning: Intraday quotes require Yahoo for now. Forcing server...\n");
		source = yahoo;
	}

	return;
}


/* Print program usage and exit */
void print_help(char *prog)
{
	printf("Usage: %s [--switch] [--switch] [TKR1] [TKR2] [etc.]\n\n", prog);

	printf("    --yahoo      Download data from %s [default]\n", servers[yahoo]);
	printf("    --google     Download data from %s\n", servers[google]);
	/*printf("    --invest     Download data from InvestorLink [commercial]\n");*/
	printf("    --up=##      Only show stocks that have moved up ## percent\n");
	printf("    --sleep=##   Sleep ## seconds between server requests [default: 5]\n");
	printf("    --intraday   Temporarily use intraday quotes for today's closing information\n");
	printf("    --divergence Look for divergences in the RSI, MACD, and MACD histogram\n");
	printf("    --tails      Look for lows outside BB, with closes inside 3 out of 4 days\n");
	printf("    --verbose    Print debug data along the way\n");
	printf("    --help       Print this text and exit\n\n");

	printf("Tickers listed on the command line will be added to the local cache and used for\n");
	printf("future runs when you don't specify any.\n\n");

	printf("We automatically filter out stocks with an average trading volume under one\n");
	printf("million shares per day over the past two weeks.\n\n");

	printf("This program will perform an analysis on the stocks you specify. It is *NOT*\n");
	printf("advice and should not be used in place of actually talking to your broker. It\n");
	printf("simply tries to look for anomalies in large amounts of data, but is not\n");
	printf("guaranteed to even do that. Use at your own risk.\n\n");

	cleanup();
	exit(0);
}

/* Free allocated global memory */
void cleanup()
{
	long x;

	if (config_dir != NULL)
		free(config_dir);

	if (old_dir != NULL)
		free(old_dir);

	if (list_dir != NULL)
		free(list_dir);

	if (tickers[0] != NULL)
		for (x = 0; x <= ticker_count; x++)
			free(tickers[x]);

	free(tickers);

	return;
}

void nosig()
{
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	return;
}

void sig()
{
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	return;
}

/***************************************************
 * 1.2 - Get/Save/Read stock data (cache)
 ***************************************************
 * Save (cache) ticker data to files for later use */
void save_to_file(char *ticker, struct stock *data, long rows)
{
	char *filename, *tkr, *tmp;
	long x, y;
	FILE *wr;

	if (!save_config)
		return;

	tkr = (char *)malloc(strlen(ticker) + 1);
	strcpy(tkr, ticker);
	tmp = tkr;
	while ((*tmp = tolower(*tmp)) && *tmp++);

	filename = (char *)malloc(strlen(config_dir) + strlen(tkr) + 6);
	sprintf(filename, "%s/%s.csv", config_dir, tkr);

	nosig();
	wr = fopen(filename, "w");
	for (x = 0; x < rows; x++)
	{
		if (data[x].date)
			fprintf(wr, "%s,%g,%g,%g,%g,%li\n", data[x].date, data[x].open, data[x].high, data[x].low, data[x].close, data[x].volume);
	}
	fclose(wr);
	sig();

	free(filename);
	free(tkr);

	return;
}

/* Load ticker data from files */
char *load_from_file(char *ticker)
{
	char *filename, *tkr, *tmp, *ret = NULL;
	struct stat buf;
	long x, y;
	FILE *re;

	if (!save_config)
		return NULL;

	tkr = (char *)malloc(strlen(ticker) + 1);
	strcpy(tkr, ticker);
	tmp = tkr;
	while (*tmp)
	{
		*tmp = tolower(*tmp);
		*tmp++;
	}
	tmp = NULL;

	filename = (char *)malloc(strlen(config_dir) + strlen(tkr) + 6);
	sprintf(filename, "%s/%s.csv", config_dir, tkr);

	if (stat(filename, &buf))
		return NULL;

	re = fopen(filename, "r");
	tmp = (char *)malloc(2048);
	while (fgets(tmp, 2048, re) != NULL)
	{
		if (ret == NULL)
		{
			ret = (char *)malloc(strlen(tmp) + 1);
			*ret = '\0';
		}
		else
			ret = (char *)realloc(ret, strlen(ret) + strlen(tmp) + 1);

		strcat(ret, tmp);
	}

	fclose(re);
	free(filename);
	free(tkr);
	free(tmp);

	return ret;
}

void delist(char *ticker)
{
	char *filename, *tkr, *tmp;
	struct stat buf;

	if (!save_config)
		return;

	tkr = (char *)malloc(strlen(ticker) + 1);
	strcpy(tkr, ticker);
	tmp = tkr;
	while (*tmp = tolower(*tmp++));
	tmp = NULL;

	filename = (char *)malloc(strlen(config_dir) + strlen(tkr) + 6);
	sprintf(filename, "%s/%s.csv", config_dir, tkr);

	tmp = (char *)malloc(strlen(old_dir) + strlen(tkr) + 6);
	sprintf(tmp, "%s/%s.csv", old_dir, tkr);


	if (stat(filename, &buf) == 0)
		if (rename(filename, tmp) == -1)
			fprintf(stderr, "Error moving %s to %s: %s\n", filename, tmp, strerror(errno));

	free(filename);
	free(tmp);

	return;
}

/* Download End of Day ticker data */
char *download_eod_data(char *ticker, time_t from)
{
	char *ret, *path = get_path(ticker, from);
	http h;
	h.yahoo_bug = (source == yahoo);
	ret = h.retrieve((char *)servers[source], path);

	free(path);
	sleep(seconds);
	return ret;
}

/* Download intrday ticker data */
char *download_intraday_data()
{
	struct hostent *record;
	struct sockaddr_in sin;
	char *ptr, *ret = NULL;
	int sock, conn;
	FILE *wr, *re;

	if ((record = gethostbyname(i_servers[source])) == NULL)
	{
		fprintf(stderr, "DNS lookup for %s failed!\n", i_servers[source]);
		cleanup();
		exit(1);
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(80);
	sin.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)record->h_addr));

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		fprintf(stderr, "Failed to open a socket!\n");
		cleanup();
		exit(1);
	}

	if ((conn = connect(sock, (struct sockaddr *)&sin, sizeof(sin))) < 0)
	{
		fprintf(stderr, "Error connecting to %s: %s\n", i_servers[source], strerror(errno));
		return NULL;
	}

	wr = fdopen(sock, "w");
	re = fdopen(sock, "r");

	intraday_request(wr);
	ret = retrieve_data(re, NULL);

	printf("-%s-\n", ret);
	free(ret);
	ret = NULL;

	/*if ((ret != NULL) && (strcasestr(ret, "Sorry, the page you requested was not found.")))
	{
		printf("Error: %s - No data found, delisting stock\n", ticker);
		delist(ticker);
		free(ret);
		ret = NULL;
	}*/

	fclose(wr);
	fclose(re);
	close(sock);

	sleep(seconds);

	return ret;
}

char *get_path(char *ticker, time_t from) {
	struct tm *date_start, date_finish;
	time_t epoch;
	char *ret = (char *)malloc(strlen(ticker) + 64);

	time(&epoch);
	memcpy(&date_finish, localtime(&epoch), sizeof(struct tm));
	epoch = ((from == 0) ? epoch - 31536000 : from + 86400);
	date_start = localtime(&epoch);

	if (source == yahoo)
		sprintf(ret, "/table.csv?s=%s&a=%i&b=%i&c=%i&d=%i&e=%i&f=%i&g=d&ignore=.csv", ticker,
			date_start->tm_mon, date_start->tm_mday, date_start->tm_year + 1900,
			date_finish.tm_mon, date_finish.tm_mday, date_finish.tm_year + 1900);
	else if (source == google)
		sprintf(ret, "/finance/historical?q=%s&output=csv", ticker);

	return ret;
}

/* Request today's information (all of it) */
void intraday_request(FILE *out)
{
	long x, stop;

	stop = (ticker_count < 100) ? ticker_count : 100;
	if (source == yahoo)
	{
		fprintf(out, "GET /d/quotes.csv?");
		for (x = 0; x <= stop; x++)
			fprintf(out, "s=%s&", tickers[x]);
		fprintf(out, "f=sd1ohgl1v&e=.csv HTTP/1.1\r\n");
	}
	else
	{
		printf("How did you even get here? Yahoo is the only server supported right now!\n");
		return;
	}

	fprintf(out, "Host: %s\r\n", servers[source]);
	fprintf(out, "Connection: close\r\n\r\n", servers[source]);

	fflush(out);

	return;
}

/* Read back CSV data from server */
char *retrieve_data(FILE *in, char *ticker)
{
	bool chunked = false, start = false;
	long x, len, size, content = 0;
	char *read, *tmp, *ret = NULL;

	size = 2048;
	read = (char *)malloc(size);
	while (fgets(read, size, in) != NULL)
	{
		if (!start)
			chunked = ((char *)strcasestr(read, "Transfer-Encoding: chunked") == NULL);

		if ((!start) && (strncasecmp(read, "Content-Length: ", 16) == 0) && (isdigit(*(read + 16))))
			content = strtol(read + 16, NULL, 10);

		if ((*read == '\r') || (*read == '\n'))
		{
			if ((content) && (ret == NULL))
			{
				ret = (char *)malloc(content + 1);
				if ((x = fread(ret, 1, content, in)) < content)
				{
					*(ret + x + 1) = 0;
					start = true;
				}
				else
				{
					*(ret + x + 1) = 0;
					break;
				}
			}
			else if (chunked)
			{
				if (fgets(read, size, in) != NULL)
				{
					len = strtol(read, NULL, 16);

					/* Why is Yahoo! off by 50 characters? */
					if (source == yahoo)
						len -= 50;

					if (len <= 0)
						continue;

					if (ret == NULL)
					{
						ret = (char *)malloc(len + 1);
						*ret = '\0';
					}
					else
						ret = (char *)realloc(ret, strlen(ret) + len + 1);

					if (len >= size)
					{
						size = len + 1;
						read = (char *)realloc(read, size);
					}

					fgets(read, size, in);
					if (x = fread(read, 1, len, in))
					{
						*(read + x) = 0;
						strcat(ret, read);
					}
				}
			}
			else
				start = true;
		}
		else if ((*read == '0') && (!strchr(read, ',')))
		{
			if (ret != NULL)
			{
				tmp = ret + strlen(ret) - 1;
				if (*tmp == '\n') *tmp = '\0';
				break;
			}
		}
		else if (start)
		{
			if (ret == NULL)
			{
				ret = (char *)malloc(strlen(read) + 1);
				*ret = '\0';
			}
			else
				ret = (char *)realloc(ret, strlen(ret) + strlen(read) + 1);

			strcat(ret, read);
		}
	}

	free(read);

	if ((ret == NULL) && (ticker != NULL))
		fprintf(stderr, "Error: did not retrieve any data for %s!\n", ticker);

	return ret;
}

/* Recursively free data variables (one multi-dimensional array) */
void recursive_free(struct stock *data, long rows)
{
	long x, y;

	for (x = 0; x < rows; x++)
		if (data[x].date)
			free(data[x].date);
	free(data);

	return;
}

/****************************************************
 * 1.3 - Functions which extrapolate from data
 ****************************************************
 * Call load_ticker() for each stock locally cached */
void update_tickers()
{
	struct dirent *file;
	DIR *saved;
	char *tmp;
	long x;

	if ((save_config) && (tickers[0] == NULL))
	{
		if ((saved = opendir(config_dir)) == NULL)
		{
			fprintf(stderr, "Error opening %s: %s\n", config_dir, strerror(errno));
			return;
		}

		while (file = readdir(saved))
		{
			if (*(file->d_name) == '.')
				continue;

			tmp = file->d_name;
			while ((*tmp != '\0') && (*tmp != '.'))
				*tmp++ = toupper(*tmp);
			*tmp = 0;

			ticker_count++;
			tickers = (char **)realloc(tickers, (ticker_count + 1) * sizeof(char *));
			tickers[ticker_count] = (char *)malloc(strlen(file->d_name) + 1);
			strcpy(tickers[ticker_count], file->d_name);
		}

		closedir(saved);
	}

	if (intraday)
		download_intraday_data();

	for (x = 0; x <= ticker_count; x++)
		load_ticker(tickers[x]);

	return;
}

/* Load ticker data (file, then internet) and parse */
void load_ticker(char *ticker)
{
	long x, len, rows, w_rows, vol;
	char *tmp, *block, *blocknew;
	struct stock *data, *weekly;
	double rsi, weekly_rsi;
	comma_separated_values csv;
	time_t from;

	if ((block = load_from_file(ticker)) == NULL)
		if ((block = download_eod_data(ticker, 0)) == NULL)
			return;

	len = strlen(block);
	data = csv.parse(block, (long *)&rows);

	if ((from = get_last_date(data, rows)) != 0)
	{
		if ((blocknew = download_eod_data(ticker, from)) != NULL)
		{
			for (x = 0; x < len; x++)
				if (*(block + x) == '\0') *(block + x) = '\n';

			tmp = (char *)malloc(strlen(block) + strlen(blocknew) + 2);
			sprintf(tmp, "%s\n%s", blocknew, block);

			free(blocknew);
			recursive_free(data, rows);

			data = csv.parse(tmp, (long *)&rows);
		}
	}

	save_to_file(ticker, data, rows);

	vol = average_volume(data, rows);
	if (find_divergence)
		diverge(tickers[x], data, rows);
	else if (find_tails)
		tails(tickers[x], data, rows);
	else if (vol > 1000000)
		analyze(ticker, data, rows);
	else if (verbose)
		printf("%s: volume = %li\n", ticker, vol);

	recursive_free(data, rows);

	return;
}

/* Get the last date we have data for, skip weekends */
time_t get_last_date(struct stock *data, long rows)
{
	struct tm last, now;
	time_t tmp, ret = 0;

	last.tm_hour = 0;
	last.tm_min = 0;
	last.tm_sec = 1;
	last.tm_isdst = -1;
	if (rows >= 1)
		if (!strptime(data[0].date, "%Y-%m-%d", &last))
			if (!strptime(data[0].date, "%d-%B-%y", &last))
				return 0;

	if ((tmp = mktime(&last)) == -1)
		return 0;

	ret = tmp;
	tmp += 86400;
	memcpy(&last, localtime(&tmp), sizeof(struct tm));
	if ((last.tm_wday == 0) || (last.tm_wday == 6))
	{
		ret = tmp;
		tmp += 86400;
		memcpy(&last, localtime(&tmp), sizeof(struct tm));
		if ((last.tm_wday == 0) || (last.tm_wday == 6))
			ret = tmp;
	}

	time(&tmp);
	if (ret > tmp)
		ret = 0;

	time(&tmp);
	memcpy(&now, localtime(&tmp), sizeof(struct tm));

	if ((now.tm_mday == last.tm_mday) && (now.tm_mon  == last.tm_mon) && (now.tm_year == last.tm_year))
		ret = 0;

	return ret;
}

/* Average the stocks volume from the last two weeks */
long average_volume(struct stock *data, long rows, long n)
{
	long ret = 0;
	int x;

	if (rows <= n)
		return 0;
	for (x = 1; x <= n; x++)
		ret += data[x].volume;

	ret /= n;
	return ret;
}

/* Convert end-of-day data to weekly data */
struct stock *make_weekly(struct stock *data, long rows, long *w_rows)
{
	long x, y, volume;
	struct tm last;
	int dow, wday;
	struct stock *ret;
	time_t tmp;

	if (rows < 1)
		return NULL;

	*w_rows = 0;
	ret = (struct stock *)malloc((rows + 1) * sizeof(struct stock));

	for (x = 1; x < rows; x++)
	{
		last.tm_hour = 0;
		last.tm_min = 0;
		last.tm_sec = 1;
		last.tm_isdst = -1;
		if (!strptime(data[x].date, "%Y-%m-%d", &last))
			if (!strptime(data[x].date, "%d-%B-%y", &last))
				continue;

		if ((tmp = mktime(&last)) == -1)
			continue;

		memcpy(&last, localtime(&tmp), sizeof(struct tm));
		if (last.tm_wday > wday)
		{
			(*w_rows)++;
			ret[*w_rows].date = data[x].date;
			ret[*w_rows].open = data[x].open;
			ret[*w_rows].high = data[x].high;
			ret[*w_rows].low = data[x].low;
			ret[*w_rows].close = data[x].close;
			ret[*w_rows].volume = data[x].volume;
		}
		else
		{
			if (ret[*w_rows].high < data[x].high)
				ret[*w_rows].high = data[x].high;
			if (ret[*w_rows].low > data[x].low)
				ret[*w_rows].low = data[x].low;
		}


		wday = last.tm_wday;
	}

	ret = (stock *)realloc(ret, (*w_rows + 1) * sizeof(struct stock));

	return ret;
}

/* Print our analysis of the stock */
void diverge(char *ticker, struct stock *data, long rows)
{
	moving_average_convergence_divergence macd;
	relative_strength_index rsi;
	simple_moving_average sma;
	bollinger bb;
	double *sma_data = NULL, *bb_data = NULL, *rsi_data = NULL, *macd_data = NULL, *macd_h = NULL;
	long reset_h = 0, reset_l = 0, x;
	int d_stock, d_rsi, d_macd, d_macd_h;

	sma_data = sma.generate(data, rows, 20, rows - 26);
	bb_data = bb.bands(data, rows, 20, 2, rows - 26);

	for (x = 0; x < rows; x++)
	{
		if ((reset_h == 0) && (data[x].close > sma_data[x] + bb_data[x]))
			reset_h = x;

		if ((reset_l == 0) && (data[x].close < sma_data[x] - bb_data[x]))
			reset_l = x;

		if ((reset_h > 0) && (reset_l > 0))
			break;
	}

	/* stock trend */
	d_stock = 0;
	if ((data[0].close > data[1].close) && (data[0].close > data[2].close))
	{
		d_stock = HIGHERHIGH;
		for (x = 1; x < reset_l; x++)
		{
			if (data[x].close > data[0].close)
			{
				d_stock = LOWERHIGH;
				break;
			}
		}
	}
	else if ((data[0].close < data[1].close) && (data[0].close < data[2].close))
	{
		d_stock = LOWERLOW;
		for (x = 1; x < reset_h; x++)
		{
			if (data[x].close < data[0].close)
			{
				d_stock = LOWERHIGH;
				break;
			}
		}
	}

	/* rsi trend */
	d_rsi = 0;
	if (d_stock)
	{
		rsi_data = rsi.generate(data, rows, 14, rows - 26);
		d_rsi = divergence(rsi_data, rows - 26, reset_h, reset_l, NULL);
	}

	/* MACD trend */
	d_macd = 0;
	if (d_rsi)
	{
		macd_data = macd.generate(data, rows, 12, 26, rows - 26);
		d_macd = divergence(macd_data, rows - 26, reset_h, reset_l, NULL);
	}

	/* MACD histogram trend */
	d_macd_h = 0;
	if (d_macd)
	{
		macd_h = macd.histogram(data, rows, 12, 26, 9, rows - 26);
		d_macd_h = divergence(macd_h, rows - 26, reset_h, reset_l, NULL);
	}

	if (d_macd_h)
	{
		if (((d_stock == HIGHERHIGH) && (d_rsi == LOWERHIGH) && (d_macd == LOWERHIGH) && (d_macd_h == LOWERHIGH)) ||
	    	    ((d_stock == LOWERLOW) && (d_rsi == HIGHERLOW) && (d_macd == HIGHERLOW) && (d_macd_h == HIGHERLOW)))
			printf("%s is in triple divergence (RSI, MACD, MACD histogram)!\n", ticker);
	}

	if (macd_h)
		free(macd_h);
	if (macd_data)
		free(macd_data);
	if (rsi_data)
		free(rsi_data);
	if (sma_data)
		free(sma_data);
	if (bb_data)
		free(bb_data);

	return;
}

/* Print our analysis of the stock */
void tails(char *ticker, struct stock *data, long rows)
{
	simple_moving_average sma;
	bollinger bb;
	double *sma_data = NULL, *bb_data = NULL;
	int x, count;

	if (rows <= 35)
		return;

	sma_data = sma.generate(data, rows, 20, rows - 26);
	bb_data = bb.bands(data, rows, 20, 2, rows - 26);

	count = 0;
	for (x = 0; x < 5; x++)
	{
		if ((data[x].low < sma_data[x] - bb_data[x]) && (data[x].close > sma_data[x] - bb_data[x]))
		{
			count++;
			if (data[x].open < sma_data[x] - bb_data[x])
				count++;
		}
		else if ((data[x].high > sma_data[x] + bb_data[x]) && (data[x].close < sma_data[x] + bb_data[x]))
		{
			count--;
			if (data[x].open > sma_data[x] + bb_data[x])
				count--;
		}

		if ((data[x].close > sma_data[x] + bb_data[x]) || (data[x].close < sma_data[x] - bb_data[x]) ||
		    ((data[x].high < sma_data[x] + bb_data[x]) && (data[x].low > sma_data[x] - bb_data[x])))
		{
			count = 0;
			break;
		}
	}

	if ((count >= 4) || (count <= -4))
		printf("%s, strength: %i\n", ticker, count);

	if (sma_data)
		free(sma_data);
	if (bb_data)
		free(bb_data);

	return;
}

/* Print our analysis of the stock */
void analyze(char *ticker, struct stock *data, long rows)
{
	relative_strength_index rsi;
	double amount, *daily_rsi, *weekly_rsi;
	struct stock *weekly;
	bool list = false;
	long x, w_rows;

	daily_rsi = rsi.generate(data, rows, 14, 1);

	/* delisted or bought out */
	if ((*daily_rsi == 0) || (*daily_rsi == 100))
	{
		if (verbose)
			printf("%s: rsi = %f\n", ticker, *daily_rsi);
		delist(ticker);
		return;
	}

	weekly = make_weekly(data, rows, &w_rows);
	weekly_rsi = rsi.generate(weekly, w_rows, 14, 12);

	/* Decide whether to show the stock or not */
	amount = (data[0].close - data[1].close) / data[0].close * 100;
	if ((percent > 0) && (amount >= percent))
		list = true;

	if ((*daily_rsi < 30) || (*daily_rsi > 70))
		list = true;
	if ((*weekly_rsi < 30) || (*weekly_rsi > 70))
		list = true;
	if (chart_patterns(data, rows, false, NULL))
		list = true;
	if (chart_patterns(weekly, w_rows, false, NULL))
		list = true;

	/* If so, spit out what we noticed */
	if (list)
	{
		if (percent > 0)
		{
			if (amount > percent)
				printf("%+.2f: %s\n", amount, ticker);
		}
		else
		{
			printf("%s:\n", ticker);
			if (*daily_rsi < 30)
				printf("\tDaily RSI: %03.2f < 30\n", *daily_rsi);
			else if (*daily_rsi > 70)
				printf("\tDaily RSI: %03.2f > 70\n", *daily_rsi);
			if (*weekly_rsi < 30)
				printf("\tWeekly RSI: %03.2f < 30\n", *weekly_rsi);
			else if (*weekly_rsi > 70)
				printf("\tWeekly RSI: %03.2f > 70\n", *weekly_rsi);

			chart_patterns(data, rows, true, "daily");
			chart_patterns(weekly, w_rows, true, "weekly");
		}
	}

	free(weekly);
	free(daily_rsi);
	free(weekly_rsi);

	return;
}

int divergence(double *values, long rows, long reset_high, long reset_low, long *pos)
{
	int ret = 0;
	long x, high = 0;

	if ((rows < 3) || (rows <= reset_high) || (rows <= reset_low))
		return ret;

	if ((reset_low > 0) && (values[0] > values[1]) && (values[0] > values[2]))
	{
		ret = HIGHERHIGH;
		for (x = 1; x < reset_low; x++)
		{
			if (values[x] > values[high])
			{
				ret = LOWERHIGH;
				high = x;
				if (!pos)
					break;
			}
		}
	}
	else if ((reset_high > 0) && (values[0] < values[1]) && (values[0] < values[2]))
	{
		ret = LOWERLOW;
		for (x = 1; x < reset_high; x++)
		{
			if (values[x] < values[high])
			{
				ret = HIGHERLOW;
				high = x;
				if (!pos)
					break;
			}
		}
	}

	if (pos)
		*pos = high;

	return ret;
}

bool chart_patterns(struct stock *data, long rows, bool print, const char *period)
{
	struct tm last;
	bool ret = false;
	double high, low;
	time_t old, now;
	long x;

	if (data[0].high != data[0].low)
	{
		if (data[0].close == data[0].open == data[0].high)
		{
			if (!print)
			       return true;
			printf("\tDragonfly on the %s (high = open = close)!\n", period);
		}
		else if (data[0].close == data[0].open == data[0].low)
		{
			if (!print)
				return true;
			printf("\tTombstone on the %s (low = open = close)!\n", period);
		}
	}

	/*time(&now);

	high = data[0].high;
	low = data[0].low;
	for (x = 1; x < rows; x++)
	{
		last.tm_hour = 0;
		last.tm_min = 0;
		last.tm_sec = 1;
		last.tm_isdst = -1;
		if (!strptime(data[x].date, "%Y-%m-%d", &last))
		{
			if (!strptime(data[x].date, "%d-%B-%y", &last))
			{
				high = low = -1;
				break;
			}
		}

		if ((old = mktime(&last)) == -1)
		{
			high = low = -1;
			break;
		}

		if (difftime(now, old) < 31536000)
		{
			if (data[x].high > high)
				high = -1;
			if (data[x].low < low)
				low = -1;
		}

		if (low == high == -1)
			break;
	}

	if (low > 0)
	{
		if (!print)
			return true;
		printf("\t52-week low: $%d\n", low);
	}
	if (high > 0)
	{
		if (!print)
			return true;
		printf("\t52-week high: $%d\n", high);
	}*/

	return ret;
}
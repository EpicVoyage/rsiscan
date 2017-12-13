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
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>
#include <string>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include "lib/config.h"
#include "lib/rsiscript.h"
#include "lib/http.h"
#include "lib/comma_separated_values.h"
#include "lib/stock.h"
#include "lib/stats/moving_average_convergence_divergence.h"
#include "lib/stats/relative_strength_index.h"
#include "lib/stats/simple_moving_average.h"
#include "lib/stats/bollinger.h"
#include "lib/stats/high.h"
#include "lib/stats/low.h"

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

#define IGNORED 0
#define HIGHERHIGH 1
#define LOWERHIGH 2
#define HIGHERLOW 3
#define LOWERLOW 4

#define UP 0
#define DOWN 1

/* "Custom" data types */
enum server {yahoo = 0, google = 1, invest = 2};
//enum bool {false = 0, true = 1};

/* Servers to query for CSV data */
const char *servers[] = { "ichart.finance.yahoo.com",
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
void print_help(const char *prog);

/* 1.2 - Get/Save/Read stock data (cache) */
char *download_eod_data(const char *ticker, time_t from);
//char *download_intraday_data();
//void intraday_request(FILE *out);
char *get_path(const char *ticker, time_t from);
char *retrieve_data(FILE *in, const char *ticker);
void recursive_free(stock *data, long rows);

/* 1.3 - Functions which extrapolate from data */
void update_tickers();
stockinfo load_ticker(const char *ticker); //, stock **data, long *rows);
time_t get_last_date(stockinfo &data);
long average_volume(const stockinfo &data, long n = 10);
//stock *make_weekly(const stock *data, long rows, long *w_rows);
stockinfo stock_bump_day(stockinfo &data);
bool diverge(const char *ticker, const stockinfo &data, const char *desc);
double *stock_reduce_close(const stock *data, long rows);
//void tails(const char *ticker, const stockinfo &data);
bool bbands_narrow(const char *ticker, const stockinfo &data);
void low52wk(const char *ticker, const stockinfo &data);
//void test_screener(const char *ticker, const stockinfo &data);
void analyze(const char *ticker, const stockinfo data);
int divergence(const double *values, long rows, long reset_high, long reset_low, long *pos = NULL);
bool chart_patterns(const stockinfo &data, bool print, const char *period);
const char *exec_script(const char* const script, const stockinfo &data);

/* Global variables */
bool verbose, save_config, offline, intraday, walk_back, find_divergence, find_tails, low52, narrow_bbands; //, test;
int percent, seconds;
enum server source;
char const *script;
config conf;

/*****************************
 * 1.0 - Program entry point
 *****************************/
int main(int argc, char **argv)
{
	verbose = intraday = false;

	percent = 0;

	script = nullptr;
	offline = false;
	walk_back = false;
	low52 = false;
	//test = false;
	find_tails = false;
	narrow_bbands = false;
	find_divergence = false;

	source = google;
	create_config();

	// Configure log file output.
	// TODO: Severity does not appear in the logs.
	char *logfile = (char *)malloc(strlen(conf.config_dir) + 13);
	sprintf(logfile, "%s/rsiscan.log", (char *)conf.config_dir);
	boost::log::add_common_attributes();
	boost::log::expressions::attr< boost::log::trivial::severity_level >("Severity");
	boost::log::add_file_log(
			boost::log::keywords::file_name = logfile,
			boost::log::keywords::format = "[%TimeStamp%] [%Severity%]: %Message%"
	);

	// Start the main program.
	read_args(argc, argv);
	update_tickers();

	free(logfile);

	return 0;
}

/*************************************************
 * 1.1 - Basic program functionality
 *************************************************
 * Create save directory for downloaded CSV data */
void create_config()
{
	save_config = true;
}

/* Parse command line options */
void read_args(int argc, char **argv)
{
	char *filename;
	int x;

	for (x = 1; x < argc; x++)
	{
		if ((strcmp(argv[x], "-v") == 0) || (strcmp(argv[x], "--verbose") == 0))
			verbose = true;
		//else if (strcmp(argv[x], "--yahoo") == 0) - TODO: Discontinued Nov. 2017.
		//	source = yahoo;
		else if (strcmp(argv[x], "--google") == 0)
			source = google;
		/*else if (strcmp(argv[x], "--invest") == 0)
			source = invest;*/
		else if ((strncmp(argv[x], "--up=", 5) == 0) && (strlen(argv[x]) > 5))
			percent = atoi(argv[x] + 5);
		else if ((strncmp(argv[x], "--sleep=", 8) == 0) && (strlen(argv[x]) > 8))
			seconds = atoi(argv[x] + 8);
		else if (strcmp(argv[x], "--offline") == 0)
			offline = true;
		//else if (strcmp(argv[x], "--intraday") == 0) - TODO: service discontinued Nov. 2017
		//	intraday = true;
		else if (strcmp(argv[x], "--walk") == 0)
			walk_back = true;
		else if (strcmp(argv[x], "--divergence") == 0)
			find_divergence = true;
		else if (strcmp(argv[x], "--tails") == 0)
			//find_tails = true;
			// TODO: Check for 3-4 days?
			script = "{bb_bottom} > {low} & {bb_bottom} < {close}";
		else if (strcmp(argv[x], "--narrow-bbands") == 0)
			narrow_bbands = true;
		else if (strcmp(argv[x], "--low52") == 0)
			low52 = true;
		else if (strcmp(argv[x], "--test") == 0)
			//test = true;
			script = "{vol} > 500000 & ({close} > {bb_top} | {close} < {bb_bottom})";
		else if (strncmp(argv[x], "--script=", 9) == 0) {
			script = argv[x] + 9;
		}
		else if (*argv[x] != '-')
		{
			filename = (char *)malloc(strlen(argv[x]) + 1);
			strcpy(filename, argv[x]);
			conf.tickers.push_back(filename);
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
void print_help(const char *prog)
{
	printf("Usage: %s [--switch] [--switch] [TKR1] [TKR2] [etc.]\n\n", prog);

	//printf("    --yahoo      Download data from %s [default]\n", servers[yahoo]);
	printf("    --google     Download data from %s\n", servers[google]);
	/*printf("    --invest     Download data from InvestorLink [commercial]\n");*/
	printf("    --up=##      Only show stocks that have moved up ## percent\n");
	printf("    --sleep=##   Sleep ## seconds between server requests [default: 5]\n");
	printf("    --offline    Do not download any data for this run\n");
	//printf("    --intraday   Temporarily use intraday quotes for today's closing information\n");
	printf("    --walk       Walk back through the stock histories\n");
	printf("    --low52      Stocks near their 52-week low\n");
	printf("    --test       A test screener...\n");
	printf("    --script=\"...\" For advanced, on-the-fly processing\n");
	printf("    --divergence Look for divergences in the RSI, MACD, and MACD histogram\n");
	printf("    --tails      Look for lows outside BB, with closes inside 3 out of 4 days\n");
	printf("    --narrow-bbands Narrow Bollinger Bands.\n");
	printf("    --verbose    Print debug data along the way\n");
	printf("    --help       Print this text and exit\n\n");

	printf("Tickers listed on the command line will be added to the local cache and used for\n");
	printf("future runs when you don't specify any.\n\n");

	printf("This program will perform an analysis on the stocks you specify. It is *NOT*\n");
	printf("advice and should not be used in place of actually talking to your broker. It\n");
	printf("simply tries to look for anomalies in large amounts of data, but is not\n");
	printf("guaranteed to even do that. Use at your own risk.\n\n");

	exit(0);
}

/* Download End of Day ticker data */
char *download_eod_data(const char *ticker, time_t from)
{
	char *ret, *path = get_path(ticker, from);
	http h;
	ret = h.retrieve((char *)servers[source], path);

	free(path);
	sleep(seconds);
	return ret;
}

/* Download intrday ticker data */
/*char *download_intraday_data()
{
	struct hostent *record;
	struct sockaddr_in sin;
	char *ret = NULL;
	int sock, conn;
	FILE *wr, *re;

	if ((record = gethostbyname(i_servers[source])) == NULL)
	{
		fprintf(stderr, "DNS lookup for %s failed!\n", i_servers[source]);
		exit(1);
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(80);
	sin.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)record->h_addr));

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		fprintf(stderr, "Failed to open a socket!\n");
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

	*if ((ret != NULL) && (strcasestr(ret, "Sorry, the page you requested was not found.")))
	{
		printf("Error: %s - No data found, delisting stock\n", ticker);
		conf.delist(ticker);
		free(ret);
		ret = NULL;
	}*

	fclose(wr);
	fclose(re);
	close(sock);

	sleep(seconds);

	return ret;
}*/

char *get_path(const char *ticker, time_t from) {
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
/*void intraday_request(FILE *out)
{
	long x, stop;

	stop = (conf.tickers.size() < 100) ? conf.tickers.size() : 100;
	if (source == yahoo)
	{
		fprintf(out, "GET /d/quotes.csv?");
		for (x = 0; x <= stop; x++)
			fprintf(out, "s=%s&", conf.tickers[x]);
		fprintf(out, "f=sd1ohgl1v&e=.csv HTTP/1.1\r\n");
	}
	else
	{
		printf("How did you even get here? Yahoo is the only server supported right now!\n");
		return;
	}

	fprintf(out, "Host: %s\r\n", servers[source]);
	fprintf(out, "Connection: close\r\n\r\n");

	fflush(out);

	return;
}*/

/* Read back CSV data from server */
char *retrieve_data(FILE *in, const char *ticker)
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
					if ((x = fread(read, 1, len, in)))
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
void recursive_free(stock *data, long rows)
{
	long x;

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
	long x, pos, position, rows = 0, /*weekly_rows = 0, divergence_rows = 0,*/ all_rows = 0, vol = 0, distance1, distance2;
	stockinfo data, all_data, weekly_data, divergence_data;
	bool diverge_daily, diverge_weekly, found_setup, cont;
	moving_average_convergence_divergence macd;
	double *sma5 = nullptr, *macd_h = nullptr;
	int len, direction1, direction2;
	double res, movement1, movement2;
	simple_moving_average sma;
	struct dirent *file;
	rsiscript rs;
	DIR *saved;
	char *tmp, *filename;
	std::string result;

	if (save_config && (!conf.tickers.size()))
	{
		if ((saved = opendir(conf.stock_dir)) == NULL)
		{
			fprintf(stderr, "Error opening %s: %s\n", (char *)conf.stock_dir, strerror(errno));
			return;
		}

		while ((file = readdir(saved)))
		{
			len = strlen(file->d_name);
			len = len > 4 ? len - 4 : 0;

			if (strcasecmp(file->d_name + len, ".csv") != 0)
				continue;

			tmp = file->d_name;
			while ((*tmp != '\0') && (*tmp != '.'))
			{
				*tmp = toupper(*tmp);
				tmp++;
			}
			*tmp = 0;

			filename = (char *)malloc(strlen(file->d_name) + 1);
			strcpy(filename, file->d_name);
			conf.tickers.push_back(filename);
		}

		closedir(saved);
	}

	//if (!offline && intraday)
	//	download_intraday_data();

	if (verbose)
		printf("%li tickers loaded.\n", conf.tickers.size());

	for (x = 0; x < conf.tickers.size(); x++)
	{
		// Load the ticker data and remember our spot.
		stockinfo data = load_ticker(conf.tickers[x]); //, &data, &rows);
		all_data = data;
		all_rows = rows;
		rows = data.length();

		// If we loaded data...
		if (rows)
		{
			if (walk_back)
			{
				sma5 = sma.generate(data, 5, rows - 11);
			}

			// Review the data.
			do {
				found_setup = false;
				if (walk_back && verbose)
					printf("**%s**\n", data[0]->date);

				vol = average_volume(data);

				if (script != nullptr) {
					result = rs.parse(script, data);
					res = atof(result.c_str());

					if (res) {
						printf("%5s: %s.\n", conf.tickers[x], rs.last_variables.c_str());
					}
				}
				else if (find_divergence)
				{
					// TODO: Detect MACD cross-over signal after divergence ->
					macd_h = macd.histogram(data, 12, 26, 9, rows - 26);
					if (((*macd_h > 0) && (macd_h[1] < 0)) || ((*macd_h < 0) && (macd_h[1] > 0))) {
						divergence_data = data;
						divergence_data.shift();
						diverge(conf.tickers[x], divergence_data, "MACD x-over after daily");
					}

					diverge_daily = diverge(conf.tickers[x], data, "potential daily");

					weekly_data = data.weekly();
					diverge_weekly = diverge(conf.tickers[x], weekly_data, "potential weekly");

					found_setup = (diverge_daily || diverge_weekly);
					if (diverge_daily && diverge_weekly)
						printf("\n%s, %s: Daily and Weekly triple divergence!\n\n", data[0]->date, conf.tickers[x]);

					if (macd_h)
						free(macd_h);
				}
				//else if (find_tails)
				//	tails(conf.tickers[x], data);
				else if (low52)
					low52wk(conf.tickers[x], data);
				else if (narrow_bbands)
					found_setup = bbands_narrow(conf.tickers[x], data);
				//else if (test)
				//	test_screener(conf.tickers[x], data);
				else //if (vol > 1000000)
					analyze(conf.tickers[x], data);
				//else if (verbose)
				//	printf("%s, %s: volume = %li\n", data[0]->date, conf.tickers[x], vol);

				// TODO?: There are more efficient ways to do this than to re-run the full test for every day.
				if (walk_back && (rows > 100))
				{
					// Determine follow-up movement.
					if (found_setup)
					{
						// TODO: Weak direction comparison.
						position = pos = all_rows - rows;
						if (position > 3)
						{
							direction1 = ((sma5[pos] > sma5[pos - 1]) || ((sma5[pos] == sma5[pos - 1]) && (sma5[pos - 1] == sma5[pos - 2]))) ? DOWN : UP;
							distance1 = 0;
							movement1 = 0;
							do
							{
								pos--;
								if (direction1 == UP)
								{
									if (all_data[pos]->close - all_data[position]->close > movement1)
										movement1 = all_data[pos]->close - all_data[position]->close;
									cont = (sma5[pos - 1] >= sma5[pos]);
								}
								else
								{
									if (all_data[position]->close - all_data[pos]->close < movement1)
										movement1 = all_data[pos]->close - all_data[position]->close;
									cont = (sma5[pos] >= sma5[pos - 1]);
								}
								//printf("%s (%s vs. %s): %.02f (%li), %.02f (%li), %.02f\n", direction1 == UP ? "UP" : "DOWN", all_data[position]->date, all_data[pos]->date, all_data[position].close, position, all_data[pos].close, pos, movement1);
								distance1++;
							} while ((pos > 1) && cont);
							//for (distance1 = 1; (pos > 1) && (sma5[pos] >= sma5[pos -  1]); pos--);
							direction2 = (direction1 == UP) ? DOWN : UP;
							distance2 = 0;
							movement2 = 0;
							position = pos;
							cont = true;
							pos--;
							for (; (pos > 1) && cont; pos--)
							{
								if (direction2 == UP)
								{
									if (all_data[pos]->close - all_data[position]->close > movement2)
										movement2 = all_data[pos]->close - all_data[position]->close;
									cont = (sma5[pos - 1] >= sma5[pos]);
								}
								else
								{
									if (all_data[pos]->close - all_data[position]->close < movement2)
										movement2 = all_data[pos]->close - all_data[position]->close;
									cont = (sma5[pos] >= sma5[pos - 1]);
								}
								//printf("%s (%s vs. %s): %.02f (%li), %.02f (%li), %.02f\n", direction2 == UP ? "UP" : "DOWN", all_data[position].date, all_data[pos].date, all_data[position].close, position, all_data[pos].close, pos, movement2);
								distance2++;
							}

							printf("    Follow-up: %s for %li (%+.02f); %s for %li (%+.02f)\n\n", (direction1 == UP ? "UP" : "DOWN"), distance1, movement1, (direction2 == UP ? "UP" : "DOWN"), distance2, movement2);
						}
					}

					data = stock_bump_day(data);
				}
			} while (walk_back && data.length() && (rows > 100));

			// Clean up.
			if (walk_back && sma5)
				free(sma5);
		}
		else
		{
			printf("Failed to load stock: %s\n", conf.tickers[x]);
		}
	}

	return;
}

/* Load ticker data (file, then internet) and parse */
stockinfo load_ticker(const char *ticker) //, stock **data, long *rows)
{
	long x; //, len;
	char /* *tmp,*/ *block, *blocknew, *filename;
	comma_separated_values csv;
	stockinfo s;
	long rows;
	time_t from;

	filename = conf.get_filename(ticker);
	if (!s.load_csv(filename)) {
		if (offline || ((block = download_eod_data(ticker, 0)) == NULL))
		{
			if (verbose)
				printf("Unable to load %s from server. Giving up.\n", ticker);

			return s;
		}
	}

	if (s.length()) {
		if ((from = get_last_date(s)) != 0) {
			if (!offline && ((blocknew = download_eod_data(ticker, from)) != NULL)) {
				//for (x = 0; x < len; x++)
				//	if (*(block + x) == '\0') *(block + x) = '\n';

				//tmp = (char *)malloc(strlen(block) + strlen(blocknew) + 2);
				//sprintf(tmp, "%s\n%s", blocknew, block);

				free(blocknew);

				stock *temp = csv.parse(blocknew, &rows);
				for (x = 0; x < rows; x++) {
					s.insert_at(temp[x], x);
				}
				free(temp);
			}
		}

		s.save_csv(filename);
	} else {
		// TODO: Delist?
	}

	free(filename);

	return s;
}

/* Get the last date we have data for, skip weekends */
time_t get_last_date(stockinfo &data)
{
	struct tm last, now;
	time_t tmp, ret = 0;

	last.tm_hour = 0;
	last.tm_min = 0;
	last.tm_sec = 1;
	last.tm_isdst = -1;
	if (!strptime(data[0]->date, "%Y-%m-%d", &last))
		if (!strptime(data[0]->date, "%d-%B-%y", &last))
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

/**
 * Average the stocks volume from the last two weeks.
 */
long average_volume(const stockinfo &data, long n)
{
	long rows = data.length();
	long ret = 0;
	int x;

	if (rows <= n)
		return 0;
	for (x = 1; x <= n; x++)
		ret += data[x]->volume;

	ret /= n;
	return ret;
}

/**
 * Convert end-of-day data to weekly data.
 */
/*stock *make_weekly(const stock *data, long rows, long *w_rows)
{
	long x;
	struct tm last;
	int wday = -1;
	stock *ret;
	time_t tmp;

	if (rows < 1)
		return NULL;

	*w_rows = -1;
	ret = (stock *)malloc((rows + 1) * sizeof(stock));

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
		// TODO: Save the date for the first trade day of the week.
		if (last.tm_wday > wday)
		{
			(*w_rows)++;
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

		ret[*w_rows].date = data[x].date;
		wday = last.tm_wday;
	}

	ret = (stock *)realloc(ret, (*w_rows + 1) * sizeof(stock));

	return ret;
}*/

/**
 * Bump the latest day/week from loaded stock data.
 *
 * @returns A new stockinfo object
 */
stockinfo stock_bump_day(stockinfo &data)
{
	long rows = data.length();
	stockinfo ret;

	if (rows < 1)
		return ret;

	ret = data;
	ret.shift();
	return ret;
}

/**
 * Reduce a loaded stock struct to an array of close prices.
 */
double *stock_reduce_close(const stockinfo &data, long rows) {
	double *ret = NULL;
	long x;

	if (rows <= 0)
	{
		ret = (double *)malloc(sizeof(double));
		ret[0] = 0;
		return ret;
	}

	ret = (double *)malloc(sizeof(double) * (rows + 1));
	for (x = 0; x < rows; x++)
		ret[x] = data[x]->close;

	return ret;
}

/**
 * Try to find triple divergences, as defined by Chris Verheigh (The Option Trainer).
 *
 * TODO: Generate a list of highs/lows from BB resets, then compare their placement to each other. The
 * code currently finds "divergences" of different indicators at different locations - which means they
 * aren't really divergences.
 *
 * TODO: We need a strength indicator.
 */
bool diverge(const char *ticker, const stockinfo &data, const char *desc)
{
	const char *debug_modes[] = {"IGNORED", "HIGHER HIGH", "LOWER HIGH", "HIGHER LOW", "LOWER LOW"};

	moving_average_convergence_divergence macd;
	relative_strength_index rsi;
	simple_moving_average sma;
	bollinger bb;
	bool is_uptrend, is_diverging = false;
	double *stock_close = NULL, /* *sma_weekly = NULL,*/ *sma_data = NULL, *bb_data = NULL, *rsi_data = NULL, *macd_data = NULL, *macd_h = NULL;
	long trend_change = 0, reset_h = 0, reset_l = 0, found_divergence = 0, x;
	int d_stock, d_rsi, d_macd, d_macd_h;
	long rows = data.length();

	//sma_weekly = sma.generate(data, 5, rows - 11);
	sma_data = sma.generate(data, 20, rows - 26);
	bb_data = bb.bands(data, 20, 2, rows - 26);

	// Find the current "trend" - "up" or "down."
	x = 0;
	do
	{
		x++;
		is_uptrend = (sma_data[x] > sma_data[x-1]);
	} while (sma_data[x] == sma_data[x-1]);

	for (; x < rows; x++)
	{
		// If the SMA trend direction changed.
		if (is_uptrend ? (sma_data[x - 1] > sma_data[x]) : (sma_data[x] > sma_data[x - 1]))
		{
			trend_change = x;
			break;
		}
	}

	// We can use shorter values, but 1-3 are almost useless.
	if (trend_change > 5)
		trend_change = 5;

	// Prices outside the Bollinger Bands reset are reset points for our "high" and "low" patterns. I think CV said to do this sometime.
	for (x = trend_change; x < rows; x++)
	{
		if ((reset_h == 0) && (data[x]->close > sma_data[x] + bb_data[x]))
			reset_h = x;

		if ((reset_l == 0) && (data[x]->close < sma_data[x] - bb_data[x]))
			reset_l = x;

		if ((reset_h > 0) && (reset_l > 0))
			break;
	}

	// stock trend
	stock_close = stock_reduce_close(data, rows);
	d_stock = divergence(stock_close, rows - 26, reset_h, reset_l, NULL);

	// rsi trend
	d_rsi = IGNORED;
	if ((d_stock == HIGHERHIGH) || (d_stock == LOWERLOW))
	{
		rsi_data = rsi.generate(data, 14, data.length() - 26);
		d_rsi = divergence(rsi_data, data.length() - 26, reset_h, reset_l, &found_divergence);
	}

	// MACD trend
	d_macd = IGNORED;
	if (d_rsi && found_divergence)
	{
		macd_data = macd.generate(data, 12, 26, rows - 26);
		d_macd = divergence(macd_data - 26, rows, reset_h, reset_l, &found_divergence);
	}

	// MACD histogram trend
	d_macd_h = IGNORED;
	if (d_macd)
	{
		macd_h = macd.histogram(data, 12, 26, 9, rows - 26);
		if (((d_stock == HIGHERHIGH) && (macd_h[0] > 0)) || ((d_stock == LOWERLOW) && (macd_h[0] < 0)))
			d_macd_h = divergence(macd_h, rows - 26, reset_h, reset_l, &found_divergence);
	}

	// Look for the divergence.
	if (d_macd_h)
	{
		if (((d_stock == HIGHERHIGH) && (d_rsi == LOWERHIGH) && (d_macd == LOWERHIGH) && (d_macd_h == LOWERHIGH)) ||
	    	    ((d_stock == LOWERLOW) && (d_rsi == HIGHERLOW) && (d_macd == HIGHERLOW) && (d_macd_h == HIGHERLOW)))
		{
			printf("%s, %s is in \033[%im%s\033[0m triple divergence\n", data[0]->date, ticker, (strncmp(desc, "potential", 6) == 0) ? 31 : 32, desc);
			printf("    \033[%im%s\033[0m; Distance: %li, %s; %.02f, %.02f; RSI: %.02f, %.02f, %s; MACD: %.02f, %.02f, %s; MACD histogram: %.02f, %.02f, %s; high reset: %li/%s, low reset: %li/%s; Volume: %li!\n\n", (d_stock == HIGHERHIGH) ? 31 : 32, debug_modes[d_stock], found_divergence, data[found_divergence]->date, stock_close[found_divergence], *stock_close,
					rsi_data[found_divergence], *rsi_data, debug_modes[d_rsi],
					macd_data[found_divergence], *macd_data, debug_modes[d_macd],
					macd_h[found_divergence], *macd_h, debug_modes[d_macd_h],
					reset_h, data[reset_h]->date, reset_l, data[reset_l]->date, data[0]->volume);
			is_diverging = true;
		}
		else if (verbose)
			printf("%s, Stock: %s/%s, RSI: %s, MACD: %s, Histogram: %s\n", data[0]->date, ticker, debug_modes[d_stock], debug_modes[d_rsi], debug_modes[d_macd], debug_modes[d_macd_h]);
	}
	else if (verbose)
		printf("%s, Stock: %s/%s, RSI: %s, MACD: %s, Histogram: %s\n", data[0]->date, ticker, debug_modes[d_stock], debug_modes[d_rsi], debug_modes[d_macd], debug_modes[d_macd_h]);

	if (stock_close)
		free(stock_close);
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

	return is_diverging;
}

/**
 * Find stocks which opened+closed inside Bollinger Bands, but which dipped below the bottom band.
 */
/*void tails(const char *ticker, const stockinfo &data)
{
	simple_moving_average sma;
	bollinger bb;
	double *sma_data = NULL, *bb_data = NULL;
	int x, count;
	long rows = data.length();

	if (data.length() <= 35)
		return;

	sma_data = sma.generate(data, 20, rows - 26);
	bb_data = bb.bands(data, 20, 2, rows - 26);

	count = 0;
	for (x = 0; x < 5; x++)
	{
		if ((data[x]->low < sma_data[x] - bb_data[x]) && (data[x]->close > sma_data[x] - bb_data[x]))
		{
			count++;
			if (data[x]->open < sma_data[x] - bb_data[x])
				count++;
		}
		else if ((data[x]->high > sma_data[x] + bb_data[x]) && (data[x]->close < sma_data[x] + bb_data[x]))
		{
			count--;
			if (data[x]->open > sma_data[x] + bb_data[x])
				count--;
		}

		if ((data[x]->close > sma_data[x] + bb_data[x]) || (data[x]->close < sma_data[x] - bb_data[x]) ||
		    ((data[x]->high < sma_data[x] + bb_data[x]) && (data[x]->low > sma_data[x] - bb_data[x])))
		{
			count = 0;
			break;
		}
	}

	if ((count >= 4) || (count <= -4))
		printf("%s, %s, strength: %i\n", data[0]->date, ticker, count);

	if (sma_data)
		free(sma_data);
	if (bb_data)
		free(bb_data);

	return;
}*/

/**
 * Narrow bollinger bands.
 */
bool bbands_narrow(const char *ticker, const stockinfo &data)
{
	simple_moving_average sma;
	bollinger bb;
	double *sma_data = nullptr, *bb_data = nullptr, narrowest = 0, widest = 0;
	bool in_bands, ret = false;
	long days_in_bands, x;
	long rows = data.length();

	sma_data = sma.generate(data, 20, rows - 26);
	bb_data = bb.bands(data, 20, 2, rows - 26);

	// TODO: Generalize me. Something like ./rsiscan '(volume > 100000) && (bb_top_daily < last_trade)'.
	if (average_volume(data) < 500000)
	{
		if (verbose)
			printf("%s, %s: Ignored for low volume.\n", data[0]->date, ticker);
		return ret;
	}

	if (!sma_data || !sma_data[0])
	{
		if (verbose)
			printf("%s, %s: Ignored because we have no SMA.\n", data[0]->date, ticker);
		return ret;
	}

	// TODO: 252 = 2014's trading days.
	widest = narrowest = bb_data[0];
	in_bands = true;
	days_in_bands = 0;
	for (x = 0; (x < 252) && (x < rows); x++)
	{
		if (bb_data[x] < narrowest)
			narrowest = bb_data[x];
		else if (bb_data[x] > widest)
			widest = bb_data[x];

		//printf("%.02f, %.02f. %.02f\n", sma_data[x], bb_data[x], data[x].close);
		if (in_bands && (sma_data[x] + bb_data[x] > data[x]->close) && (data[x]->close > sma_data[x] - bb_data[x]))
			days_in_bands++;
		else
			in_bands = false;
	}

	ret = (bb_data[0] < (narrowest * 1.15));

	if (ret)
	{
		printf("%s, \033[%im%s\033[0m: Narrow bands (In Bands: %li, Close: %.02f, Width: %.02f, Narrowest: %.02f, Widest: %.02f).\n", data[0]->date, (days_in_bands > 20) ? 32 : 0, ticker, days_in_bands, data[0]->close, bb_data[0], narrowest, widest);
	}

	return ret;
}

/**
 * Find stocks near their 52-week low.
 */
void low52wk(const char *ticker, const stockinfo &data)
{
	low low;
	high high;
	double low52, high52, close;

	close = data[0]->close;
	low52 = low.find(data, 251);
	high52 = high.find(data, 251);

	if ((low52 == 0) || (high52 == 0))
	{
		if (verbose)
			printf("%s, %s: close = %.2f, low52 = %.2f, high52 = %.2f\n", data[0]->date, ticker, close, low52, high52);
		/*conf.delist(ticker);*/
		return;
	}

	/* 15% */
	if (close < (low52 + ((high52 - low52) * 0.15)))
		printf("%s, %s: close = %.2f, low52 = %.2f, high52 = %.2f, range = %.2f\n", data[0]->date, ticker, close, low52, high52, high52 - low52);

	return;
}

/**
 * 1. Volume > 500000
 * 2. BB Top < Last Trade || BB Bottom > Last Trade
 * 3. Wait for intra-day stall
 * 4. Buy Call at $.05?
 * 5. Profit???
 */
/*void test_screener(const char *ticker, const stockinfo &data)
{
	simple_moving_average sma;
	bollinger bb;
	double *sma_data = NULL, *bb_data = NULL;

	sma_data = sma.generate(data, 20, data.length() - 26);
	bb_data = bb.bands(data, 20, 2, data.length() - 26);

	// TODO: Generalize me. Something like ./rsiscan '(volume > 100000) && (bb_top_daily < last_trade)'.
	if (average_volume(data) < 500000)
	{
		if (verbose)
			printf("%s, %s: Ignored for low volume.\n", data[0]->date, ticker);
		return;
	}

	if (!sma_data || !sma_data[0])
	{
		if (verbose)
			printf("%s, %s: Ignored because we have no SMA.\n", data[0]->date, ticker);
		return;
	}

	if ((data[0]->close > sma_data[0] + bb_data[0]) && (data[2]->close > sma_data[2] + bb_data[2]))
	{
		printf("%s, %s: Above Bollinger Bands (Close: %.02f, SMA: %.02f, BB: %.02f, volume: %li).\n", data[0]->date, ticker, data[0]->close, sma_data[0], bb_data[0], data[0]->volume);
	}
	else if (data[0]->close < sma_data[0] - bb_data[0])
	{
		printf("%s, %s: Below Bollinger Bands (Close: %.02f, SMA: %.02f, BB: %.02f, volume: %li).\n", data[0]->date, ticker, data[0]->close, sma_data[0], bb_data[0], data[0]->volume);
	}
}*/

/* Print our analysis of the stock */
void analyze(const char *ticker, stockinfo data)
{
	relative_strength_index rsi;
	double amount, *daily_rsi, *weekly_rsi;
	stockinfo weekly;
	bool list = false;
	//long w_rows;

	daily_rsi = rsi.generate(data, 14, 1);

	/* delisted or bought out */
	if ((*daily_rsi == 0) || (*daily_rsi == 100))
	{
		if (verbose)
			printf("%s, %s: rsi = %f\n", data[0]->date, ticker, *daily_rsi);
		conf.delist(ticker);
		return;
	}

	weekly = data.weekly();
	weekly_rsi = rsi.generate(weekly, 14, 12);

	/* Decide whether to show the stock or not */
	amount = (data[0]->close - data[1]->close) / data[0]->close * 100;
	if ((percent > 0) && (amount >= percent))
		list = true;

	if ((*daily_rsi < 30) || (*daily_rsi > 70))
		list = true;
	if ((*weekly_rsi < 30) || (*weekly_rsi > 70))
		list = true;
	if (chart_patterns(data, false, NULL))
		list = true;
	if (chart_patterns(weekly, false, NULL))
		list = true;

	/* If so, spit out what we noticed */
	if (list)
	{
		if (percent > 0)
		{
			if (amount > percent)
				printf("%s, %+.2f: %s\n", data[0]->date, amount, ticker);
		}
		else
		{
			printf("%s, %s:\n", data[0]->date, ticker);
			if (*daily_rsi < 30)
				printf("\tDaily RSI: %03.2f < 30\n", *daily_rsi);
			else if (*daily_rsi > 70)
				printf("\tDaily RSI: %03.2f > 70\n", *daily_rsi);
			if (*weekly_rsi < 30)
				printf("\tWeekly RSI: %03.2f < 30\n", *weekly_rsi);
			else if (*weekly_rsi > 70)
				printf("\tWeekly RSI: %03.2f > 70\n", *weekly_rsi);

			chart_patterns(data, true, "daily");
			chart_patterns(weekly, true, "weekly");
		}
	}

	free(daily_rsi);
	free(weekly_rsi);

	return;
}

/**
 * Trend: Determine whether the current value (values[0]) is a Higher High, Lower High, Higher Low, or Lower Low.
 *
 * @param values The values to check against each other.
 * @param rows The number of values we have.
 * @param reset_high How far back do we go before we stop checking for additional highs?
 * @param reset_low How far back do we go before we stop checking for additional lows?
 * @param pos - NULL => Ignored. 0 => Return location we found. +/- => Try to find a divergence near pos.
 */
int divergence(const double *values, long rows, long reset_high, long reset_low, long *pos)
{
	int ret = IGNORED;
	bool matches_direction;
	long x, start, stop, found = 0;

	if ((rows < 3) || (rows <= reset_high) || (rows <= reset_low))
		return ret;

	// Find highs.
	if ((reset_low > 0) && (values[0] > values[1]) && (values[0] > values[2]))
	{
		ret = HIGHERHIGH;
		start = (pos && *pos) ? *pos - 1 : 2;
		stop = (pos && *pos) ? *pos + 2 : reset_low;
		for (x = start; x < stop; x++)
		{
			matches_direction = true; //((values[0] < 0) && (values[x] < 0)) || ((values[0] > 0) && (values[x] > 0));
			if (values[x] > values[found] && matches_direction)
			{
				ret = LOWERHIGH;
				found = x;
				break;
			}
		}
	}

	// Find lows.
	else if ((reset_high > 0) && (values[0] < values[1]) && (values[0] < values[2]))
	{
		ret = LOWERLOW;
		start = (pos && *pos) ? *pos - 1 : 2;
		stop = (pos && *pos) ? *pos + 2 : reset_high;
		for (x = start; x < stop; x++)
		{
			matches_direction = true; //((values[0] < 0) && (values[x] < 0)) || ((values[0] > 0) && (values[x] > 0));
			if ((values[x] < values[found]) && matches_direction)
			{
				ret = HIGHERLOW;
				found = x;
				break;
			}
		}
	}

	if (pos)
		*pos = found;

	return ret;
}

bool chart_patterns(const stockinfo &data, bool print, const char *period)
{
	//struct tm last;
	bool ret = false;
	//double high, low;
	//time_t old, now;
	//long x;
	//long rows = data.length();

	if (!data.length()) {
		return ret;
	}

	if (data[0]->high != data[0]->low)
	{
		if (data[0]->close == data[0]->open == data[0]->high)
		{
			if (!print)
			       return true;
			printf("\t%s: Dragonfly on the %s (high = open = close)!\n", data[0]->date, period);
		}
		else if (data[0]->close == data[0]->open == data[0]->low)
		{
			if (!print)
				return true;
			printf("\t%s, Tombstone on the %s (low = open = close)!\n", data[0]->date, period);
		}
	}

	/*time(&now);

	high = data[0]->high;
	low = data[0]->low;
	for (x = 1; x < rows; x++)
	{
		last.tm_hour = 0;
		last.tm_min = 0;
		last.tm_sec = 1;
		last.tm_isdst = -1;
		if (!strptime(data[x]->date, "%Y-%m-%d", &last))
		{
			if (!strptime(data[x]->date, "%d-%B-%y", &last))
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
			if (data[x]->high > high)
				high = -1;
			if (data[x]->low < low)
				low = -1;
		}

		if (low == high == -1)
			break;
	}

	if (low > 0)
	{
		if (!print)
			return true;
		printf("\t%s, 52-week low: $%d\n", data[0]->date, low);
	}
	if (high > 0)
	{
		if (!print)
			return true;
		printf("\t%s, 52-week high: $%d\n", data[0]->date, high);
	}*/

	return ret;
}

char *topo_get_list(int which)
{
	char *ret = NULL;

	/* Possible lists in CSV format:
	 * ********************************************
	 * S&P 500:
	 * http://www2.standardandpoors.com/app/Satellite?pagename=spcom/page/download&sectorid=%20%3E%20'00'&itemname=%3E=%20'1'&dt=01-AUG-2007&indexcode=500
	 *
	 * Australian Securities Exchange (ASX):
	 * http://www.asx.com.au/asx/downloadCsv/ASXListedCompanies.csv
	 *
	 * New Zealand S. Exchange (NZX):
	 * http://www.nzx.com/scripts/portal_pages/p_csv_by_market.csv?code=ALL&board_type=S
	 *
	 * American:
	 * http://www.nasdaq.com//asp/symbols.asp?exchange=Q&start=0 (NASDAQ)
	 * http://www.nasdaq.com//asp/symbols.asp?exchange=1&start=0 (Amex)
	 * http://www.nasdaq.com//asp/symbols.asp?exchange=N&start=0 (NYSE)
	 *
	 * American Exchange (AMEX):
	 * http://www.amex.com/equities/dataDwn/EQUITY_EODLIST_03AUG2007.csv
	 *
	 * Chicago Stock Exchange:
	 * http://www.chx.com/content/Trading_Information/TI_tape_A.asp
	 * http://www.chx.com/content/Trading_Information/TI_tape_B.asp
	 * http://www.chx.com/content/Trading_Information/TI_tape_C.asp
	 */

	return ret;
}

/* Copy data out of stock structure into an array */
/*double *topo_copyoutd(struct stock *data, long rows, int type)
{
	double *ret;
	long x;

	if ((rows < 0) || ((type != OPEN) && (type != HIGH) && (type != LOW) && (type != CLOSE)))
		return NULL;

	ret = (double *)malloc(rows * sizeof(double));

	for (x = 0; x < rows; x++)
	{
		if (type == OPEN)
			ret[x] = data[x].open;
		else if (type == HIGH)
			ret[x] = data[x].high;
		else if (type == LOW)
			ret[x] = data[x].low;
		else if (type == CLOSE)
			ret[x] = data[x].close;
	}

	return ret;
}*/

/* Copy data out of stock structure into an array */
/*long *topo_copyoutl(struct stock *data, long rows, int type)
{
	long *ret;
	long x;

	if ((rows < 0) || ((type != DATE) && (type != VOLUME)))
		return NULL;

	ret = (long *)malloc(rows * sizeof(long));

	for (x = 0; x < rows; x++)
	{
		if (type == DATE)
			ret[x] = data[x].numeric;
		else if (type == VOLUME)
			ret[x] = data[x].volume;
	}

	return ret;
}*/

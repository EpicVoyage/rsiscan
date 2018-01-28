#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <algorithm>
#include <chrono>

#include <boost/log/trivial.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include "lib/comma_separated_values.h"
#include "lib/stock.h"

/**
 * Class setup.
 */
stockinfo::stockinfo(const stockinfo &init): orig_filename(nullptr), dirty(false) {
	copy(init);
}

/**
 * Class clean-up.
 */
stockinfo::~stockinfo() {
	long x, sz = data.size();;
	for (x = 0; x < sz; x++) {
		free(data[x].date);
	}

	if (orig_filename != nullptr) {
		free(orig_filename);
	}
}

/**
 * Load a CSV file into the data struct.
 */
bool stockinfo::load_csv(const char *filename) {
	comma_separated_values csv;
	struct stock *s;
	struct stat buf;
	char *tmp, *ret = nullptr;
	long x, rows;
	FILE *re;

	// Ensure that the file exists before we load it.
	if (stat(filename, &buf)) {
		BOOST_LOG_TRIVIAL(trace) << "Unable to load CSV file: " << filename;
		return false;
	}

	// Prep for the save_csv command.
	x = strlen(filename);
	orig_filename = (char *)malloc(x + 1);
	strcpy(orig_filename, filename);
	dirty = false;

	BOOST_LOG_TRIVIAL(trace) << "Reading stock data from: " << filename;

	// Load the entire file into memory.
	re = fopen(filename, "r");
	tmp = (char *)malloc(2048);
	while (fgets(tmp, 2048, re) != NULL) {
		if (ret == nullptr) {
			ret = (char *)malloc(strlen(tmp) + 1);
			*ret = '\0';
		}
		else
			ret = (char *)realloc(ret, strlen(ret) + strlen(tmp) + 1);

		strcat(ret, tmp);
	}

	// File cleanup.
	fclose(re);
	free(tmp);

	// Parse any CSV data in the file.
	if (ret != nullptr) {
		s = csv.parse(ret, &rows);
		for (x = 0; x < rows; x++) {
			s[x].timestamp = parse_csv_time(s[x].date);
			data.push_back(s[x]);
		}

		// Free the file memory.
		free(ret);
	}

	uniq();

	BOOST_LOG_TRIVIAL(trace) << "Loaded records: " << data.size();
	return true;
}

/**
 * Save the data struct as CSV data.
 *
 * @param filename. Optional if load_csv() was called first.
 * @return boolean. True if save was successful.
 */
bool stockinfo::save_csv(const char *filename) {
	const char *fn = (filename == nullptr) ? orig_filename : filename;
	long x, sz;
	FILE *wr;

	// We cannot save unless we have a filename.
	if (fn == nullptr) {
		BOOST_LOG_TRIVIAL(trace) << "No filename provided. Unable to save.";
		return false;
	}

	// Do not re-save if nothing has changed.
	if (!dirty && (strcmp(fn, orig_filename) == 0)) {
		BOOST_LOG_TRIVIAL(trace) << "No changes since we read the file. Skipping save operation.";
		return false;
	}

	BOOST_LOG_TRIVIAL(trace) << "Writing stock data to: " << filename;

	// Prepare to write.
	nosig();
	wr = fopen(filename, "w");
	sz = data.size();

	// Write the data.
	for (x = 0; x < sz; x++)
	{
		if (data[x].date)
			fprintf(wr, "%s,%g,%g,%g,%g,%li\n", data[x].date, data[x].open, data[x].high, data[x].low, data[x].close, data[x].volume);
	}

	// Clean up.
	fclose(wr);
	sig();

	return true;
}

/**
 * Insert stock element at the desired pos. Moves any later elements up by 1, which can be slow.
 *
 * @param struct stock s The element to insert.
 * @param long pos The place to insert s (default: 0).
 * @return A pointer to this class instance.
 */
stockinfo &stockinfo::insert_at(const struct stock s, const long pos) {
	struct stock tmp;

	// Make our own copy of the data.
	memcpy(&tmp, &s, sizeof(s));
	if (s.date != nullptr) {
		tmp.date = (char *)malloc(strlen(s.date));
		strcpy(tmp.date, s.date);

		// Parse the date into a timestamp, if needed.
		tmp.timestamp = parse_csv_time(tmp.date);
	}

	// Retrieve an iterator that points to our insertion point.
	std::vector<struct stock>::iterator p = data.begin();
	p += pos;

	// Insert tmp.
	data.insert(p, tmp);

	// The data structure should be saved.
	dirty = true;

	return *this;
}

/**
 * Return the number of items in the data struct.
 */
const long stockinfo::length() const {
	return data.size();
}
const long stockinfo::length() {
	return data.size();
}

/**
 * Remove the first element and return it.
 */
struct stock stockinfo::shift() {
	struct stock ret;

	if (data.size() > 0) {
		memcpy(&ret, &data[0], sizeof(data[0]));
		if (data[0].date != nullptr) {
			ret.date = (char *)malloc(strlen(data[0].date));
			strcpy(ret.date, data[0].date);
		}
	}

	return ret;
}

/**
 * Allow us to read data from the data struct.
 *
 * @param index The desired data point.
 * @return The data point requested. nullptr if index does not exist.
 */
const struct stock *stockinfo::operator [](const long index) const {
	if (index < 0 || index >= data.size()) {
		if (data.size() == 0) {
			BOOST_LOG_TRIVIAL(trace) << "Tried to access record " << index << " but no records exist!";
		} else {
			BOOST_LOG_TRIVIAL(trace) << "Tried to access record " << index << " but highest record is " << data.size() - 1 << "!";
		}
		return nullptr;
	}

	return &data[index];
}
const struct stock *stockinfo::operator [](const long index) {
	// Scott Meyers on reducing code duplication. https://stackoverflow.com/a/123995/850782
	return const_cast<struct stock *>(static_cast<const stockinfo &>(*this)[index]);
}

/**
 * Add a struct stock to the end of the data element.
 *
 * @return Pointer to the current class instance.
 */
stockinfo &stockinfo::operator +=(const struct stock s) {
	struct stock tmp;

	// Make our own copy of the data.
	memcpy(&tmp, &s, sizeof(s));
	if (s.date != nullptr) {
		tmp.date = (char *)malloc(strlen(s.date));
		strcpy(tmp.date, s.date);

		// Parse the date into a timestamp, if needed.
		tmp.timestamp = parse_csv_time(tmp.date);
	}

	// Insert tmp.
	data.push_back(tmp);

	// The data structure should be saved.
	dirty = true;

	return *this;
}

stockinfo &stockinfo::operator =(const stockinfo &s) {
	copy(s);
	return *this;
}

/**
 * Sort the data structure by timestamp, then check for duplicate timestamps.
 *
 * @return Pointer to the current class instance.
 */
stockinfo &stockinfo::uniq() {
	long x, length;

	// The data must be sorted by timestamp before we can check for duplicate timestamps.
	sort();

	// Check neighbors for duplicate timestamps.
	length = data.size() - 1;
	for (x = 0; x < length; x++) {
		// Determine if the next element matches the current timestamp.
		if (data[x].timestamp == data[x+1].timestamp) {
			BOOST_LOG_TRIVIAL(info) << "Removing duplicate: " << x;

			// Remove the duplicate element.
			std::vector<struct stock>::iterator p = data.begin();
			p += x;
			data.erase(p);

			// We have modified the data structure.
			dirty = true;

			// Decrease the run length since we removed an element.
			length--;
		}
	}

	return *this;
}

/**
 * Sort the data structure by timestamp, in descending order.
 *
 * @todo Should we update set the dirty flag?
 * @return Pointer to the current class instance.
 */
stockinfo &stockinfo::sort() {
	std::sort(data.begin(), data.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.timestamp > rhs.timestamp;
	});

	return *this;
}

/**
 * NOTE: data must be sorted before this is called.
 */
template<class T>
stockinfo stockinfo::rollup_iterator(T &iterator, int number) {
	boost::gregorian::date d;
	bool add = false, init = true;
	struct stock tmp;
	struct tm last;
	long x, length;
	stockinfo ret;
	boost::gregorian::date current_date;

	length = data.size();
	BOOST_LOG_TRIVIAL(info) << "Rollup size: " << length;

	// Go back to the last [number iterator period] on record.
	if (length > 0) {
		localtime_r(&data[0].timestamp, &last);
		current_date = boost::gregorian::date(last.tm_year + 1900, last.tm_mon + 1, last.tm_mday);
		x = 0;
		while (iterator > current_date) {
			multi_decrement(iterator, number);
			x++;
		}

		BOOST_LOG_TRIVIAL(info) << "First date is " << (x * number) << " periods back.";
	}

	// Loop through the dates and collect them.
	for (x = 0; x < length; x++) {
		localtime_r(&data[x].timestamp, &last);
		current_date = boost::gregorian::date(last.tm_year + 1900, last.tm_mon + 1, last.tm_mday);

		// Is it time to switch to decrement the iterator?
		if (iterator > current_date) {
			BOOST_LOG_TRIVIAL(info) << "Decrement iterator.";
			multi_decrement(iterator, number);
			init = true;

			// New week.
			if (add)
				ret += tmp;
		}

		if (init) {
			// Re-set the object data for the new week..
			tmp.open = data[x].open;
			tmp.high = data[x].high;
			tmp.low = data[x].low;
			tmp.close = data[x].close;
			tmp.volume = data[x].volume;
			init = false;
		} else {
			// Update the existing week.
			if (data[x].high > tmp.high)
				tmp.high = data[x].high;
			if (data[x].low < tmp.low) {
				tmp.low = data[x].low;
			}
			tmp.open = data[x].open;
			tmp.volume += data[x].volume;
		}

		// Capture the earliest day of the week.
		tmp.timestamp = data[x].timestamp;
		tmp.date = data[x].date;

		// Start saving after the first run.
		add = true;
	}

	if (add)
		ret += tmp;

	return ret;
}

template<class T>
T &stockinfo::multi_decrement(T &iterator, int number) {
	for (int x = 0; x < number; x++) {
		--iterator;
	}

	return iterator;
}

/**
 * Roll daily data up into larger time periods. Sorts the data first.
 *
 * @todo Allow choice of weekly alignment day.
 *
 * @param int number. Default: 1
 * @param timeperiods period. Default: week.
 * @param bool align_week Default: false. If set to true, iterate on Sundays.
 * @return New stockinfo object.
 */
stockinfo stockinfo::rollup(int number, timeperiods period, bool align_week) {
	if (data.size() < 2) {
		return *this;
	}

	// This only works if the data is in order. It does not necessarily have to be unique...
	sort();

	struct tm first;
	localtime_r(&data[0].timestamp, &first);

	boost::gregorian::date d(first.tm_year + 1900, first.tm_mon + 1, first.tm_mday);

	if (align_week) {
		BOOST_LOG_TRIVIAL(info) << "Boost Sunday alignment start: " << d;
  	auto at_saturday   = boost::gregorian::greg_weekday(boost::gregorian::Sunday);
  	d = next_weekday(d, at_saturday);
		BOOST_LOG_TRIVIAL(info) << "Boost Sunday alignment finish: " << d;
	}

	boost::gregorian::day_iterator itr_day(d);
	boost::gregorian::week_iterator itr_week(d);
	boost::gregorian::month_iterator itr_month(d);
	boost::gregorian::year_iterator itr_year(d);
	stockinfo ret;

	switch (period) {
		case day:
			ret = rollup_iterator(itr_day, number);
			break;
		case week:
			ret = rollup_iterator(itr_week, number);
			break;
		case month:
			ret = rollup_iterator(itr_month, number);
			break;
		case year:
			ret = rollup_iterator(itr_year, number);
			break;
		default:
			// TODO: Return default.
			ret = *this;
			break;
	}

	return ret;
}

stockinfo stockinfo::weekly(bool align_week) {
	return rollup(1, week, align_week);
}

/**
 * Parse YYYY-MM-DD or d-Mmm-YY dates into something machine-readable.
 *
 * @param char *date The date to parse.
 * @return time_t
 */
time_t stockinfo::parse_csv_time(const char *date) {
	struct tm parsed;
	time_t ret = 0;

	if (date == nullptr) {
		return ret;
	}

	// Zero out the time sections. We don't care about them yet (TODO?)
	parsed.tm_hour = 0;
	parsed.tm_min = 0;
	parsed.tm_sec = 1;
	parsed.tm_isdst = -1;

	// Yahoo's format was: %Y-%m-%d. Google's is %d-%B-%y.
	if (!strptime(date, "%Y-%m-%d", &parsed)) {
		if (!strptime(date, "%d-%B-%y", &parsed)) {
			BOOST_LOG_TRIVIAL(info) << "Failed to parse date: " << date;
			return ret;
		}
	}

	// Convert <struct tm> into <time_t>.
	ret = mktime(&parsed);
	return ret == -1 ? 0 : ret;
}

/**
 * Prevent the interruption of critical tasks.
 */
void stockinfo::nosig()
{
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	return;
}

/**
 * Allow the program to be halted again.
 */
void stockinfo::sig()
{
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);

	return;
}

void stockinfo::copy(const stockinfo &s) {
	struct stock tmp;
	long x, length;

	length = s.length();
	for (x = 0; x < length; x++) {
		memcpy(&tmp, s[x], sizeof(tmp));
		if (s[x]->date) {
			tmp.date = (char *)malloc(strlen(s[x]->date));
			strcpy(tmp.date, s[x]->date);
		}

		data.push_back(tmp);
	}

	if (length > 0)
		dirty = true;
}

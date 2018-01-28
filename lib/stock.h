/**
 * @todo Rename this file from "stock.h" to "stockinfo.h"
 */
#include <vector>
#include <ctime>

#ifndef _stock_h
#define _stock_h
struct stock {
	char *date; // TODO: Remove.
	time_t timestamp; // Preferred over date.
	double open;
	double high;
	double low;
	double close;
	long volume;
};

enum timeperiods {day = 0, week = 1, month = 2, year = 3};

class stockinfo {
	friend class config;
	public:
		stockinfo(const stockinfo &init);
		stockinfo(): orig_filename(nullptr), dirty(false) {};
		~stockinfo();
		bool load_csv(const char *filename);
		bool save_csv(const char *filename = nullptr);
		stockinfo &insert_at(const struct stock s, const long pos = 0);
		const long length() const;
		const long length();

		struct stock shift();

		const struct stock *operator [](const long index) const;
		const struct stock *operator [](const long index);
		stockinfo &operator +=(const struct stock s);
		stockinfo &operator =(const stockinfo &s);

		stockinfo &uniq();
		stockinfo &sort();

		template<class T>
		stockinfo rollup_iterator(T &iterator, int number);
		stockinfo rollup(int number = 1, timeperiods period = week, bool align_week = false);
		stockinfo weekly(bool align_week = false);

	private:
		time_t parse_csv_time(const char *date);
		void nosig();
		void sig();
		void copy(const stockinfo &s);

		template<class T>
		T &multi_decrement(T &iterator, int number = 1);

		std::vector<struct stock> data;
		char *orig_filename;
		bool dirty;
};
#endif

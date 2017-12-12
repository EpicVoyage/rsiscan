#include <vector>
#include "lib/stock.h"

#ifndef _config_h
#define _config_h
class config {
	public:
		// @ref https://stackoverflow.com/a/5424521/850782
		template <class T>
    class proxy {
        friend class config;
    private:
        T data;
        T operator=(const T& arg) { data = arg; return data; }
				//operator T&() { return data; }
    public:
        operator const T&() const { return data; }
    };

		proxy<char*> home, stock_dir, old_dir, list_dir, config_dir;
		std::vector<char *> tickers;
		bool save_config;

		config();
		~config();
		bool load(const char *identifier, stockinfo &in);
		bool save(const char *identifier, stockinfo &in);
		void delist(const char *identifier);

		char *get_filename(const char *identifier);

	private:
		time_t parse_csv_time(const char *date);
		void create_dir(const char *path);
		void nosig();
		void sig();
		void copy(const stockinfo &s);
};
#endif

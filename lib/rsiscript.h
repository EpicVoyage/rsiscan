#include <string>
#include "lib/stock.h"

#ifndef _rsiscript_h
#define _rsiscript_h
class rsiscript {
public:
	std::string parse(const char* const script, const stockinfo &data);

private:
	std::string replace_variables(const std::string &script, const stockinfo &data);
	std::string variables(const std::string var, const stockinfo &data);
	std::string exec_script_operations(const std::string &script, const char *operators);
	const std::string exec_script_calculate(const std::string &script);

	template<typename T>
	std::string exec_script_calculate_operation(const T val1, const T val2, char operation);
};
#endif

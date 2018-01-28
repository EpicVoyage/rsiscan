#include <vector>
#include <string>
#include "lib/stock.h"

#ifndef _rsiscript_h
#define _rsiscript_h
class rsiscript {
public:
	// Public interfaces.
	std::string parse(const char* const script, const stockinfo &data);
	std::string last_variables;

private:
	// Standard class functions.
	std::string replace_variables(const std::string &script, const stockinfo &data);
	std::string variables(const std::string &req, const stockinfo &data);
	std::string exec_script_operations(const std::string &script, const char *operators);
	const std::string exec_script_calculate(const std::string &script);

	// Function templates.
	template<typename T>
	std::string last_variable(const std::string &name, T value);

	template<typename T>
	std::string exec_script_calculate_operation(const T val1, const T val2, char operation);

	template<class ContainerT>
	void tokenize(const std::string& str, ContainerT& tokens,
              const std::string& delimiters = " ", bool trimEmpty = false);

	// Variables.
	std::vector<std::string> last_variables_used;
};
#endif

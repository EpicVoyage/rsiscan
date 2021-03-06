#include <ctype.h>
#include <regex>
#include <algorithm>

#include <boost/log/trivial.hpp>

#include "lib/stats/relative_strength_index.h"
#include "lib/stats/exponential_moving_average.h"
#include "lib/stats/simple_moving_average.h"
#include "lib/stats/bollinger.h"
#include "lib/rsiscript.h"

/**
 * Parse a math-related script. There are several phases.
 *
 * 1. Replace variables.
 * 2. Identify sections in parenthesis. Process these first.
 * 3. Process the math equations/comparisons.
 *
 * @param const char *script [ex: (1+3)/(2 * (4 + 6))]
 * @return const char *result
 */
int script_max_paren_depth = 50;
std::string rsiscript::parse(const char* const script, const stockinfo &data) {
	std::string err = "0", repl, expr = script;
	std::size_t pos, lparen_pos, rparen_pos = 0;
	unsigned int lparens, rparens;
	bool found = false;

	last_variables_used.clear();
	last_variables.clear();
	if (script == nullptr)
		return err;

	BOOST_LOG_TRIVIAL(trace) << "Script: " << script;
	expr = replace_variables(expr, data);

	do {
		// Search for a ')'.
		pos = rparen_pos = expr.find(")", rparen_pos);
		found = (rparen_pos != std::string::npos);

		// Find the matching '('.
		if (found) {
			rparens = 1;
			lparens = 0;

			while (pos > 0) {
				pos--;

				if (expr[pos] == ')') // TODO: Should not be possible.
					rparens++;
				else if (expr[pos] == '(') {
					lparens++;

					// Will only happen with a '(' find.
					if (lparens == rparens) {
						lparen_pos = pos;
						break;
					}
				}
			}

			// ERROR: Mismatched parenthesis.
			if (rparens != lparens) {
				BOOST_LOG_TRIVIAL(error) << "Mismatched parenthesis: " << script;
				printf("ERROR: Mismatched parenthesis: %s\n", script);
				return err;
			}

			// Parse the substring. Do not include the current parenthesis set.
			repl = parse(expr.substr(lparen_pos + 1, rparen_pos - lparen_pos - 1).c_str(), data);
			repl = exec_script_calculate(repl);
			expr.replace(lparen_pos, rparen_pos - lparen_pos + 1, repl);

			rparen_pos -= (rparen_pos - lparen_pos - repl.length());
		}
	} while (found);

	expr = exec_script_calculate(expr);
	BOOST_LOG_TRIVIAL(info) << "End of run: " << expr;
	return expr;
}

/**
 * Find/replace the variables in our script.
 *
 * @return string
 */
std::string rsiscript::replace_variables(const std::string &script, const stockinfo &data) {
	std::string expr = script;
	std::string err = "0", repl;
	std::size_t pos, lparen_pos, rparen_pos = 0;
	unsigned int lparens, rparens;
	bool found = false;

	do {
		// Search for a ')'.
		pos = rparen_pos = expr.find("}", rparen_pos);
		found = (rparen_pos != std::string::npos);

		// Find the matching '('.
		if (found) {
			rparens = 1;
			lparens = 0;

			while (pos > 0) {
				pos--;

				if (expr[pos] == '}') // TODO: Should not be possible.
					rparens++;
				else if (expr[pos] == '{') {
					lparens++;

					// Will only happen with a '(' find.
					if (lparens == rparens) {
						lparen_pos = pos;
						break;
					}
				}

				/*if (lparens > script_max_paren_depth) {
					BOOST_LOG_TRIVIAL(error) << "Found too many opening parens: " << lparens;
					printf("Found too many opening parens: %iu\n", lparens);
					return nullptr;
				}*/
			}

			// ERROR: Mismatched parenthesis.
			if (rparens != lparens) {
				BOOST_LOG_TRIVIAL(error) << "Mismatched brackets: " << script;
				printf("ERROR: Mismatched brackets: %s\n", script.c_str());
				return err;
			}

			// Parse the substring. Do not include the current parenthesis set.
			repl = replace_variables(expr.substr(lparen_pos + 1, rparen_pos - lparen_pos - 1), data);
			BOOST_LOG_TRIVIAL(trace) << "Replacing: " << repl;
			repl = variables(repl, data);
			// TODO: Replace all instances of this same variable?
			expr.replace(lparen_pos, rparen_pos - lparen_pos + 1, repl);

			rparen_pos -= (rparen_pos - lparen_pos - repl.length());
		}
	} while (found);

	BOOST_LOG_TRIVIAL(trace) << "Replaced variables: " << expr;

	return expr;
}

/**
 * Parse script variables into values.
 *
 * Format: high
 * Format: rsi
 * Format: high:52wk
 * Format: rsi:52wk(28)
 *
 * @param string req The variable to process.
 * @param stockinfo data The stock data to use in processing req.
 * @return string The script without any more variables.
 */
std::string rsiscript::variables(const std::string &req, const stockinfo &data) {
	std::string ret = "0";
	std::vector<std::string> tokens;
	stockinfo *working_data = (stockinfo *)&data;
	stockinfo week_data;

	tokenize(req, tokens, ":", true);
	BOOST_LOG_TRIVIAL(trace) << "Variable tokens: " << tokens.size();

	// TODO: Parse options for stat variables. Weekly, monthly, RSI/SMA/EMA/BB/&c.
	std::string period;
	if (tokens.size() >= 2) {
		period = tokens[1];
	}
	if (period.length()) {
		int number;
		timeperiods tp;
		parse_period(period, number, tp);

		week_data = working_data->rollup(number, tp);
		working_data = &week_data;
	}

	if (!(*working_data).length()) {
		return ret;
	}

	// Process the requested variable.
	std::string var = tokens[0];
	if (var.compare("open") == 0) {
		ret = last_variable(req, (*working_data)[0]->open);
	}
	else if (var.compare("high") == 0) {
		ret = last_variable(req, (*working_data)[0]->high);
	}
	else if (var.compare("low") == 0) {
		ret = last_variable(req, (*working_data)[0]->low);
	}
	else if (var.compare("close") == 0) {
		ret = last_variable(req, (*working_data)[0]->close);
	}
	else if (var.compare("volume") == 0) {
		ret = last_variable(req, (*working_data)[0]->volume);
	}
	else if (var.compare("rsi") == 0) {
		relative_strength_index rsi;
		double *rsi_data = rsi.generate(*working_data, 14, (*working_data).length() - 26);
		ret = last_variable(req, *rsi_data);
		free(rsi_data);
	}
	else if (var.compare("sma") == 0) {
		simple_moving_average sma;
		double *sma_data = sma.generate(*working_data, 20, (*working_data).length() - 26);
		ret = last_variable(req, *sma_data);
		free(sma_data);
	}
	else if (var.compare("ema") == 0) {
		simple_moving_average ema;
		double *ema_data = ema.generate(*working_data, 20, (*working_data).length() - 26);
		ret = last_variable(req, *ema_data);
		free(ema_data);
	}
	else if (var.compare("bb_top") == 0) {
		simple_moving_average sma;
		bollinger bb;
		double *sma_data = sma.generate(*working_data, 20, (*working_data).length() - 26);
		double *bb_data = bb.bands(*working_data, 14, (*working_data).length() - 26);
		ret = last_variable(req, *sma_data + *bb_data);
		free(sma_data);
		free(bb_data);
	}
	else if (var.compare("bb_bottom") == 0) {
		simple_moving_average sma;
		bollinger bb;
		double *sma_data = sma.generate(*working_data, 20, (*working_data).length() - 26);
		double *bb_data = bb.bands(*working_data, 14, (*working_data).length() - 26);
		ret = last_variable(req, *sma_data - *bb_data);
		free(sma_data);
		free(bb_data);
	}

	return ret;
}

/**
 * Parse a string like "52weeks" roughly into "52" and "week." It would have been nice to use regex for this, but the
 * regex parser does not appear to work in this dev's compiler.
 *
 * @todo We could refactor this to place number & period into a struct.
 *
 * @param req String
 * @param number Return value.
 * @param period Return value.
 */
void rsiscript::parse_period(std::string req, int &number, timeperiods &period) {
	std::istringstream i(req);
	i >> number;

	if (!number) {
		number = 1;
	}

	if (req.compare("week"))
		period = week;
	else if (req.compare("month"))
		period = month;
	else if (req.compare("year"))
		period = year;
	else //if (req.compare("day"))
		period = day;

	return;
}

/**
 * Store name + value in last_variables.
 *
 * @param name
 * @param value
 * @return string value
 */
template<typename T>
std::string rsiscript::last_variable(const std::string &name, T value) {
	std::string ret = std::to_string(value);
	bool add = !last_variables_used.size();

	if (!add) {
		add = std::find(last_variables_used.begin(), last_variables_used.end(), name) == last_variables_used.end();
	}

	if (add) {
		if (last_variables.length()) {
			last_variables += ", ";
		}
		last_variables += name + " = " + ret;
		last_variables_used.push_back(name);
	}

	return ret;
}

/**
 * Direct the order of operator usage.
 *
 * Phase 1: ^
 * Phase 2: * / %
 * Phase 3: + -
 * Phase 4: > < =
 * Phase 5: | &
 *
 * @todo >= <=
 *
 * @return string The value calculated.
 */
const std::string rsiscript::exec_script_calculate(const std::string &script) {
	std::string ret;

	BOOST_LOG_TRIVIAL(trace) << "Script chunk: " << script;

	// Operational passes. The terminating 0's allow us to use string functions.
	const char first_pass[] = {'^', 0};
	const char second_pass[] = {'*', 'x', '/', '%', 0};
	const char third_pass[] = {'+', '-', 0};
	const char fourth_pass[] = {'>', '<', '=', 0};
	const char fifth_pass[] = {'|', '&', 0};

	// Pass over the string three times to give us proper order of operation.
	ret = exec_script_operations(script, first_pass);
	ret = exec_script_operations(ret, second_pass);
	ret = exec_script_operations(ret, third_pass);
	ret = exec_script_operations(ret, fourth_pass);
	ret = exec_script_operations(ret, fifth_pass);

	BOOST_LOG_TRIVIAL(trace) << "Script chunk reduced: " << ret;

	return ret;
}

/**
 * Find numbers and their operators for direct calculation.
 *
 * @return string The calculated value(s).
 */
std::string rsiscript::exec_script_operations(const std::string &script, const char *operators) {
	std::size_t sc_len = script.length();
	const char digits[] = "0123456789.";
	std::size_t x, start, operation, end;
	bool isdigit, calc_end, calc_start = false;
	long num1l, num2l;
	double num1d, num2d;
	std::string ret = script, result, num1str, num2str;

	for (x = 0; x < sc_len; x++) {
		/**
		 * This logic assumes well-formed expressions: 4+(*1+2) => 4+*3 => 4+3 => 7.
		 */

		// Ignore space characters.
		if (isspace(ret[x]))
			continue;

		// Mark the beginning of numbers as we work through the string.
		isdigit = (strchr(digits, ret[x]) != NULL);
		if (!calc_start && isdigit) {
			calc_start = true;
			start = x;
		}
		// After we find a number, find an operation.
		else if (calc_start && (strchr(operators, ret[x]) != NULL)) {
			operation = x;
			calc_end = false;

			// Is there another number after the operator?
			// TODO: Allow negative numbers.
			for (x++; x < sc_len; x++) {
				if (strchr(digits, ret[x]) != NULL) {
					calc_end = true;
					break;
				}
			}

			// If we found another number...
			if (calc_end) {
				// Find the end of the number;
				end = sc_len;
				for (x++; x < sc_len; x++) {
					if (strchr(digits, ret[x]) == NULL) {
						end = x - 1;
						break;
					}
				}

				// Pull this expression apart.
				num1str = ret.substr(start, operation - start);
				num2str = ret.substr(operation + 1, end - operation);

				BOOST_LOG_TRIVIAL(trace) << "Script operation: " << num1str << ", " << ret[operation] << ", " << num2str;

				// If this is a long/integer operation
				if ((num1str.find(".") == std::string::npos) && (num2str.find(".") == std::string::npos)) {
					num1l = atol(num1str.c_str());
					num2l = atol(num2str.c_str());

					result = exec_script_calculate_operation(num1l, num2l, ret[operation]);
				} else {
					num1d = atof(num1str.c_str());
					num2d = atof(num2str.c_str());

					result = exec_script_calculate_operation(num1d, num2d, ret[operation]);
				}

				ret.replace(start, end - start + 1, result);
				sc_len = ret.length();
				x = start; // Re-process this result since it might start the next calculation.

				BOOST_LOG_TRIVIAL(trace) << "Script operation result: " << ret;
			} // End calculation.

			//calc_start = false;
		} // End operator section.
		else if (!isdigit) {
			// If this is not a valid digit, a space, or in the current operations list, start over.
			calc_start = false;
		}
	} // Next character.

	return ret;
}

/**
 * Takes two numeric (int/double) values and a comparison operator. Performs the required operation.
 *
 * @return int/double
 */
template<typename T>
std::string rsiscript::exec_script_calculate_operation(const T val1, const T val2, char operation) {
	double res;
	T result;

	switch(operation) {
		case '*':
		case 'x':
			result = val1 * val2;
			break;
		case '/':
			res = (double)val1 / (double)val2;
			break;
		case '+':
			result = val1 + val2;
			break;
		case '-':
			result = val1 - val2;
			break;
		case '^':
			result = pow(val1, val2);
			break;
		case '%':
			result = remainder(val1, val2);
			break;
		case '>':
			result = (val1 > val2) ? 1 : 0;
			break;
		case '<':
			result = (val1 < val2) ? 1 : 0;
			break;
		case '=':
			result = (val1 == val2) ? 1 : 0;
			break;
		case '|':
			result = (val1 || val2) ? 1 : 0;
			break;
		case '&':
			result = (val1 && val2) ? 1 : 0;
			break;
		default:
			result = 0;
			BOOST_LOG_TRIVIAL(error) << "Unknown script operation: " << operation;
	}

	// Force (double) for division.
	return (operation == '/') ? std::to_string(res) : std::to_string(result);
}

/**
 * Split a string.
 *
 * @link https://stackoverflow.com/a/1493195/850782
 *
 * @param string str
 * @param tokens The return structure.
 * @param string delimiters The characters to split for (default: " ").
 * @param bool trimEmpty Should empty values be skipped (default: true)?
 * @return None. Use tokens.
 */
template<class ContainerT>
void rsiscript::tokenize(const std::string& str, ContainerT& tokens,
				const std::string& delimiters, bool trimEmpty)
{
	std::string::size_type pos, lastPos = 0, length = str.length();

	using value_type	= typename ContainerT::value_type;
	using size_type		= typename ContainerT::size_type;

	while(lastPos < length + 1)
	{
		pos = str.find_first_of(delimiters, lastPos);
		if(pos == std::string::npos)
		{
			pos = length;
		}

		if(pos != lastPos || !trimEmpty)
			tokens.push_back(value_type(str.data()+lastPos,
					(size_type)pos-lastPos ));

		lastPos = pos + 1;
	}
}

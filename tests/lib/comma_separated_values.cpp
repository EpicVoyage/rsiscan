#include "lib/third_party/catch2/catch.hpp"
#include "lib/comma_separated_values.h"
using namespace Catch;

TEST_CASE("Parse 5 columns into a stock struct", "[csv]") {
	comma_separated_values csv;
	long rows;
	struct stock *values = csv.parse("1date,4,10,1,3,100000\r\n2date,7,10,1,8,50", &rows);

	REQUIRE(rows == 2);
	REQUIRE_THAT(values[0].date, Equals("1date"));
	REQUIRE_THAT(values[1].date, Equals("2date"));
	REQUIRE(values[1].volume == 50);
}

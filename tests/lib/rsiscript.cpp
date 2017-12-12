#include "lib/third_party/catch2/catch.hpp"
#include "lib/rsiscript.h"
#include "lib/stock.h"
using namespace Catch;

TEST_CASE("Test basic mathematical calculations", "[script]") {
	stockinfo si;
	rsiscript rs;

	// Basic test.
	REQUIRE_THAT(rs.parse("5+2", si).c_str(), Equals("7"));

	// Order of operation. Test both multiplication forms.
	REQUIRE_THAT(rs.parse("6^2+5*4x3/2-1", si).c_str(), Equals("65.000000"));

	// Order of operation with parenthesis.
	REQUIRE_THAT(rs.parse("(1+3 * 10)/(2 * (4 +6))", si).c_str(), Equals("1.550000"));

	// Comparison operators.
	REQUIRE_THAT(rs.parse("(1 > 2) | (2 < 80)", si).c_str(), Equals("1"));
	REQUIRE_THAT(rs.parse("(1 > 2) & (2 < 80)", si).c_str(), Equals("0"));
}

TEST_CASE("Test variable replacement", "[script]") {
	struct stock s;
	stockinfo si;
	rsiscript rs;

	// Create the first element.
	s.date = (char *)malloc(11);
	strcpy(s.date, "10-10-2017");
	s.open = 10;
	s.high = 12;
	s.low = 8;
	s.close = 9;
	s.volume = 150000;
	si.insert_at(s);

	// Basic test.
	REQUIRE_THAT(rs.parse("{volume} > 1000000", si).c_str(), Equals("0"));
	REQUIRE_THAT(rs.parse("{volume} > 100000", si).c_str(), Equals("1"));
}

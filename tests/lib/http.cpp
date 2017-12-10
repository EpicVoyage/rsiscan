#include "lib/third_party/catch2/catch.hpp"
#include "lib/http.h"
using namespace Catch;

TEST_CASE("Retrieve stock information from Google", "[http]") {
	http h;
	REQUIRE_THAT(h.retrieve("finance.google.com", "/finance/historical?q=GOOG&output=csv"), Contains(",")); //Matches("^[^,]+,[^,]+,[^,]+,[^,]+,[^,]+,[^,]+\n.*"));
}

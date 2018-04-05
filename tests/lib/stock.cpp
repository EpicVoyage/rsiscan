#include "lib/third_party/catch2/catch.hpp"
#include "lib/stock.h"
using namespace Catch;

TEST_CASE("Load goog.csv", "[stockinfo,csv]") {
	stockinfo si;
	bool success;

	// @todo Fix filename.
	success = si.load_csv("/Users/chris/.rsiscan/data/goog.csv");

	// Valid test cases.
	REQUIRE(success == true);
	REQUIRE(si.length() > 10);
	REQUIRE(si[0]->volume > 1);
	REQUIRE(si[si.length()-1]->volume > 1);

	// Invalid entry.
	REQUIRE(si[si.length()] == nullptr);
}

TEST_CASE("Load non-existant file", "[stockinfo,csv]") {
	stockinfo si;
	bool success;

	// @todo Fix filename.
	success = si.load_csv("non-existant-file-goo.csv");

	// Validate that no data exists in our class.
	REQUIRE(success == false);
	REQUIRE(si.length() == 0);
	REQUIRE(si[0] == nullptr);
}

TEST_CASE("Manually build the data", "[stockinfo]") {
	stockinfo si;
	struct stock s, t, u, v;

	// Create the first element.
	s.date = (char *)malloc(11);
	strcpy(s.date, "10-10-2017");
	s.volume = 2;
	si.insert_at(s);

	// Insert at the beginning.
	t.date = (char *)malloc(11);
	strcpy(t.date, "09-09-2017");
	t.volume = 1;
	si.insert_at(t);

	// Manually insert at the end.
	u.date = (char *)malloc(11);
	strcpy(u.date, "11-11-2017");
	u.volume = 3;
	si.insert_at(u, 2);

	// Insert at the end by operator.
	v.date = (char *)malloc(11);
	strcpy(v.date, "12-12-2017");
	v.volume = 4;
	si += v;

	// Validate the data in our class.
	REQUIRE(si.length() == 4);
	REQUIRE(si[0]->volume == 1);
	REQUIRE(si[1]->volume == 2);
	REQUIRE(si[2]->volume == 3);
	REQUIRE(si[3]->volume == 4);
	REQUIRE(si[4] == nullptr);
}

TEST_CASE("Test assignment", "[stockinfo]") {
	stockinfo si, sj, *pi, *pj;
	struct stock s;

	// Create the first element.
	s.date = (char *)malloc(11);
	strcpy(s.date, "10-10-2017");
	s.volume = 2;
	si.insert_at(s);

	// Assignment
	sj = si;
	pi = &si;
	pj = pi;

	// Validate the data in our class.
	REQUIRE(sj.length() == 1);
	REQUIRE(sj[0]->volume == 2);
	REQUIRE(sj[1] == nullptr);

	REQUIRE(pj->length() == 1);
	REQUIRE((*pj)[0]->volume == 2);
	REQUIRE((*pj)[1] == nullptr);
}

TEST_CASE("Test weekly roll-ups", "[stockinfo]") {
	stockinfo si, sj;
	struct stock s, t, u, v, w;

	// Create the first element.
	s.date = (char *)malloc(11);
	strcpy(s.date, "2017-10-09");
	s.open = 2;
	s.high = 4;
	s.low = 2;
	s.close = 3;
	s.volume = 2;
	si.insert_at(s);

	// Insert at the beginning.
	t.date = (char *)malloc(11);
	strcpy(t.date, "2017-10-10");
	t.open = 4;
	t.high = 6;
	t.low = 3;
	t.close = 4;
	t.volume = 1;
	si.insert_at(t);

	// Manually insert at the end.
	u.date = (char *)malloc(11);
	strcpy(u.date, "2017-10-11");
	u.open = 3;
	u.high = 7;
	u.low = 3;
	u.close = 3;
	u.volume = 3;
	si.insert_at(u, 2);

	// Insert at the end by operator.
	v.date = (char *)malloc(11);
	strcpy(v.date, "2017-10-14");
	v.open = 2;
	v.high = 3;
	v.low = 1;
	v.close = 3;
	v.volume = 4;
	si += v;

	// Insert at the end by operator.
	w.date = (char *)malloc(11);
	strcpy(w.date, "2017-10-18");
	w.open = 15;
	w.high = 23;
	w.low = 15;
	w.close = 23;
	w.volume = 5;
	si += w;

	// Test = operator and weekly roll-up.
	sj = si.weekly(true);

	// Validate the data in our class.
	REQUIRE(sj.length() == 2);
	REQUIRE(sj[0]->open == 15);
	REQUIRE(sj[0]->high == 23);
	REQUIRE(sj[0]->low == 15);
	REQUIRE(sj[0]->close == 23);
	REQUIRE(sj[0]->volume == 5);
	REQUIRE(sj[1]->open == 2);
	REQUIRE(sj[1]->high == 7);
	REQUIRE(sj[1]->low == 1);
	REQUIRE(sj[1]->close == 3);
	REQUIRE(sj[1]->volume == 10);
	REQUIRE(sj[2] == nullptr);
}

TEST_CASE("Test 4-week roll-ups", "[stockinfo]") {
	stockinfo si, sj;
	struct stock s, t, u, v, w;

	// Create the first element.
	s.date = (char *)malloc(11);
	strcpy(s.date, "2017-09-01");
	s.open = 2;
	s.high = 4;
	s.low = 2;
	s.close = 3;
	s.volume = 2;
	si.insert_at(s);

	// Insert at the beginning.
	t.date = (char *)malloc(11);
	strcpy(t.date, "2017-09-02");
	t.open = 4;
	t.high = 6;
	t.low = 3;
	t.close = 4;
	t.volume = 1;
	si.insert_at(t);

	// Manually insert at the end.
	u.date = (char *)malloc(11);
	strcpy(u.date, "2017-09-03");
	u.open = 3;
	u.high = 7;
	u.low = 3;
	u.close = 3;
	u.volume = 3;
	si.insert_at(u, 2);

	// Insert at the end by operator.
	v.date = (char *)malloc(11);
	strcpy(v.date, "2017-09-29");
	v.open = 2;
	v.high = 3;
	v.low = 1;
	v.close = 3;
	v.volume = 4;
	si += v;

	// Insert at the end by operator.
	w.date = (char *)malloc(11);
	strcpy(w.date, "2017-09-30");
	w.open = 15;
	w.high = 23;
	w.low = 15;
	w.close = 23;
	w.volume = 5;
	si += w;

	// Test = operator and weekly roll-up.
	sj = si.rollup(4, week, true);

	// Validate the data in our class.
	REQUIRE(sj.length() == 2);
	REQUIRE(sj[0]->open == 2);
	REQUIRE(sj[0]->high == 23);
	REQUIRE(sj[0]->low == 1);
	REQUIRE(sj[0]->close == 23);
	REQUIRE(sj[0]->volume == 9);
	REQUIRE(sj[1]->open == 2);
	REQUIRE(sj[1]->high == 7);
	REQUIRE(sj[1]->low == 2);
	REQUIRE(sj[1]->close == 3);
	REQUIRE(sj[1]->volume == 6);
	REQUIRE(sj[2] == nullptr);
}
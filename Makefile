CXX=clang++
CXXVERSION=c++1y
LIBLOCATIONS=-L. -L/usr/local/Cellar/boost/1.65.1/lib
LIBS=-lm -lboost_log-mt -lboost_log_setup-mt -lboost_thread-mt -lboost_system-mt
BIN=rsiscan
MACOS=-D_DARWIN_C_SOURCE
CXXFLAGS=-DBOOST_LOG_DYN_LINK -g -I. -I/usr/local/Cellar/boost/1.65.1/include -std=$(CXXVERSION) -Wall -pedantic $(MACOS)
SOURCE=lib/http.o lib/comma_separated_values.o lib/stock.o lib/stats/relative_strength_index.o lib/stats/bollinger.o lib/stats/simple_moving_average.o lib/stats/moving_average_convergence_divergence.o lib/stats/exponential_moving_average.o lib/stats/high.o lib/stats/low.o
TESTS=tests/main.o tests/lib/http.o tests/lib/comma_separated_values.o tests/lib/stock.o

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS)

rsiscan: lib rsiscan.o
	$(CXX) $(CXXFLAGS) $(LIBLOCATIONS) $(LIBS) -lrsiscan -o $(BIN) rsiscan.o

lib: $(SOURCE)
	$(CXX) -shared $(CXXFLAGS) $(LIBLOCATIONS) $(LIBS) -o lib$(BIN).so $(SOURCE)

test: lib $(TESTS)
	$(CXX) $(CXXFLAGS) $(LIBLOCATIONS) $(LIBS) -lrsiscan -o tests/runall $(TESTS)
	./tests/runall

tests: test

all: rsiscan test

clean:
	rm -f $(BIN) lib$(BIN).so tests/runall *.o lib/*.o lib/stats/*.o runall/*.o
	rm -rf *.dSYM tests/*.dSYM

distclean: clean

# Set our basic expectations for CMake.
CMAKE_MINIMUM_REQUIRED(VERSION 2.8 FATAL_ERROR)
CMAKE_POLICY(SET CMP0015 NEW)
CMAKE_POLICY(SET CMP0048 NEW)

# Declare ourselves.
PROJECT(rsiscan VERSION 1.0.0 DESCRIPTION "Stock Screener")

# Find and configure the Boost library.
SET(Boost_USE_STATIC_LIBS OFF)
SET(Boost_USE_MULTITHREADED ON)
SET(Boost_USE_STATIC_RUNTIME OFF)
add_definitions(-DBOOST_LOG_DYN_LINK)
FIND_PACKAGE(Boost REQUIRED COMPONENTS log log_setup thread system)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})

# Configure ourselves as sources for headers and libraries.
INCLUDE_DIRECTORIES(".")
LINK_DIRECTORIES(".")

# Create a library that can be linked against.
ADD_LIBRARY(rsiscan SHARED
        lib/config.cpp lib/rsiscript.cpp lib/http.cpp lib/comma_separated_values.cpp lib/stock.cpp lib/stats/relative_strength_index.cpp lib/stats/bollinger.cpp lib/stats/simple_moving_average.cpp lib/stats/moving_average_convergence_divergence.cpp lib/stats/exponential_moving_average.cpp lib/stats/high.cpp lib/stats/low.cpp lib/config.h lib/rsiscript.h lib/http.h lib/comma_separated_values.h lib/stock.h lib/stats/relative_strength_index.h lib/stats/bollinger.h lib/stats/simple_moving_average.h lib/stats/moving_average_convergence_divergence.h lib/stats/exponential_moving_average.h lib/stats/high.h lib/stats/low.h)
TARGET_LINK_LIBRARIES(rsiscan
        ${Boost_LIBRARIES}
        m)
TARGET_COMPILE_OPTIONS(rsiscan PUBLIC -std=c++1y -Wall -pedantic -D_DARWIN_C_SOURCE)
SET_TARGET_PROPERTIES(rsiscan PROPERTIES VERSION ${PROJECT_VERSION} COMPILE_DEFINITIONS "BOOST_LOG_DYN_LINK")

# Create a command-line scanner.
ADD_EXECUTABLE(rsiscan-bin rsiscan.cpp)
TARGET_LINK_LIBRARIES(rsiscan-bin rsiscan)
TARGET_COMPILE_OPTIONS(rsiscan-bin PUBLIC -std=c++1y -Wall -pedantic -D_DARWIN_C_SOURCE)
SET_TARGET_PROPERTIES(rsiscan-bin PROPERTIES OUTPUT_NAME rsiscan)

# Create an executable for the unit tests.
ADD_EXECUTABLE(runall tests/main.cpp tests/lib/rsiscript.cpp tests/lib/http.cpp tests/lib/comma_separated_values.cpp tests/lib/stock.cpp)
TARGET_LINK_LIBRARIES(runall rsiscan)
TARGET_COMPILE_OPTIONS(runall PUBLIC -std=c++1y -Wall -pedantic -D_DARWIN_C_SOURCE)
SET_TARGET_PROPERTIES(runall PROPERTIES OUTPUT_NAME tests/runall)

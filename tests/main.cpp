#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#define CATCH_CONFIG_RUNNER
#include "lib/third_party/catch2/catch.hpp"
#include "lib/config.h"

int main( int argc, char* argv[] ) {
	config conf;

  // Configure log file output.
	// TODO: Severity does not appear in the logs.
	char *logfile = (char *)malloc(strlen(conf.config_dir) + 13);
	sprintf(logfile, "%s/rsiscan.log", (char *)conf.config_dir);
	boost::log::add_common_attributes();
	boost::log::expressions::attr< boost::log::trivial::severity_level >("Severity");
	boost::log::add_file_log(
			boost::log::keywords::file_name = logfile,
			boost::log::keywords::format = "[%TimeStamp%] [%Severity%]: %Message%"
	);

  int result = Catch::Session().run( argc, argv );

  return result;
}

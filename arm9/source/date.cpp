#include "date.h"

#include "language.h"

#include <stdio.h>
#include <string>
#include <time.h>

/**
 * Get the current time formatted for the top bar.
 * @return std::string containing the time.
 */
std::string RetTime()
{
	time_t raw;
	time(&raw);
	const struct tm *Time = localtime(&raw);

	char tmp[8];
	strftime(tmp, sizeof(tmp), STR_TIME_FORMAT.c_str(), Time);

	return tmp;
}

/**
 * Get the current time formatted as specified.
 * @return std::string containing the time.
 */
std::string RetTime(const char *format)
{
	time_t raw;
	time(&raw);
	const struct tm *Time = localtime(&raw);

	char tmp[64];
	strftime(tmp, sizeof(tmp), format, Time);

	return tmp;
}

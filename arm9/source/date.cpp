#include "date.h"

#include "language.h"

#include <stdio.h>
#include <string>
#include <time.h>

/**
 * Get the current time formatted as specified.
 * @return std::string containing the time.
 */
std::string RetTime(const char *format, time_t *raw)
{
	if (!format) 
	{
		format = STR_TIME_FORMAT.c_str();
	}
	time_t systime;
	if (!raw) {
		raw = &systime;
		time(raw);
	}
	const struct tm *Time = localtime(raw);

	char tmp[64];
	strftime(tmp, sizeof(tmp), format, Time);

	return tmp;
}

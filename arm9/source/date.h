#ifndef DATE_H
#define DATE_H

#include <string>

/**
 * Format the time as specified.
 * If no format is specified, use format for the top bar.
 * If no time is specified, use the system time.
 * @return std::string containing the time.
 */
std::string RetTime(const char *format = nullptr, time_t *raw = nullptr);

#endif // DATE_H

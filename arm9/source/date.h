#ifndef DATE_H
#define DATE_H

#include <string>

/**
 * Get the current time formatted for the top bar.
 * @return std::string containing the time.
 */
std::string RetTime();

/**
 * Get the current time formatted as specified.
 * @return std::string containing the time.
 */
std::string RetTime(const char *format);

#endif // DATE_H

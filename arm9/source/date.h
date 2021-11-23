#ifndef DATE_H
#define DATE_H

#include <string>

/**
 * Get the current time formatted for the top bar.
 * @return std::string containing the time.
 */
std::string RetTime();

/**
 * Get the current time formatted for filenames.
 * @return std::string containing the time.
 */
std::string RetTimeForFilename();

#endif // DATE_H

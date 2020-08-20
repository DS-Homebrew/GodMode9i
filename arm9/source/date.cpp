#include "date.h"

#include <ctime>
#include <cstdio>
#include <malloc.h>

#include <string>
using std::string;

/**
 * Get the current date as a C string.
 * @param format Date format.
 * @param buf Output buffer.
 * @param size Size of the output buffer.
 * @return Number of bytes written, excluding the NULL terminator.
 * @return Current date. (Caller must free() this string.)
 */
size_t GetDate(DateFormat format, char *buf, size_t size)
{
	time_t Raw;
	time(&Raw);
	const struct tm *Time = localtime(&Raw);

	switch (format) {
		case FORMAT_YDM:
			return strftime(buf, size, "%Y-%d-%m_%k-%M", Time);
		case FORMAT_YMD:
			return strftime(buf, size, "%Y-%m-%d_%k-%M", Time);
		case FORMAT_DM:
			return strftime(buf, size, "%d/%m", Time); // Ex: 26/12
		case FORMAT_MD:
			return strftime(buf, size, "%m/%d", Time); // Ex: 12/26
		case FORMAT_M_D:
			return strftime(buf, size, "%d.%m.", Time); // Ex: 26.12.
		case FORMAT_MY:
			return strftime(buf, size, "%m  %Y", Time);
		case FORMAT_M:
			return strftime(buf, size, "%m", Time);
		case FORMAT_Y:
			return strftime(buf, size, "%Y", Time);
		default:
			break;
	}

	// Invalid format.
	// Should not get here...
	if (size > 0) {
		*buf = 0;
	}
	return 0;
}

/**
 * Get the current time formatted for the top bar.
 * @return std::string containing the time.
 */
std::string RetTime()
{
	time_t Raw;
	time(&Raw);
	const struct tm *Time = localtime(&Raw);

	char Tmp[8];
	strftime(Tmp, sizeof(Tmp), "%k:%M", Time);

	return std::string(Tmp);
}

/**
 * Get the current time formatted for filenames.
 * @return std::string containing the time.
 */
std::string RetTimeForFilename()
{
	time_t Raw;
	time(&Raw);
	const struct tm *Time = localtime(&Raw);

	char Tmp[8];
	strftime(Tmp, sizeof(Tmp), "%k%M%S", Time);

	return std::string(Tmp);
}

/**
 * Draw the date using the specified format.
 * @param Xpos X position.
 * @param Ypos Y position.
 * @param size Text size.
 * @param format Date format.
 */
char* DrawDateF(DateFormat format)
{
	char date_str[24];
	GetDate(format, date_str, sizeof(date_str));
	if (date_str[0] == 0)
		return "";
	return date_str;
}

/**
 * Draw the date.
 * Date format depends on language setting.
 * @param screen Top or Bottom screen.
 * @param Xpos X position.
 * @param Ypos Y position.
 * @param size Text size.
 */
char* DrawDate()
{
	// Date formats.
	// - Index: Language ID.
	// - Value: Date format.
	static const uint8_t date_fmt[8] = {
		FORMAT_MD,	// Japanese
		FORMAT_MD,	// English
		FORMAT_DM,	// French
		FORMAT_M_D,	// German
		FORMAT_DM,	// Italian
		FORMAT_DM,	// Spanish
		FORMAT_MD,	// Chinese
		FORMAT_MD,	// Korean
	};

	return DrawDateF((DateFormat)date_fmt[PersonalData->language]);
}

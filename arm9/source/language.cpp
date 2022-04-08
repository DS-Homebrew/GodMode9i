#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <string>

#include "config.h"
#include "font.h"
#include "inifile.h"

#define STRING(what, def) std::string STR_##what;
#include "language.inl"
#undef STRING

bool rtl = false;
int firstCol =0;
int lastCol = -1;
Alignment alignStart = Alignment::left;
Alignment alignEnd = Alignment::right;

/**
 * Get strings from the ini with special processing
 */
std::string getString(CIniFile &ini, const std::string &item, const std::string &defaultValue) {
	std::string out = ini.GetString("LANGUAGE", item, defaultValue);

	// Convert "\n" to actual newlines
	for(uint i = 0; i < out.length() - 1; i++) {
		if(out[i] == '\\') {
			switch(out[i + 1]) {
				case 'n':
				case 'N':
					out = out.substr(0, i) + '\n' + out.substr(i + 2);
					break;
				case 'a':
				case 'A':
					out = out.substr(0, i) + (font->charExists(u'') ? "" : "<A>") + out.substr(i + 2); // U+E000
					break;
				case 'b':
				case 'B':
					out = out.substr(0, i) + (font->charExists(u'') ? "" : "<B>") + out.substr(i + 2); // U+E001
					break;
				case 'x':
				case 'X':
					out = out.substr(0, i) + (font->charExists(u'') ? "" : "<X>") + out.substr(i + 2); // U+E002
					break;
				case 'y':
				case 'Y':
					out = out.substr(0, i) + (font->charExists(u'') ? "" : "<Y>") + out.substr(i + 2); // U+E003
					break;
				case 'l':
				case 'L':
					out = out.substr(0, i) + (font->charExists(u'') ? "" : "<L>") + out.substr(i + 2); // U+E004
					break;
				case 'r':
				case 'R':
					out = out.substr(0, i) + (font->charExists(u'') ? "" : "<R>") + out.substr(i + 2); // U+E005
					break;
				case 'd':
				case 'D':
					switch(out[i + 2]) {
						default:
							out = out.substr(0, i) + (font->charExists(u'') ? "" : "←↑↓→") + out.substr(i + 2); // U+E006
							break;
						case 'u':
						case 'U':
							out = out.substr(0, i) + (font->charExists(u'') ? "" : "↑") + out.substr(i + 3); // U+E079
							break;
						case 'd':
						case 'D':
							out = out.substr(0, i) + (font->charExists(u'') ? "" : "↓") + out.substr(i + 3); // U+E07A
							break;
						case 'l':
						case 'L':
							out = out.substr(0, i) + (font->charExists(u'') ? "" : "←") + out.substr(i + 3); // U+E07B
							break;
						case 'r':
						case 'R':
							out = out.substr(0, i) + (font->charExists(u'') ? "" : "→") + out.substr(i + 3); // U+E07C
							break;
						case 'v':
						case 'V':
							out = out.substr(0, i) + (font->charExists(u'') ? "" : "↑↓") + out.substr(i + 3); // U+E07D
							break;
						case 'h':
						case 'H':
							out = out.substr(0, i) + (font->charExists(u'') ? "" : "←→") + out.substr(i + 3); // U+E07E
							break;
					}
				default:
					break;
			}
		} else if(out[i] == '&') {
			if(out.substr(i + 1, 3) == "lrm") {
				out = out.substr(0, i) + "\u200E" + out.substr(i + 4); // Left-to-Right mark
			} else if(out.substr(i + 1, 3) == "rlm") {
				out = out.substr(0, i) + "\u200F" + out.substr(i + 4); // Right-to-Left mark
			}
		}
	}

	return out;
}

/**
 * Initialize translations.
 * Uses the language ID specified in settings.ui.language.
 */
void langInit(bool reloading) {
	// If reloading then don't change if ini not found
	if(reloading && access(config->languageIniPath().c_str(), F_OK) != 0)
		return;

	CIniFile languageini(config->languageIniPath());

#define STRING(what, def) STR_##what = getString(languageini, ""#what, def);
#include "language.inl"
#undef STRING

	rtl = languageini.GetString("PROPERTIES", "DIR", "ltr") == "rtl";
	if(rtl) {
		firstCol = -1;
		lastCol = 0;
		alignStart = Alignment::right;
		alignEnd = Alignment::left;
	} else {
		firstCol = 0;
		lastCol = -1;
		alignStart = Alignment::left;
		alignEnd = Alignment::right;
	}
}

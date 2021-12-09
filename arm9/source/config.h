#ifndef CONFIG_H
#define CONFIG_H

#include "inifile.h"

class Config {
	const char *_configPath;

	std::string _languageIniPath;
	std::string _fontPath;

	static const char *getSystemLanguage(void);

public:
	Config(void);
	~Config(void) {};

	void save(void);

	const std::string &languageIniPath(void) { return _languageIniPath; }
	void languageIniPath(const std::string &languageIniPath) { _languageIniPath = languageIniPath; }

	const std::string &fontPath(void) { return _fontPath; }
};

extern Config *config;

#endif // CONFIG_H
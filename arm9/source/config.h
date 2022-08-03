#ifndef CONFIG_H
#define CONFIG_H

#include "inifile.h"

#include <nds.h>

class Config {
	const char *_configPath;

	std::string _languageIniPath;
	std::string _fontPath;
	bool _screenSwap;

	static const char *getSystemLanguage(void);

public:
	Config(void);
	~Config(void) {};

	void save(void);

	const std::string &languageIniPath(void) { return _languageIniPath; }
	void languageIniPath(const std::string &languageIniPath) { _languageIniPath = languageIniPath; }

	const std::string &fontPath(void) { return _fontPath; }

	bool screenSwap(void) { return _screenSwap; }
	void screenSwap(bool &screenSwap) { _screenSwap = screenSwap; }
	u32 screenSwapKey(void) { return _screenSwap ? KEY_TOUCH : 0; }
};

extern Config *config;

#endif // CONFIG_H
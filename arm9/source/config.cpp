#include "config.h"

#include "driveOperations.h"

#include <nds.h>

Config *config;

const char *Config::getSystemLanguage() {
	bool useTwlCfg = (((isDSiMode() || REG_SCFG_EXT != 0)) && (*(u8*)0x02000400 != 0) && (*(u8*)0x02000401 == 0) && (*(u8*)0x02000402 == 0) && (*(u8*)0x02000404 == 0) && (*(u8*)0x02000448 != 0));
	switch(useTwlCfg ? *(u8*)0x02000406 : PersonalData->language) {
		case 0:
			return "ja-JP";
		case 1:
		default:
			return "en-US";
		case 2:
			return "fr-FR";
		case 3:
			return "de-DE";
		case 4:
			return "it-IT";
		case 5:
			return "es-ES";
		case 6:
			return "zh-CN";
		case 7:
			return "ko-KR";
	}
}

Config::Config() {
	_configPath = sdMounted ? "sd:/gm9i/config.ini" : "fat:/gm9i/config.ini";

	// Load from config file
	CIniFile ini(_configPath);

	std::string langPath = ini.GetString("GODMODE9I", "LANGUAGE_INI_PATH", "DEFAULT_NOT_FOUND");
	if (langPath == "DEFAULT_NOT_FOUND") {
		bool isWritable = sdMounted ? driveWritable(Drive::sdCard) : (flashcardMounted && driveWritable(Drive::flashcard));
		_languagePromptNeeded = isWritable;
		char defaultLanguagePath[40];
		sniprintf(defaultLanguagePath, sizeof(defaultLanguagePath), "nitro:/languages/%s/language.ini", getSystemLanguage());
		_languageIniPath = defaultLanguagePath;
		// Do not write LANGUAGE_INI_PATH to disk yet — languageMenu() will do it after the user confirms
	} else {
		_languageIniPath = langPath;
		_languagePromptNeeded = false;
	}

	_fontPath = ini.GetString("GODMODE9I", "FONT_PATH", "sd:/gm9i/font.frf");
	_screenSwap = ini.GetInt("GODMODE9I", "SCREEN_SWAP", 0);

	// Create the config file if it doesn't exist (but only when language is already known)
	if (access(_configPath, F_OK) != 0 && !_languagePromptNeeded)
		save();
}

void Config::save() {
	CIniFile ini(_configPath);

	ini.SetString("GODMODE9I", "LANGUAGE_INI_PATH", _languageIniPath);
	ini.SetString("GODMODE9I", "FONT_PATH", _fontPath);
	ini.SetInt("GODMODE9I", "SCREEN_SWAP", _screenSwap);

	ini.SaveIniFile(_configPath);
}

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

	char defaultLanguagePath[36];
	sniprintf(defaultLanguagePath, sizeof(defaultLanguagePath), "nitro:/languages/%s/language.ini", getSystemLanguage());
	_languageIniPath = ini.GetString("GODMODE9I", "LANGUAGE_INI_PATH", defaultLanguagePath);
	_fontPath = ini.GetString("GODMODE9I", "FONT_PATH", "sd:/gm9i/font.frf");

	// If the config doesn't exist, create it
	if(access(_configPath, F_OK) != 0)
		save();
}

void Config::save() {
	CIniFile ini(_configPath);

	ini.SetString("GODMODE9I", "LANGUAGE_INI_PATH", _languageIniPath);
	ini.SetString("GODMODE9I", "FONT_PATH", _fontPath);

	ini.SaveIniFile(_configPath);
}

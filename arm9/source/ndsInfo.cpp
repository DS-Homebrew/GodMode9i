#include "ndsInfo.h"

#include "font.h"
#include "language.h"
#include "screenshot.h"
#include "tonccpy.h"

#include <nds.h>
#include <stdio.h>

constexpr std::string *langNames[8] {
	&STR_JAPANESE,
	&STR_ENGLISH,
	&STR_FRENCH,
	&STR_GERMAN,
	&STR_ITALIAN,
	&STR_SPANISH,
	&STR_CHINESE,
	&STR_KOREAN
};

void ndsInfo(const char *path) {
	FILE *file = fopen(path, "rb");
	if(!file)
		return;

	char headerTitle[0xD] = {0};
	fread(headerTitle, 1, 0xC, file);

	char tid[5] = {0};
	fread(tid, 1, 4, file);

	u32 ofs;
	fseek(file, 0x68, SEEK_SET);
	fread(&ofs, sizeof(u32), 1, file);
	if(ofs < 0x8000 || fseek(file, ofs, SEEK_SET) != 0) {
		fclose(file);
		return;
	}

	u16 version;
	fread(&version, sizeof(u16), 1, file);

	u8 *iconBitmap = new u8[8 * 0x200];
	u16 *iconPalette = new u16[8 * 0x10];
	u16 *iconAnimation = new u16[0x40](); // Initialize to 0 for DS icons

	if(version == 0x0103) { // DSi
		fseek(file, 0x1240 - 2, SEEK_CUR);
		fread(iconBitmap, 1, 8 * 0x200, file);
		fread(iconPalette, 2, 8 * 0x10, file);
		fread(iconAnimation, 2, 0x40, file);

		fseek(file, ofs + 0x240, SEEK_SET);
	} else if((version & ~3) == 0) { // DS
		fseek(file, 0x20 - 2, SEEK_CUR);
		fread(iconBitmap, 1, 0x200, file);
		fread(iconPalette, 2, 0x10, file);
	} else {
		fclose(file);
		return;
	}

	int languages = 5 + (version & 0x3);
	char16_t *titles = new char16_t[languages * 0x80];
	fread(titles, 2, languages * 0x80, file);

	fclose(file);

	oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);

	u16 *iconGfx = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_16Color);
	oamSet(&oamSub, 0, 256 - 36, 4, 0, 0, SpriteSize_32x32, SpriteColorFormat_16Color, iconGfx, -1, false, false, false, false, false);
	
	tonccpy(iconGfx, iconBitmap, 0x200);
	tonccpy(SPRITE_PALETTE_SUB, iconPalette, 0x20);

	oamUpdate(&oamSub);

	u16 pressed = 0, held = 0;
	int animationFrame = 0, frameDelay = 0, lang = 1;
	while(1) {
		font->clear(false);
		font->printf(0, 0, false, Alignment::left, Palette::white, STR_HEADER_TITLE.c_str(), headerTitle);
		font->printf(0, 1, false, Alignment::left, Palette::white, STR_TITLE_ID.c_str(), tid);
		font->printf(0, 2, false, Alignment::left, Palette::white, STR_TITLE_IN_LANGUAGE.c_str(), langNames[lang]->c_str());
		font->print(2, 3, false, titles + lang * 0x80);
		font->update(false);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();

			if(iconAnimation[animationFrame] && animationFrame < 0x40) {
				if(frameDelay < (iconAnimation[animationFrame] & 0xFF) - 1) {
					frameDelay++;
				} else {
					frameDelay = 0;
					if(!iconAnimation[++animationFrame])
						animationFrame = 0;

					tonccpy(iconGfx, iconBitmap + ((iconAnimation[animationFrame] >> 8) & 7) * 0x200, 0x200);
					tonccpy(SPRITE_PALETTE_SUB, iconPalette + ((iconAnimation[animationFrame] >> 0xB) & 7) * 0x10, 0x20);
					oamSetFlip(&oamSub, 0, iconAnimation[animationFrame] & BIT(14), iconAnimation[animationFrame] & BIT(15));
					oamUpdate(&oamSub);
				}
			}
		} while(!held);

		if(held & KEY_UP) {
			if(lang > 0)
				lang--;
		} else if(held & KEY_DOWN) {
			if(lang < languages - 1)
				lang++;
		} else if(pressed & KEY_B) {
			break;
		} else if(keysHeld() & KEY_R && pressed & KEY_L) {
			screenshot();
		}
	}

	delete[] iconBitmap;
	delete[] iconPalette;
	delete[] iconAnimation;
	delete[] titles;

	oamFreeGfx(&oamSub, iconGfx);
	oamDisable(&oamSub);
}

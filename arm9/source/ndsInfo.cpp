#include "ndsInfo.h"

#include "date.h"
#include "tonccpy.h"

#include <nds.h>
#include <stdio.h>

extern PrintConsole bottomConsole, bottomConsoleBG, topConsole;

constexpr const char *langNames[8] {
	"Japanese",
	"English",
	"French",
	"German",
	"Italian",
	"Spanish",
	"Chinese",
	"Korean"
};

extern void reinitConsoles(void);

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
	fseek(file, ofs, SEEK_SET);

	u16 version;
	fread(&version, sizeof(u16), 1, file);

	u8 iconBitmap[8][0x200] = {{0}};
	u16 iconPalette[8][0x10] = {{0}};
	u16 iconAnimation[0x40] = {0};

	if(version == 0x0103) { // DSi
		fseek(file, 0x1240 - 2, SEEK_CUR);
		fread(iconBitmap, 1, 8 * 0x200, file);
		fread(iconPalette, 2, 8 * 0x10, file);
		fread(iconAnimation, 2, 0x40, file);

		fseek(file, ofs + 0x240, SEEK_SET);
	} else { // DS
		fseek(file, 0x20 - 2, SEEK_CUR);
		fread(iconBitmap[0], 1, 0x200, file);
		fread(iconPalette[0], 2, 0x10, file);
	}

	int languages = 5 + (version & 0x3);
	char16_t titles[languages][0x80] = {0};
	fread(titles, 2, languages * 0x80, file);

	fclose(file);

	oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);

	u16 *iconGfx = oamAllocateGfx(&oamSub, SpriteSize_32x32, SpriteColorFormat_16Color);
	oamSet(&oamSub, 0, 256 - 36, 4, 0, 0, SpriteSize_32x32, SpriteColorFormat_16Color, iconGfx, -1, false, false, false, false, false);
	
	tonccpy(iconGfx, iconBitmap[0], 0x200);
	tonccpy(SPRITE_PALETTE_SUB, iconPalette[0], 0x20);

	oamUpdate(&oamSub);

	u16 pressed = 0, held = 0;
	int animationFrame = 0, frameDelay = 0, lang = 1;
	while(1) {
		consoleClear();

		iprintf("Header Title: %s\n", headerTitle);
		iprintf("Title ID: %s\n", tid);
		iprintf("Title: (%s)\n  ", langNames[lang]);
		for(int j = 0; j < 0x80 && titles[lang][j]; j++) {
			if(titles[lang][j] == '\n')
				iprintf("\n  ");
			else
				iprintf("%c", titles[lang][j]);
		}
		iprintf("\n");

		consoleSelect(&topConsole);
		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
			iprintf("\x1B[30m\x1B[0;26H %s", RetTime().c_str()); // Print time
			if(iconAnimation[animationFrame] && animationFrame < 0x40) {
				if(frameDelay < (iconAnimation[animationFrame] & 0xFF) - 1) {
					frameDelay++;
				} else {
					frameDelay = 0;
					if(!iconAnimation[++animationFrame])
						animationFrame = 0;

					tonccpy(iconGfx, iconBitmap[(iconAnimation[animationFrame] >> 8) & 7], 0x200);
					tonccpy(SPRITE_PALETTE_SUB, iconPalette[(iconAnimation[animationFrame] >> 0xB) & 7], 0x20);
					oamSetFlip(&oamSub, 0, iconAnimation[animationFrame] & BIT(14), iconAnimation[animationFrame] & BIT(15));
					oamUpdate(&oamSub);
				}
			}
		} while(!held);
		consoleSelect(&bottomConsole);

		if(held & KEY_UP) {
			if(lang > 0)
				lang--;
		} else if(held & KEY_DOWN) {
			if(lang < languages - 1)
				lang++;
		} else if(pressed & KEY_B) {
			break;
		}
	}

	oamFreeGfx(&oamSub, iconGfx);
	oamDisable(&oamSub);
}

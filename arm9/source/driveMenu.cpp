/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/

#include <nds.h>
#include <nds/arm9/dldi.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include "main.h"
#include "date.h"
#include "screenshot.h"
#include "dumpOperations.h"
#include "driveOperations.h"
#include "fileOperations.h"
#include "font.h"
#include "language.h"
#include "my_sd.h"
#include "read_card.h"
#include "startMenu.h"

#define ENTRIES_START_ROW 1
#define ENTRY_PAGE_LENGTH 10

enum class DriveMenuOperation {
	none,
	sdCard,
	flashcard,
	ramDrive,
	sysNand,
	nitroFs,
	fatImage,
	gbaCart,
	ndsCard,
};

//static bool ramDumped = false;

bool flashcardMountSkipped = true;
static bool flashcardMountRan = true;
static int dmCursorPosition = 0;
static std::vector<DriveMenuOperation> dmOperations;
char romTitle[2][13] = {0};
u32 romSize[2], romSizeTrimmed;

static u8 gbaFixedValue = 0;
static u8 stored_SCFG_MC = 0;

extern bool arm7SCFGLocked;

void dm_drawTopScreen(void) {
	font->clear(true);

	// Top bar
	font->printf(0, 0, true, Alignment::left, Palette::blackGreen, "%*c", 256 / font->width(), ' ');
	font->print(0, 0, true, STR_ROOT, Alignment::left, Palette::blackGreen);

	// Print time
	font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);

	if (dmOperations.size() == 0) {
		font->print(0, 1, true, STR_NO_DRIVES_FOUND);
	} else
	for (int i = 0; i < (int)dmOperations.size(); i++) {
		Palette pal = dmCursorPosition == i ? Palette::white : Palette::gray;
		switch(dmOperations[i]) {
			case DriveMenuOperation::sdCard:
				font->printf(0, i + 1, true, Alignment::left, pal, STR_SDCARD_LABEL.c_str(), sdLabel[0] == 0 ? STR_UNTITLED.c_str() : sdLabel);
				if(!driveWritable(Drive::sdCard))
					font->print(-1, i + 1, true, "[R]", Alignment::right, pal);
				break;
			case DriveMenuOperation::flashcard:
				font->printf(0, i + 1, true, Alignment::left, pal, STR_FLASHCARD_LABEL.c_str(), fatLabel[0] == 0 ? STR_UNTITLED.c_str() : fatLabel);
				if(!driveWritable(Drive::flashcard))
					font->print(-1, i + 1, true, "[R]", Alignment::right, pal);
				break;
			case DriveMenuOperation::ramDrive:
				font->print(0, i + 1, true, STR_RAMDRIVE_LABEL, Alignment::left, pal);
				break;
			case DriveMenuOperation::sysNand:
				font->print(0, i + 1, true, STR_SYSNAND_LABEL, Alignment::left, pal);
				if(!driveWritable(Drive::nand))
					font->print(-1, i + 1, true, "[R]", Alignment::right, pal);
				break;
			case DriveMenuOperation::nitroFs:
				font->print(0, i + 1, true, STR_NITROFS_LABEL, Alignment::left, pal);
				font->print(-1, i + 1, true, "[R]", Alignment::right, pal);
				break;
			case DriveMenuOperation::fatImage:
				font->printf(0, i + 1, true, Alignment::left, pal, STR_FAT_LABEL.c_str(), imgLabel[0] == 0 ? STR_UNTITLED.c_str() : imgLabel);
				font->print(-1, i + 1, true, "[R]", Alignment::right, pal);
				break;
			case DriveMenuOperation::gbaCart:
				font->printf(0, i + 1, true, Alignment::left, pal, STR_GBA_GAMECART.c_str(), romTitle[1]);
				break;
			case DriveMenuOperation::ndsCard:
				if(romTitle[0][0] != 0)
					font->printf(0, i + 1, true, Alignment::left, pal, STR_NDS_GAMECARD.c_str(), romTitle[0]);
				else
					font->print(0, i + 1, true, STR_NDS_GAMECARD_NO_TITLE, Alignment::left, pal);
				break;
			case DriveMenuOperation::none:
				break;
		}
	}

	font->update(true);
}

void dm_drawBottomScreen(void) {
	font->clear(false);

	int row = -1;

	if (!isDSiMode() && isRegularDS) {
		font->print(0, row--, false, STR_POWERTEXT_DS);
	} else if (is3DS) {
		font->print(0, row--, false, STR_HOMETEXT);
		font->print(0, row--, false, STR_POWERTEXT_3DS);
	} else {
		font->print(0, row--, false, STR_POWERTEXT);
	}

	font->print(0, row--, false, STR_START_START_MENU);

	if ((isDSiMode() && memcmp(io_dldi_data->friendlyName, "Default", 7) == 0) || sdMountedDone) {
		font->print(0, row--, false, sdMounted ? STR_UNMOUNT_SDCARD : STR_REMOUNT_SDCARD);
	} else if(flashcardMounted) {
		font->print(0, row--, false, STR_UNMOUNT_FLASHCARD);
	}
	if ((sdMounted && driveWritable(Drive::sdCard)) || (flashcardMounted && driveWritable(Drive::flashcard))) {
		font->print(0, row--, false, STR_SCREENSHOTTEXT);
	}

	if(dmOperations[dmCursorPosition] == DriveMenuOperation::nitroFs || dmOperations[dmCursorPosition] == DriveMenuOperation::fatImage)
		font->print(0, row--, false, STR_IMAGETEXT);
	font->print(0, row--, false, titleName);

	switch(dmOperations[dmCursorPosition]) {
		case DriveMenuOperation::sdCard:
			font->printf(0, 0, false, Alignment::left, Palette::white, STR_SDCARD_LABEL.c_str(), sdLabel[0] == 0 ? STR_UNTITLED.c_str() : sdLabel);
			font->printf(0, 1, false, Alignment::left, Palette::white, STR_SD_FAT.c_str(), getBytes(sdSize).c_str());
			font->printf(0, 2, false, Alignment::left, Palette::white, STR_N_FREE.c_str(), getBytes(getBytesFree("sd:/")).c_str());
			break;
		case DriveMenuOperation::flashcard:
			font->printf(0, 0, false, Alignment::left, Palette::white, STR_FLASHCARD_LABEL.c_str(), fatLabel[0] == 0 ? STR_UNTITLED.c_str() : fatLabel);
			font->printf(0, 1, false, Alignment::left, Palette::white, STR_SLOT1_FAT.c_str(), getBytes(fatSize).c_str());
			font->printf(0, 2, false, Alignment::left, Palette::white, STR_N_FREE.c_str(), getBytes(getBytesFree("fat:/")).c_str());
			break;
		case DriveMenuOperation::gbaCart:
			font->printf(0, 0, false, Alignment::left, Palette::white, STR_GBA_GAMECART.c_str(), romTitle[1]);
			font->printf(0, 1, false, Alignment::left, Palette::white, STR_GBA_GAME.c_str(), getBytes(romSize[1]).c_str());
			break;
		case DriveMenuOperation::nitroFs:
			font->print(0, 0, false, STR_NITROFS_LABEL);
			font->print(0, 1, false, STR_GAME_VIRTUAL);
			break;
		case DriveMenuOperation::ndsCard:
			if(romTitle[0][0] != 0) {
				font->printf(0, 0, false, Alignment::left, Palette::white, STR_NDS_GAMECARD.c_str(), romTitle[0]);
				font->printf(0, 1, false, Alignment::left, Palette::white, STR_NDS_GAME.c_str(), getBytes(romSize[0]).c_str(), getBytes(romSizeTrimmed).c_str());
			} else {
				font->print(0, 0, false, STR_NDS_GAMECARD_NO_TITLE);
			}
			break;
		case DriveMenuOperation::ramDrive:
			font->print(0, 0, false, STR_RAMDRIVE_LABEL);
			font->printf(0, 1, false, Alignment::left, Palette::white, STR_RAMDRIVE_FAT.c_str(), getBytes(ramdSize).c_str());
			font->printf(0, 2, false, Alignment::left, Palette::white, STR_N_FREE.c_str(), getBytes(getBytesFree("ram:/")).c_str());
			break;
		case DriveMenuOperation::sysNand:
			font->print(0, 0, false, STR_SYSNAND_LABEL);
			font->printf(0, 1, false, Alignment::left, Palette::white, STR_SYSNAND_FAT.c_str(), getBytes(nandSize).c_str());
			font->printf(0, 2, false, Alignment::left, Palette::white, STR_N_FREE.c_str(), getBytes(getBytesFree("nand:/")).c_str());
			break;
		case DriveMenuOperation::fatImage:
			font->printf(0, 0, false, Alignment::left, Palette::white, STR_FAT_LABEL.c_str(), imgLabel[0] == 0 ? STR_UNTITLED.c_str() : imgLabel);
			font->printf(0, 1, false, Alignment::left, Palette::white, STR_FAT_IMAGE.c_str(), getBytes(imgSize).c_str());
			break;
		case DriveMenuOperation::none:
			break;
	}

	font->update(false);
}

void driveMenu (void) {
	int pressed = 0;
	int held = 0;

	while (true) {
		if (!isDSiMode() && isRegularDS) {
			gbaFixedValue = *(u8*)(0x080000B2);
		}

		dmOperations.clear();
		if (sdMounted && !sdRemoved)
			dmOperations.push_back(DriveMenuOperation::sdCard);
		if (nandMounted)
			dmOperations.push_back(DriveMenuOperation::sysNand);
		if (flashcardMounted && !driveRemoved(Drive::flashcard))
			dmOperations.push_back(DriveMenuOperation::flashcard);
		if (ramdriveMounted)
			dmOperations.push_back(DriveMenuOperation::ramDrive);
		if (imgMounted)
			dmOperations.push_back(DriveMenuOperation::fatImage);
		if (nitroMounted)
			dmOperations.push_back(DriveMenuOperation::nitroFs);
		if (!isDSiMode() && isRegularDS && gbaFixedValue == 0x96) {
			dmOperations.push_back(DriveMenuOperation::gbaCart);
			*(u16*)(0x020000C0) = 0;
			if(romTitle[1][0] == 0) {
				tonccpy(romTitle[1], (char*)0x080000A0, 12);
				romSize[1] = 0;
				for (romSize[1] = (1 << 20); romSize[1] < (1 << 25); romSize[1] <<= 1) {
					vu16 *rompos = (vu16*)(0x08000000 + romSize[1]);
					bool romend = true;
					for (int j = 0; j < 0x1000; j++) {
						if (rompos[j] != j) {
							romend = false;
							break;
						}
					}
					if (romend)
						break;
				}
			}
		} else if (romTitle[1][0] != 0) {
			romTitle[1][0] = 0;
			romSize[1] = 0;
		}
		if (((io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA) || (isRegularDS && !flashcardMounted && romTitle[1][0] != 0))
		|| (isDSiMode() && !arm7SCFGLocked && !(REG_SCFG_MC & BIT(0)))) {
			dmOperations.push_back(DriveMenuOperation::ndsCard);
			if(romTitle[0][0] == 0 && ((io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA) || !flashcardMounted) && !isRegularDS) {
				sNDSHeaderExt ndsHeader;
				cardInit(&ndsHeader);
				tonccpy(romTitle[0], ndsHeader.gameTitle, 12);
				romSize[0] = 0x20000 << ndsHeader.deviceSize;
				romSizeTrimmed = (isDSiMode() && (ndsHeader.unitCode != 0) && (ndsHeader.twlRomSize > 0))
										? ndsHeader.twlRomSize : ndsHeader.romSize + 0x88;
			}
		} else if (romTitle[0][0] != 0) {
			romTitle[0][0] = 0;
			romSizeTrimmed = romSize[0] = 0;
		}

		if(dmCursorPosition >= (int)dmOperations.size())
			dmCursorPosition = dmOperations.size() - 1;

		dm_drawBottomScreen();
		dm_drawTopScreen();

		stored_SCFG_MC = REG_SCFG_MC;

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			pressed = keysDownRepeat();
			held = keysHeld();
			swiWaitForVBlank();

			if (!isDSiMode() && isRegularDS) {
				if (*(u8*)(0x080000B2) != gbaFixedValue) {
					break;
				}
				if(ramdriveMounted && driveRemoved(Drive::ramDrive)) {
					currentDrive = Drive::ramDrive;
					chdir("ram:/");
					ramdriveUnmount();
					break;
				}
			} else if (isDSiMode()) {
				if ((REG_SCFG_MC != stored_SCFG_MC) || (flashcardMounted && driveRemoved(Drive::flashcard))) {
					break;
				}
				if (sdMounted && sdRemoved) {
					currentDrive = Drive::sdCard;
					chdir("sd:/");
					sdUnmount();
					break;
				}
			}
		} while (!(pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_X | KEY_L | KEY_START
#ifdef SCREENSWAP
				| KEY_TOUCH
#endif
		)));

		if(dmOperations.size() != 0) {
			if (pressed & KEY_UP) {
				dmCursorPosition -= 1;
				if(dmCursorPosition < 0)
					dmCursorPosition = dmOperations.size() - 1;
			} else if (pressed & KEY_DOWN) {
				dmCursorPosition += 1;
				if(dmCursorPosition >= (int)dmOperations.size())
					dmCursorPosition = 0;
			} else if(pressed & KEY_LEFT) {
				dmCursorPosition -= ENTRY_PAGE_LENGTH;
				if(dmCursorPosition < 0)
					dmCursorPosition = 0;
			} else if(pressed & KEY_RIGHT) {
				dmCursorPosition += ENTRY_PAGE_LENGTH;
				if(dmCursorPosition >= (int)dmOperations.size())
					dmCursorPosition = dmOperations.size() - 1;
			}
		}

		if (pressed & KEY_A) {
			if (dmOperations[dmCursorPosition] == DriveMenuOperation::sdCard && sdMounted) {
				currentDrive = Drive::sdCard;
				chdir("sd:/");
				screenMode = 1;
				break;
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::flashcard && flashcardMounted) {
				currentDrive = Drive::flashcard;
				chdir("fat:/");
				screenMode = 1;
				break;
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::gbaCart && isRegularDS && flashcardMounted && gbaFixedValue == 0x96) {
				gbaCartDump();
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::nitroFs && nitroMounted) {
				if ((sdMounted && nitroCurrentDrive == Drive::sdCard)
				|| (flashcardMounted && nitroCurrentDrive == Drive::flashcard)
				|| (ramdriveMounted && nitroCurrentDrive == Drive::ramDrive)
				|| (nandMounted && nitroCurrentDrive == Drive::nand)
				|| (imgMounted && nitroCurrentDrive == Drive::fatImg))
				{
					currentDrive = Drive::nitroFS;
					chdir("nitro:/");
					screenMode = 1;
					break;
				}
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::ndsCard && (sdMounted || flashcardMounted || romTitle[1][0] != 0)) {
				ndsCardDump();
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::ramDrive && ramdriveMounted) {
				currentDrive = Drive::ramDrive;
				chdir("ram:/");
				screenMode = 1;
				break;
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::sysNand && isDSiMode() && nandMounted) {
				currentDrive = Drive::nand;
				chdir("nand:/");
				screenMode = 1;
				break;
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::fatImage && imgMounted) {
				if ((sdMounted && imgCurrentDrive == Drive::sdCard)
				|| (flashcardMounted && imgCurrentDrive == Drive::flashcard)
				|| (ramdriveMounted && imgCurrentDrive == Drive::ramDrive)
				|| (nandMounted && imgCurrentDrive == Drive::nand))
				{
					currentDrive = Drive::fatImg;
					chdir("img:/");
					screenMode = 1;
					break;
				}
			}
		}

		// Unmount/Remount FAT image
		if ((held & KEY_R) && (pressed & KEY_X)) {
			if (dmOperations[dmCursorPosition] == DriveMenuOperation::nitroFs) {
				currentDrive = Drive::nitroFS;
				chdir("nitro:/");
				nitroUnmount();
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::fatImage) {
				currentDrive = Drive::fatImg;
				chdir("img:/");
				imgUnmount();
			}
		}

		// Unmount/Remount SD card
		if ((held & KEY_R) && (pressed & KEY_B)) {
			if ((isDSiMode() && memcmp(io_dldi_data->friendlyName, "Default", 7) == 0) || sdMountedDone) {
				if (sdMounted) {
					currentDrive = Drive::sdCard;
					chdir("sd:/");
					sdUnmount();
				} else if(!sdRemoved) {
					sdMounted = sdMount();
				}
			} else {
				if (flashcardMounted) {
					currentDrive = Drive::flashcard;
					chdir("fat:/");
					flashcardUnmount();
				}
			}
		}

		if (pressed & KEY_START) {
			startMenu();
		}

#ifdef SCREENSWAP
		// Swap screens
		if (pressed & KEY_TOUCH) {
			screenSwapped = !screenSwapped;
			screenSwapped ? lcdMainOnBottom() : lcdMainOnTop();
		}
#endif

		// Make a screenshot
		if ((held & KEY_R) && (pressed & KEY_L)) {
			screenshot();
		}

		if (isDSiMode() && !flashcardMountSkipped) {
			if (driveRemoved(Drive::flashcard)) {
				if (flashcardMounted) {
					flashcardUnmount();
					flashcardMountRan = false;
				}
			} else if (!flashcardMountRan) {
				flashcardMountRan = true;
				flashcardMounted = flashcardMount();	// Try to mount flashcard
			}
		}
	}
}

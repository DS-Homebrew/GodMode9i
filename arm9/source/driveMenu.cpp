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

#define ENTRIES_START_ROW 1
#define ENTRY_PAGE_LENGTH 10

enum class DriveMenuOperation {
	none,
	sdCard,
	flashcard,
	ramDrive1,
	ramDrive2,
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

static u8 gbaFixedValue = 0;

extern bool arm7SCFGLocked;
extern bool expansionPakFound;

void dm_drawTopScreen(void) {
	font->clear(true);

	// Top bar
	font->printf(0, 0, true, Alignment::left, Palette::blackGreen, "%*c", 256 / font->width(), ' ');
	font->print(0, 0, true, "[root]", Alignment::left, Palette::blackGreen);

	// Print time
	font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);

	if (dmOperations.size() == 0) {
		font->print(0, 1, true, "No drives found!", Alignment::left, Palette::blackGreen);
	} else
	for (int i = 0; i < (int)dmOperations.size(); i++) {
		Palette pal = dmCursorPosition == i ? Palette::white : Palette::gray;
		switch(dmOperations[i]) {
			case DriveMenuOperation::sdCard:
				font->printf(0, i + 1, true, Alignment::left, pal, "[sd:] SDCARD (%s)", sdLabel[0] == 0 ? "UNTITLED" : sdLabel);
				break;
			case DriveMenuOperation::flashcard:
				font->printf(0, i + 1, true, Alignment::left, pal, "[fat:] FLASHCARD (%s)", fatLabel[0] == 0 ? "UNTITLED" : fatLabel);
				break;
			case DriveMenuOperation::ramDrive1:
				font->print(0, i + 1, true, "[ram1:] RAMDRIVE", Alignment::left, pal);
				break;
			case DriveMenuOperation::ramDrive2:
				font->print(0, i + 1, true, "[ram2:] RAMDRIVE", Alignment::left, pal);
				break;
			case DriveMenuOperation::sysNand:
				font->print(0, i + 1, true, "[nand:] SYSNAND", Alignment::left, pal);
				break;
			case DriveMenuOperation::nitroFs:
				font->print(0, i + 1, true, "[nitro:] NDS GAME IMAGE", Alignment::left, pal);
				if (!((sdMounted && nitroCurrentDrive==0)
				|| (flashcardMounted && nitroCurrentDrive==1)
				|| (ramdrive1Mounted && nitroCurrentDrive==2)
				|| (ramdrive2Mounted && nitroCurrentDrive==3)
				|| (nandMounted && nitroCurrentDrive==4)
				|| (imgMounted && nitroCurrentDrive==6)))
					font->print(256 - font->width(), i + 1, true, "[x]", Alignment::right, pal);
				break;
			case DriveMenuOperation::fatImage:
				if ((sdMounted && imgCurrentDrive==0)
				|| (flashcardMounted && imgCurrentDrive==1)
				|| (ramdrive1Mounted && imgCurrentDrive==2)
				|| (ramdrive2Mounted && imgCurrentDrive==3)
				|| (nandMounted && imgCurrentDrive==4)) {
					font->printf(0, i + 1, true, Alignment::left, pal, "[nitro:] FAT IMAGE (%s)", imgLabel[0] == 0 ? "UNTITLED" : imgLabel);
				} else {
					font->print(0, i + 1, true, "[nitro:] FAT IMAGE", Alignment::left, pal);
					font->print(256 - font->width(), i + 1, true, "[x]", Alignment::right, pal);
				}
				break;
			case DriveMenuOperation::gbaCart:
				font->print(0, i + 1, true, "GBA GAMECART", Alignment::left, pal);
				if (gbaFixedValue != 0x96)
					font->print(256 - font->width(), i + 1, true, "[x]", Alignment::right, pal);
				break;
			case DriveMenuOperation::ndsCard:
				font->print(0, i + 1, true, "NDS GAMECARD", Alignment::left, pal);
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
		font->print(0, row--, false, POWERTEXT_DS);
	} else if (is3DS) {
		font->print(0, row--, false, HOMETEXT);
		font->print(0, row--, false, POWERTEXT_3DS);
	} else {
		font->print(0, row--, false, POWERTEXT);
	}

	if (sdMountedDone) {
		if (isRegularDS || sdMounted) {
			font->print(0, row--, false, sdMounted ? "R+B - Unmount SD card" : "R+B - Remount SD card");
		}
	} else {
		font->print(0, row--, false, flashcardMounted ? "R+B - Unmount Flashcard" : "R+B - Remount Flashcard");
	}
	if (sdMounted || flashcardMounted) {
		font->print(0, row--, false, SCREENSHOTTEXT);
	}

	font->print(0, row--, false, IMAGETEXT);
	font->print(0, row--, false, titleName);

	switch(dmOperations[dmCursorPosition]) {
		case DriveMenuOperation::sdCard:
			font->printf(0, 0, false, Alignment::left, Palette::white, "[sd:] SDCARD (%s)", sdLabel[0] == 0 ? "UNTITLED" : sdLabel);
			font->printf(0, 1, false, Alignment::left, Palette::white, "(SD FAT, %s)", getDriveBytes(sdSize).c_str());
			font->printf(0, 2, false, Alignment::left, Palette::white, "%s free", getDriveBytes(getBytesFree("sd:/")).c_str());
			break;
		case DriveMenuOperation::flashcard:
			font->printf(0, 0, false, Alignment::left, Palette::white, "[fat:] FLASHCARD (%s)", fatLabel[0] == 0 ? "UNTITLED" : fatLabel);
			font->printf(0, 1, false, Alignment::left, Palette::white, "(Slot-1 SD FAT, %s)", getDriveBytes(fatSize).c_str());
			font->printf(0, 2, false, Alignment::left, Palette::white, "%s free", getDriveBytes(getBytesFree("fat:/")).c_str());
			break;
		case DriveMenuOperation::gbaCart:
			font->print(0, 0, false, "GBA GAMECART");
			font->print(0, 1, false, "(GBA Game)");
			break;
		case DriveMenuOperation::nitroFs:
			font->print(0, 0, false, "[nitro:] NDS GAME IMAGE\n");
			font->print(0, 1, false, "(Game Virtual)");
			break;
		case DriveMenuOperation::ndsCard:
			font->print(0, 0, false, "NDS GAMECARD\n");
			font->print(0, 1, false, "(NDS Game)");
			break;
		case DriveMenuOperation::ramDrive1:
			font->print(0, 0, false, "[ram1:] RAMDRIVE\n");
			font->print(0, 1, false, "(RAMdrive FAT, 9 MB)");
			break;
		case DriveMenuOperation::ramDrive2:
			font->print(0, 0, false, "[ram2:] RAMDRIVE\n");
			font->print(0, 1, false, "(RAMdrive FAT, 16 MB)");
			break;
		case DriveMenuOperation::sysNand:
			font->print(0, 0, false, "[nand:] SYSNAND");
			font->printf(0, 1, false, Alignment::left, Palette::white, "(SysNAND FAT, %s)", getDriveBytes(fatSize).c_str());
			font->printf(0, 2, false, Alignment::left, Palette::white, "%s free", getDriveBytes(getBytesFree("nand:/")).c_str());
			break;
		case DriveMenuOperation::fatImage:
			font->print(0, 0, false, "[img:] FAT IMAGE");
			font->printf(0, 1, false, Alignment::left, Palette::white, "(Image FAT, %s)", getDriveBytes(imgSize).c_str());
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
		if (sdMounted)
			dmOperations.push_back(DriveMenuOperation::sdCard);
		if (nandMounted)
			dmOperations.push_back(DriveMenuOperation::sysNand);
		if (flashcardMounted)
			dmOperations.push_back(DriveMenuOperation::flashcard);
		if (ramdrive1Mounted)
			dmOperations.push_back(DriveMenuOperation::ramDrive1);
		if (ramdrive2Mounted)
			dmOperations.push_back(DriveMenuOperation::ramDrive2);
		if (imgMounted)
			dmOperations.push_back(DriveMenuOperation::fatImage);
		if (expansionPakFound
		|| (io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA)
		|| (isDSiMode() && !arm7SCFGLocked && !(REG_SCFG_MC & BIT(0))))
			dmOperations.push_back(DriveMenuOperation::ndsCard);
		if (!isDSiMode() && isRegularDS)
			dmOperations.push_back(DriveMenuOperation::gbaCart);
		if (nitroMounted)
			dmOperations.push_back(DriveMenuOperation::nitroFs);

		dm_drawBottomScreen();
		dm_drawTopScreen();

		stored_SCFG_MC = REG_SCFG_MC;

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			// Print time
			font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
			font->update(true);
	
			scanKeys();
			pressed = keysDownRepeat();
			held = keysHeld();
			swiWaitForVBlank();

			if (!isDSiMode() && isRegularDS) {
				if (*(u8*)(0x080000B2) != gbaFixedValue) {
					break;
				}
			} else if (isDSiMode()) {
				if (REG_SCFG_MC != stored_SCFG_MC) {
					break;
				}
			}
		} while (!(pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_R
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
				currentDrive = 0;
				chdir("sd:/");
				screenMode = 1;
				break;
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::flashcard && flashcardMounted) {
				currentDrive = 1;
				chdir("fat:/");
				screenMode = 1;
				break;
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::gbaCart && isRegularDS && flashcardMounted && gbaFixedValue == 0x96) {
				gbaCartDump();
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::nitroFs && nitroMounted) {
				if ((sdMounted && nitroCurrentDrive==0)
				|| (flashcardMounted && nitroCurrentDrive==1)
				|| (ramdrive1Mounted && nitroCurrentDrive==2)
				|| (ramdrive2Mounted && nitroCurrentDrive==3)
				|| (nandMounted && nitroCurrentDrive==4)
				|| (imgMounted && nitroCurrentDrive==6))
				{
					currentDrive = 5;
					chdir("nitro:/");
					screenMode = 1;
					break;
				}
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::ndsCard && (sdMounted || flashcardMounted)) {
				ndsCardDump();
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::ramDrive1 && isDSiMode() && ramdrive1Mounted) {
				currentDrive = 2;
				chdir("ram1:/");
				screenMode = 1;
				break;
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::ramDrive2 && isDSiMode() && ramdrive2Mounted) {
				currentDrive = 3;
				chdir("ram2:/");
				screenMode = 1;
				break;
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::sysNand && isDSiMode() && nandMounted) {
				currentDrive = 4;
				chdir("nand:/");
				screenMode = 1;
				break;
			} else if (dmOperations[dmCursorPosition] == DriveMenuOperation::fatImage && imgMounted) {
				if ((sdMounted && imgCurrentDrive==0)
				|| (flashcardMounted && imgCurrentDrive==1)
				|| (ramdrive1Mounted && imgCurrentDrive==2)
				|| (ramdrive2Mounted && imgCurrentDrive==3)
				|| (nandMounted && imgCurrentDrive==4))
				{
					currentDrive = 6;
					chdir("img:/");
					screenMode = 1;
					break;
				}
			}
		}

		// Unmount/Remount FAT image
		if ((held & KEY_R) && (pressed & KEY_X)) {
			if (nitroMounted) {
				currentDrive = 5;
				chdir("nitro:/");
				nitroUnmount();
			} else if (imgMounted) {
				currentDrive = 6;
				chdir("img:/");
				imgUnmount();
			}
		}

		// Unmount/Remount SD card
		if ((held & KEY_R) && (pressed & KEY_B)) {
			if (isDSiMode() && sdMountedDone) {
				if (sdMounted) {
					currentDrive = 0;
					chdir("sd:/");
					sdUnmount();
				} else if (isRegularDS) {
					sdMounted = sdMount();
				}
			} else {
				if (flashcardMounted) {
					currentDrive = 1;
					chdir("fat:/");
					flashcardUnmount();
				} else {
					flashcardMounted = flashcardMount();
				}
			}
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

		if (isDSiMode() && !flashcardMountSkipped && !pressed && !held) {
			if (REG_SCFG_MC == 0x11) {
				if (flashcardMounted) {
					flashcardUnmount();
				}
			} else if (!flashcardMountRan) {
				flashcardMounted = flashcardMount();	// Try to mount flashcard
			}
			flashcardMountRan = false;
		}
	}
}

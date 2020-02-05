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

#define SCREEN_COLS 32
#define ENTRIES_PER_SCREEN 22
#define ENTRIES_START_ROW 1
#define ENTRY_PAGE_LENGTH 10

using namespace std;

//static bool ramDumped = false;

bool flashcardMountSkipped = true;
static bool flashcardMountRan = true;
static bool dmTextPrinted = false;
static int dmCursorPosition = 0;
static int dmAssignedOp[6] = {-1};
static int dmMaxCursors = -1;

static u8 gbaFixedValue = 0;

extern PrintConsole topConsole, bottomConsole;

extern void printBorderTop(void);
extern void printBorderBottom(void);
extern void clearBorderTop(void);
extern void clearBorderBottom(void);

void dm_drawTopScreen(void) {
	/*if (!ramDumped) {
		printf ("Dumping RAM...");
		FILE* destinationFile = fopen("sd:/ramdump.bin", "wb");
		fwrite((void*)0x02000000, 1, 0x400000, destinationFile);
		fclose(destinationFile);
		consoleClear();
		ramDumped = true;
	}*/

	consoleClear();

	printf ("\x1B[30m");		// Print background black color
	// Print time
	printf ("\x1b[0;27H");
	printf (RetTime().c_str());

	printf ("\x1b[0;0H");
	printf ("[root]");
	printf ("\x1B[47m");		// Print foreground white color

	// Move to 2nd row
	printf ("\x1b[1;0H");

	if (dmMaxCursors == -1) {
		printf ("No drives found!");
	} else
	for (int i = 0; i <= dmMaxCursors; i++) {
		iprintf ("\x1b[%d;0H", i + ENTRIES_START_ROW);
		if (dmCursorPosition == i) {
			printf ("\x1B[47m");		// Print foreground white color
		} else {
			printf ("\x1B[40m");		// Print foreground black color
		}
		if (dmAssignedOp[i] == 0) {
			printf ("[sd:] SDCARD");
			if (sdLabel[0] != '\0') {
				iprintf (" (%s)", sdLabel);
			}
		} else if (dmAssignedOp[i] == 1) {
			printf ("[fat:] FLASHCART");
			if (fatLabel[0] != '\0') {
				iprintf (" (%s)", fatLabel);
			}
		} else if (dmAssignedOp[i] == 2) {
			printf ("GBA GAMECART");
			if (gbaFixedValue != 0x96) {
				iprintf ("\x1b[%d;29H", i + ENTRIES_START_ROW);
				printf ("[x]");
			}
		} else if (dmAssignedOp[i] == 3) {
			printf ("[nitro:] NDS GAME IMAGE");
			if ((sdMounted && nitroCurrentDrive==0)
			|| (flashcardMounted && nitroCurrentDrive==1)
			|| (ramdrive1Mounted && nitroCurrentDrive==2)
			|| (ramdrive2Mounted && nitroCurrentDrive==3))
			{
				// Do nothing
			}
			else
			{
				iprintf ("\x1b[%d;29H", i + ENTRIES_START_ROW);
				printf ("[x]");
			}
		} else if (dmAssignedOp[i] == 4) {
			printf ("NDS GAMECARD");
		} else if (dmAssignedOp[i] == 5) {
			printf ("[ram1:] RAMDRIVE");
		} else if (dmAssignedOp[i] == 6) {
			printf ("[ram2:] RAMDRIVE");
		}
	}
}

void dm_drawBottomScreen(void) {
	consoleClear();

	printf ("\x1B[47m");		// Print foreground white color
	printf ("\x1b[23;0H");
	printf (titleName);
	if (isDSiMode() && sdMountedDone) {
		if (isRegularDS || sdMounted) {
			printf ("\n");
			printf (sdMounted ? "R+B - Unmount SD card" : "R+B - Remount SD card");
		}
	} else {
		printf ("\n");
		printf (flashcardMounted ? "R+B - Unmount Flashcard" : "R+B - Remount Flashcard");
	}
	if (sdMounted || flashcardMounted) {
		printf ("\n");
		printf (SCREENSHOTTEXT);
	}
	printf ("\n");
	if (!isDSiMode() && isRegularDS) {
		printf (POWERTEXT_DS);
	} else if (is3DS) {
		printf (POWERTEXT_3DS);
		printf ("\n");
		printf (HOMETEXT);
	} else {
		printf (POWERTEXT);
	}

	printf ("\x1B[40m");		// Print foreground black color
	printf ("\x1b[0;0H");
	if (dmAssignedOp[dmCursorPosition] == 0) {
		printf ("[sd:] SDCARD");
		if (sdLabel[0] != '\0') {
			iprintf (" (%s)", sdLabel);
		}
		printf ("\n(SD FAT, ");
		printDriveBytes(sdSize);
		printf(")");
	} else if (dmAssignedOp[dmCursorPosition] == 1) {
		printf ("[fat:] FLASHCART");
		if (fatLabel[0] != '\0') {
			iprintf (" (%s)", fatLabel);
		}
		printf ("\n(Slot-1 SD FAT, ");
		printDriveBytes(fatSize);
		printf(")");
	} else if (dmAssignedOp[dmCursorPosition] == 2) {
		printf ("GBA GAMECART\n");
		printf ("(GBA Game)");
	} else if (dmAssignedOp[dmCursorPosition] == 3) {
		printf ("[nitro:] NDS GAME IMAGE\n");
		printf ("(Game Virtual)");
	} else if (dmAssignedOp[dmCursorPosition] == 4) {
		printf ("NDS GAMECARD\n");
		printf ("(NDS Game)");
	} else if (dmAssignedOp[dmCursorPosition] == 5) {
		printf ("[ram1:] RAMDRIVE\n");
		printf ("(RAMdrive FAT, 9 MB)");
	} else if (dmAssignedOp[dmCursorPosition] == 6) {
		printf ("[ram2:] RAMDRIVE\n");
		printf ("(RAMdrive FAT, 16 MB)");
	}
}

void driveMenu (void) {
	int pressed = 0;
	int held = 0;

	while (true) {
		if (!isDSiMode() && isRegularDS) {
			gbaFixedValue = *(u8*)(0x080000B2);
		}

		for (int i = 0; i < 6; i++) {
			dmAssignedOp[i] = -1;
		}
		dmMaxCursors = -1;
		if (isDSiMode() && sdMounted){
			dmMaxCursors++;
			dmAssignedOp[dmMaxCursors] = 0;
		}
		if (flashcardMounted) {
			dmMaxCursors++;
			dmAssignedOp[dmMaxCursors] = 1;
		}
		if (ramdrive1Mounted) {
			dmMaxCursors++;
			dmAssignedOp[dmMaxCursors] = 5;
		}
		if (ramdrive2Mounted) {
			dmMaxCursors++;
			dmAssignedOp[dmMaxCursors] = 6;
		}
		if ((io_dldi_data->ioInterface.features & FEATURE_SLOT_GBA) || (isDSiMode() && !(REG_SCFG_MC & BIT(0)))) {
			dmMaxCursors++;
			dmAssignedOp[dmMaxCursors] = 4;
		}
		if (!isDSiMode() && isRegularDS) {
			dmMaxCursors++;
			dmAssignedOp[dmMaxCursors] = 2;
		}
		if (nitroMounted) {
			dmMaxCursors++;
			dmAssignedOp[dmMaxCursors] = 3;
		}

		if (dmCursorPosition < 0) 	dmCursorPosition = dmMaxCursors;		// Wrap around to bottom of list
		if (dmCursorPosition > dmMaxCursors)	dmCursorPosition = 0;		// Wrap around to top of list

		if (!dmTextPrinted) {
			consoleSelect(&bottomConsole);
			dm_drawBottomScreen();
			consoleSelect(&topConsole);
			dm_drawTopScreen();

			dmTextPrinted = true;
		}

		stored_SCFG_MC = REG_SCFG_MC;

		printf ("\x1B[30m");		// Print black color for time text

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			// Move to right side of screen
			printf ("\x1b[0;27H");
			// Print time
			printf (RetTime().c_str());
	
			scanKeys();
			pressed = keysDownRepeat();
			held = keysHeld();
			swiWaitForVBlank();

			if (!isDSiMode() && isRegularDS) {
				if (*(u8*)(0x080000B2) != gbaFixedValue) {
					dmTextPrinted = false;
					break;
				}
			} else if (isDSiMode()) {
				if (REG_SCFG_MC != stored_SCFG_MC) {
					dmTextPrinted = false;
					break;
				}
			}
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN) && !(pressed & KEY_A) && !(held & KEY_R));
	
		printf ("\x1B[47m");		// Print foreground white color

		if ((pressed & KEY_UP) && dmMaxCursors != -1) {
			dmCursorPosition -= 1;
			dmTextPrinted = false;
		}
		if ((pressed & KEY_DOWN) && dmMaxCursors != -1) {
			dmCursorPosition += 1;
			dmTextPrinted = false;
		}

		if (dmCursorPosition < 0) 	dmCursorPosition = dmMaxCursors;		// Wrap around to bottom of list
		if (dmCursorPosition > dmMaxCursors)	dmCursorPosition = 0;		// Wrap around to top of list

		if (pressed & KEY_A) {
			if (dmAssignedOp[dmCursorPosition] == 0 && isDSiMode() && sdMounted) {
				dmTextPrinted = false;
				currentDrive = 0;
				chdir("sd:/");
				screenMode = 1;
				break;
			} else if (dmAssignedOp[dmCursorPosition] == 1 && flashcardMounted) {
				dmTextPrinted = false;
				currentDrive = 1;
				chdir("fat:/");
				screenMode = 1;
				break;
			} else if (dmAssignedOp[dmCursorPosition] == 2 && isRegularDS && flashcardMounted && gbaFixedValue == 0x96) {
				dmTextPrinted = false;
				gbaCartDump();
			} else if (dmAssignedOp[dmCursorPosition] == 3 && nitroMounted) {
				if ((sdMounted && nitroCurrentDrive==0)
				|| (flashcardMounted && nitroCurrentDrive==1)
				|| (ramdrive1Mounted && nitroCurrentDrive==2)
				|| (ramdrive2Mounted && nitroCurrentDrive==3))
				{
					dmTextPrinted = false;
					currentDrive = nitroCurrentDrive;
					chdir("nitro:/");
					screenMode = 1;
					break;
				}
			} else if (dmAssignedOp[dmCursorPosition] == 4 && (sdMounted || flashcardMounted)) {
				dmTextPrinted = false;
				ndsCardDump();
			} else if (dmAssignedOp[dmCursorPosition] == 5 && isDSiMode() && ramdrive1Mounted) {
				dmTextPrinted = false;
				currentDrive = 2;
				chdir("ram1:/");
				screenMode = 1;
				break;
			} else if (dmAssignedOp[dmCursorPosition] == 6 && isDSiMode() && ramdrive2Mounted) {
				dmTextPrinted = false;
				currentDrive = 3;
				chdir("ram2:/");
				screenMode = 1;
				break;
			}
		}

		// Unmount/Remount SD card
		if ((held & KEY_R) && (pressed & KEY_B)) {
			dmTextPrinted = false;
			if (isDSiMode() && sdMountedDone) {
				if (sdMounted) {
					if (currentDrive == 0) {
						sdUnmount();
					} else {
						consoleSelect(&bottomConsole);
						consoleClear();
						printf ("\x1B[47m");		// Print foreground white color
						printf ("Please open and exit SDCARD\n");
						printf ("before unmounting.\n");
						for (int i = 0; i < 60*2; i++) {
							swiWaitForVBlank();
						}
					}
				} else if (isRegularDS) {
					sdMounted = sdMount();
				}
			} else {
				if (flashcardMounted && currentDrive == 1) {
					flashcardUnmount();
				} else if (!flashcardMounted)  {
					flashcardMounted = flashcardMount();
				}
			}
		}

		// Make a screenshot
		if ((held & KEY_R) && (pressed & KEY_L)) {
			if (sdMounted || flashcardMounted) {
				if (access((sdMounted ? "sd:/gm9i" : "fat:/gm9i"), F_OK) != 0) {
					mkdir((sdMounted ? "sd:/gm9i" : "fat:/gm9i"), 0777);
				}
				if (access((sdMounted ? "sd:/gm9i/out" : "fat:/gm9i/out"), F_OK) != 0) {
					mkdir((sdMounted ? "sd:/gm9i/out" : "fat:/gm9i/out"), 0777);
				}
				char timeText[8];
				snprintf(timeText, sizeof(timeText), "%s", RetTime().c_str());
				char fileTimeText[8];
				snprintf(fileTimeText, sizeof(fileTimeText), "%s", RetTimeForFilename().c_str());
				char snapPath[40];
				// Take top screenshot
				snprintf(snapPath, sizeof(snapPath), "%s:/gm9i/out/snap_%s_top.bmp", (sdMounted ? "sd" : "fat"), fileTimeText);
				screenshotbmp(snapPath);
				// Seamlessly swap top and bottom screens
				lcdMainOnBottom();
				printBorderBottom();
				consoleSelect(&bottomConsole);
				dm_drawTopScreen();
				printf("\x1B[30m");		// Print black color for time text
				printf("\x1b[0;27H");
				printf(timeText);
				clearBorderTop();
				consoleSelect(&topConsole);
				dm_drawBottomScreen();
				// Take bottom screenshot
				snprintf(snapPath, sizeof(snapPath), "%s:/gm9i/out/snap_%s_bot.bmp", (sdMounted ? "sd" : "fat"), fileTimeText);
				screenshotbmp(snapPath);
				dmTextPrinted = false;
				lcdMainOnTop();
				printBorderTop();
				clearBorderBottom();
			}
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

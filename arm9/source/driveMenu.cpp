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
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include "main.h"
#include "date.h"
#include "driveOperations.h"

#define SCREEN_COLS 32
#define ENTRIES_PER_SCREEN 22
#define ENTRIES_START_ROW 1
#define ENTRY_PAGE_LENGTH 10

using namespace std;

bool flashcardMountSkipped = true;
static bool flashcardMountRan = true;
static bool dmTextPrinted = false;
static int dmCursorPosition = 0;

static u8 gbaFixedValue = 0;

void gbaCartDump(void) {
	int pressed = 0;

	printf ("\x1b[0;27H");
	printf ("     ");	// Clear time
	consoleInit(NULL, 1, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);
	printf("Dump GBA cart ROM to\n");
	printf("\"fat:/gm9i/out\"?\n");
	printf("(<A> yes, <B> no)");
	while (true) {
		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
		} while (!(pressed & KEY_A) && !(pressed & KEY_B));

		if (pressed & KEY_A) {
			consoleClear();
			if (access("fat:/gm9i", F_OK) != 0) {
				printf("Creating directory...");
				mkdir("fat:/gm9i", 0777);
			}
			if (access("fat:/gm9i/out", F_OK) != 0) {
				printf ("\x1b[0;0H");
				printf("Creating directory...");
				mkdir("fat:/gm9i/out", 0777);
			}
			char gbaHeaderGameTitle[13] = "\0";
			char gbaHeaderGameCode[5] = "\0";
			char gbaHeaderMakerCode[3] = "\0";
			for (int i = 0; i < 12; i++) {
				gbaHeaderGameTitle[i] = *(char*)(0x080000A0+i);
				if (*(u8*)(0x080000A0+i) == 0) {
					break;
				}
			}
			for (int i = 0; i < 4; i++) {
				gbaHeaderGameCode[i] = *(char*)(0x080000AC+i);
				if (*(u8*)(0x080000AC+i) == 0) {
					break;
				}
			}
			for (int i = 0; i < 2; i++) {
				gbaHeaderMakerCode[i] = *(char*)(0x080000B0+i);
			}
			u8 gbaHeaderSoftwareVersion = *(u8*)(0x080000BC);
			char destPath[256];
			snprintf(destPath, sizeof(destPath), "fat:/gm9i/out/%s_%s%s_%x.gba", gbaHeaderGameTitle, gbaHeaderGameCode, gbaHeaderMakerCode, gbaHeaderSoftwareVersion);
			consoleClear();
			printf("Dumping...\n");
			printf("Do not remove the GBA cart.\n");
			// Determine ROM size
			u32 romSize = 0x02000000;
			for (u32 i = 0x09FE0000; i > 0x08000000; i -= 0x20000) {
				if (*(u32*)(i) == 0xFFFE0000) {
					romSize -= 0x20000;
				} else {
					break;
				}
			}
			// Dump!
			remove(destPath);
			FILE* destinationFile = fopen(destPath, "wb");
			fwrite((void*)0x08000000, 1, romSize, destinationFile);
			fclose(destinationFile);
			break;
		}
		if (pressed & KEY_B) {
			break;
		}
	}
}

void driveMenu (void) {
	int pressed = 0;
	int held = 0;
	int assignedOp[3] = {-1};
	int maxCursors = -1;

	while (true) {
		if (!isDSiMode() && isRegularDS) {
			gbaFixedValue = *(u8*)(0x080000B2);
		}

		for (int i = 0; i < 3; i++) {
			assignedOp[i] = -1;
		}
		maxCursors = -1;
		if (isDSiMode() && sdMounted){
			maxCursors++;
			assignedOp[maxCursors] = 0;
		}
		if (flashcardMounted) {
			maxCursors++;
			assignedOp[maxCursors] = 1;
		}
		if (!isDSiMode() && isRegularDS) {
			maxCursors++;
			assignedOp[maxCursors] = 2;
		}
		if (nitroMounted) {
			maxCursors++;
			assignedOp[maxCursors] = 3;
		}

		if (dmCursorPosition < 0) 	dmCursorPosition = maxCursors;		// Wrap around to bottom of list
		if (dmCursorPosition > maxCursors)	dmCursorPosition = 0;		// Wrap around to top of list

		if (!dmTextPrinted) {
			consoleInit(NULL, 1, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);
			printf ("\x1B[40m");		// Print foreground black color
			if (assignedOp[dmCursorPosition] == 0) {
				printf ("[sd:] SDCARD");
				if (sdLabel[0] != '\0') {
					iprintf (" (%s)", sdLabel);
				}
				printf ("\n(SD FAT)");
			} else if (assignedOp[dmCursorPosition] == 1) {
				printf ("[fat:] FLASHCART");
				if (fatLabel[0] != '\0') {
					iprintf (" (%s)", fatLabel);
				}
				printf ("\n(Slot-1 SD FAT)");
			} else if (assignedOp[dmCursorPosition] == 2) {
				printf ("GBA GAMECART\n");
				printf ("(GBA Game)");
			} else if (assignedOp[dmCursorPosition] == 3) {
				printf ("[nitro:] NDS GAME IMAGE\n");
				printf ("(Game Virtual)");
			}
			printf ("\x1B[47m");		// Print foreground white color
			if (isDSiMode() && sdMountedDone) {
				if (sdMounted) {
					printf ("\x1b[21;0H");
					printf (titleName);
					printf ("\x1b[22;0H");
					printf ("R+B - Unmount SD card");
				} else {
					printf ("\x1b[22;0H");
					printf (titleName);
				}
			} else {
				printf ("\x1b[21;0H");
				printf (titleName);
				printf ("\x1b[22;0H");
				printf (flashcardMounted ? "R+B - Unmount Flashcard" : "R+B - Remount Flashcard");
			}
			printf ("\x1b[23;0H");
			printf ((!isDSiMode() && isRegularDS) ? POWERTEXT_DS : POWERTEXT);

			consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);

			printf ("\x1B[42m");		// Print green color
			printf ("[root]");
			printf ("\x1B[47m");		// Print foreground white color

			// Move to 2nd row
			printf ("\x1b[1;0H");

			if (maxCursors == -1) {
				printf ("\x1b[2;0H");
				printf ("No drives found!");
			} else
			for (int i = 0; i <= maxCursors; i++) {
				iprintf ("\x1b[%d;0H", i + ENTRIES_START_ROW);
				if (dmCursorPosition == i) {
					printf ("\x1B[47m");		// Print foreground white color
				} else {
					printf ("\x1B[40m");		// Print foreground black color
				}
				if (assignedOp[i] == 0) {
					printf ("[sd:] SDCARD");
					if (sdLabel[0] != '\0') {
						iprintf (" (%s)", sdLabel);
					}
				} else if (assignedOp[i] == 1) {
					printf ("[fat:] FLASHCART");
					if (fatLabel[0] != '\0') {
						iprintf (" (%s)", fatLabel);
					}
				} else if (assignedOp[i] == 2) {
					printf ("GBA GAMECART");
					if (gbaFixedValue != 0x96) {
						iprintf ("\x1b[%d;29H", i + ENTRIES_START_ROW);
						printf ("[x]");
					}
				} else if (assignedOp[i] == 3) {
					printf ("[nitro:] NDS GAME IMAGE");
					if ((!sdMounted && !nitroSecondaryDrive)
					|| (!flashcardMounted && nitroSecondaryDrive))
					{
						iprintf ("\x1b[%d;29H", i + ENTRIES_START_ROW);
						printf ("[x]");
					}
				}
			}

			dmTextPrinted = true;
		}

		stored_SCFG_MC = REG_SCFG_MC;

		printf ("\x1B[42m");		// Print green color for time text

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

		if ((pressed & KEY_UP) && maxCursors != -1) {
			dmCursorPosition -= 1;
			dmTextPrinted = false;
		}
		if ((pressed & KEY_DOWN) && maxCursors != -1) {
			dmCursorPosition += 1;
			dmTextPrinted = false;
		}

		if (dmCursorPosition < 0) 	dmCursorPosition = maxCursors;		// Wrap around to bottom of list
		if (dmCursorPosition > maxCursors)	dmCursorPosition = 0;		// Wrap around to top of list

		if (pressed & KEY_A) {
			if (assignedOp[dmCursorPosition] == 0 && isDSiMode() && sdMounted) {
				dmTextPrinted = false;
				secondaryDrive = false;
				chdir("sd:/");
				screenMode = 1;
				break;
			} else if (assignedOp[dmCursorPosition] == 1 && flashcardMounted) {
				dmTextPrinted = false;
				secondaryDrive = true;
				chdir("fat:/");
				screenMode = 1;
				break;
			} else if (assignedOp[dmCursorPosition] == 2 && isRegularDS && flashcardMounted && gbaFixedValue == 0x96) {
				dmTextPrinted = false;
				gbaCartDump();
			} else if (assignedOp[dmCursorPosition] == 3 && nitroMounted) {
				if ((sdMounted && !nitroSecondaryDrive)
				|| (flashcardMounted && nitroSecondaryDrive))
				{
					dmTextPrinted = false;
					secondaryDrive = nitroSecondaryDrive;
					chdir("nitro:/");
					screenMode = 1;
					break;
				}
			}
		}

		// Unmount/Remount SD card
		if ((held & KEY_R) && (pressed & KEY_B)) {
			dmTextPrinted = false;
			if (isDSiMode() && sdMountedDone) {
				if (sdMounted) {
					sdUnmount();
				}
			} else {
				if (flashcardMounted) {
					flashcardUnmount();
				} else {
					flashcardMounted = flashcardMount();
				}
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

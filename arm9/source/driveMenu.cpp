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
#define ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10

using namespace std;

bool flashcardMountSkipped = true;
static bool flashcardMountRan = true;
static bool dmTextPrinted = false;
int dmCursorPosition = 0;

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
			char destPath[256];
			snprintf(destPath, sizeof(destPath), "fat:/gm9i/out/%s_%s_%s.gba", gbaHeaderGameTitle, gbaHeaderGameCode, gbaHeaderMakerCode);
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

	while (true) {
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

		gbaFixedValue = *(u8*)(0x080000B2);

		if (!dmTextPrinted) {
			consoleInit(NULL, 1, BgType_Text4bpp, BgSize_T_256x256, 15, 0, false, true);
			if (isDSiMode()) {
				if (dmCursorPosition == 0) {
					printf ("[sd:] SDCARD\n");
					printf ("(SD FAT)");
				} else {
					printf ("[fat:] GAMECART\n");
					printf ("(Flashcart FAT)");
				}
			} else {
				if (dmCursorPosition == 0) {
					printf ("[fat:] GAMECART\n");
					printf ("(Flashcart FAT)");
				} else if (!isDSiMode() && isRegularDS) {
					printf ("GBA GAMECART\n");
					printf ("(GBA Game)");
				}
			}
			iprintf ("\x1b[%i;0H", 21);
			printf (titleName);
			printf ("\x1b[22;0H");
			if (isDSiMode()) {
				printf (sdMounted ? "R+B - Unmount SD card" : "R+B - Remount SD card");
			} else {
				printf (flashcardMounted ? "R+B - Unmount Flashcard" : "R+B - Remount Flashcard");
			}
			printf ("\x1b[23;0H");
			printf ((!isDSiMode() && isRegularDS) ? POWERTEXT_DS : POWERTEXT);

			consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);

			printf ("[root]");

			// Move to 2nd row
			printf ("\x1b[1;0H");
			// Print line of dashes
			printf ("--------------------------------");

			// Show cursor
			printf ("\x1b[%d;0H*", dmCursorPosition + ENTRIES_START_ROW);

			printf ("\x1b[2;1H");
			if (isDSiMode()){
				printf ("[sd:] SDCARD");
				if (!sdMounted) {
					printf ("\x1b[2;29H");
					printf ("[x]");
				}
				printf ("\x1b[3;1H");
			}
			printf ("[fat:] GAMECART");
			if (!flashcardMounted) {
				iprintf ("\x1b[%i;29H", 2+isDSiMode());
				printf ("[x]");
			}
			if (!isDSiMode() && isRegularDS) {
				printf ("\x1b[3;1H");
				printf ("GBA GAMECART");
				if (gbaFixedValue != 0x96) {
					printf ("\x1b[3;29H");
					printf ("[x]");
				}
			}

			dmTextPrinted = true;
		}

		stored_SCFG_MC = REG_SCFG_MC;

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
			
			if ((REG_SCFG_MC != stored_SCFG_MC)
			|| (*(u8*)(0x080000B2) != gbaFixedValue)) {
				dmTextPrinted = false;
				break;
			}
		} while (!(pressed & KEY_UP) && !(pressed & KEY_DOWN) && !(pressed & KEY_A) && !(held & KEY_R));
	
		if (pressed & KEY_UP) {
			if (isDSiMode() || isRegularDS) {
				dmCursorPosition -= 1;
				dmTextPrinted = false;
			}
		}
		if (pressed & KEY_DOWN) {
			if (isDSiMode() || isRegularDS) {
				dmCursorPosition += 1;
				dmTextPrinted = false;
			}
		}
		
		if (dmCursorPosition < 0) 	dmCursorPosition = 1;		// Wrap around to bottom of list
		if (dmCursorPosition > 1)	dmCursorPosition = 0;		// Wrap around to top of list

		if (pressed & KEY_A) {
			if (dmCursorPosition == 0) {
				if (isDSiMode()) {
					if (sdMounted) {
						dmTextPrinted = false;
						secondaryDrive = false;
						chdir("sd:/");
						screenMode = 1;
						break;
					}
				} else {
					if (flashcardMounted) {
						dmTextPrinted = false;
						secondaryDrive = true;
						chdir("fat:/");
						screenMode = 1;
						break;
					}
				}
			} else {
				if (isDSiMode()) {
					if (flashcardMounted) {
						dmTextPrinted = false;
						secondaryDrive = true;
						chdir("fat:/");
						screenMode = 1;
						break;
					}
				} else if (isRegularDS && flashcardMounted && gbaFixedValue == 0x96) {
					dmTextPrinted = false;
					gbaCartDump();
				}
			}
		}

		// Unmount/Remount SD card
		if ((held & KEY_R) && (pressed & KEY_B)) {
			dmTextPrinted = false;
			if (isDSiMode()) {
				if (sdMounted) {
					sdUnmount();
				} else {
					sdMounted = sdMount();
				}
			} else {
				if (flashcardMounted) {
					flashcardUnmount();
				} else {
					flashcardMounted = flashcardMount();
				}
			}
		}
	}
}

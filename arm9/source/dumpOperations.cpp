#include <nds.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fstream>

#include "auxspi.h"
#include "date.h"
#include "driveOperations.h"
#include "ndsheaderbanner.h"
#include "read_card.h"
#include "tonccpy.h"

extern PrintConsole topConsole, bottomConsole;

void ndsCardSaveDump(const char* filename) {
	std::ofstream output(filename, std::ofstream::binary);
	if(output.is_open()) {
		auxspi_extra card_type = auxspi_has_extra();
		consoleClear();
		printf("Dumping save...\n");
		printf("Do not remove the NDS card.\n");
		unsigned char* buffer;
		if(card_type == AUXSPI_INFRARED) {
			int size = auxspi_save_size_log_2(card_type);
			int size_blocks = 1 << std::max(0, (int8(size) - 18));
			int type = auxspi_save_type(card_type);
			if(size < 16)
				size_blocks = 1;
			else
				size_blocks = 1 << (size - 16);
			u32 LEN = std::min(1 << size, 1 << 16);
			buffer = new unsigned char[LEN*size_blocks];
			auxspi_read_data(0, buffer, LEN*size_blocks, type, card_type);
			output.write((char*)buffer, LEN*size_blocks);
		} else {
			int type = cardEepromGetType();
			int size = cardEepromGetSize();
			buffer = new unsigned char[size];
			cardReadEeprom(0, buffer, size, type);
			output.write((char*)buffer, size);
		}
		delete[] buffer;
	}
	output.close();
}

void ndsCardDump(void) {
	int pressed = 0;

	consoleSelect(&bottomConsole);
	consoleClear();
	printf ("\x1B[47m");		// Print foreground white color
	printf("Dump NDS card ROM to\n");
	printf("\"%s:/gm9i/out\"?\n", (sdMounted ? "sd" : "fat"));
	printf("(<A> yes, <Y> trim, <B> no)");

	consoleSelect(&topConsole);
	printf ("\x1B[42m");		// Print green color
	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	do {
		// Move to right side of screen
		printf ("\x1b[0;26H");
		// Print time
		printf ("_%s" ,RetTime().c_str());

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & KEY_A) && !(pressed & KEY_Y) && !(pressed & KEY_B));

	consoleSelect(&bottomConsole);
	printf ("\x1B[47m");		// Print foreground white color

	if ((pressed & KEY_A) || (pressed & KEY_Y)) {
		consoleClear();
		char folderPath[2][256];
		sprintf(folderPath[0], "%s:/gm9i", (sdMounted ? "sd" : "fat"));
		sprintf(folderPath[1], "%s:/gm9i/out", (sdMounted ? "sd" : "fat"));
		if (access(folderPath[0], F_OK) != 0) {
			printf("Creating directory...");
			mkdir(folderPath[0], 0777);
		}
		if (access(folderPath[1], F_OK) != 0) {
			printf ("\x1b[0;0H");
			printf("Creating directory...");
			mkdir(folderPath[1], 0777);
		}
		consoleClear();
		// Read header
		sNDSHeaderExt* ndsCardHeader = (sNDSHeaderExt*)malloc(0x1000);
		if (cardInit (ndsCardHeader) == 0) {
			printf("Dumping...\n");
			printf("Do not remove the NDS card.\n");
		} else {
			printf("Unable to dump the ROM.\n");
			for (int i = 0; i < 60*2; i++) {
				swiWaitForVBlank();
			}
			free(ndsCardHeader);
			return;
		}
		char gameTitle[13] = {0};
		tonccpy(gameTitle, ndsCardHeader->gameTitle, 12);
		char gameCode[7] = {0};
		tonccpy(gameCode, ndsCardHeader->gameCode, 6);
		bool trimRom = (pressed & KEY_Y);
		char romBuffer[0x200];
		char destPath[256];
		sprintf(destPath, "%s:/gm9i/out/%s_%s_%x%s.nds", (sdMounted ? "sd" : "fat"), gameTitle, gameCode, ndsCardHeader->romversion, (trimRom ? "_trim" : ""));
		char destSavPath[256];
		sprintf(destSavPath, "%s:/gm9i/out/%s_%s_%x%s.sav", (sdMounted ? "sd" : "fat"), gameTitle, gameCode, ndsCardHeader->romversion, (trimRom ? "_trim" : ""));
		// Determine ROM size
		u32 romSize = 0;
		if (trimRom) {
			romSize = ((ndsCardHeader->unitCode != 0) && (ndsCardHeader->twlRomSize > 0))
						? ndsCardHeader->twlRomSize : ndsCardHeader->romSize;
		} else switch (ndsCardHeader->deviceSize) {
			case 0x00:
				romSize = 0x20000;
				break;
			case 0x01:
				romSize = 0x40000;
				break;
			case 0x02:
				romSize = 0x80000;
				break;
			case 0x03:
				romSize = 0x100000;
				break;
			case 0x04:
				romSize = 0x200000;
				break;
			case 0x05:
				romSize = 0x400000;
				break;
			case 0x06:
				romSize = 0x800000;
				break;
			case 0x07:
				romSize = 0x1000000;
				break;
			case 0x08:
				romSize = 0x2000000;
				break;
			case 0x09:
				romSize = 0x4000000;
				break;
			case 0x0A:
				romSize = 0x8000000;
				break;
			case 0x0B:
				romSize = 0x10000000;
				break;
			case 0x0C:
				romSize = 0x20000000;
				break;
		}
		// Dump!
		remove(destPath);
		FILE* destinationFile = fopen(destPath, "wb");
		for (u32 src = 0; src < romSize; src += 0x200) {
			consoleSelect(&topConsole);
			printf ("\x1B[42m");		// Print green color
			// Move to right side of screen
			printf ("\x1b[0;26H");
			// Print time
			printf ("_%s" ,RetTime().c_str());

			printf ("\x1b[8;0H");
			printf ("Progress:\n");
			printf ("%i/%i Bytes                       ", (int)src, (int)romSize);
			cardRead (src, romBuffer);
			fwrite(romBuffer, 1, 0x200, destinationFile);
		}
		fclose(destinationFile);
		ndsCardSaveDump(destSavPath);
		free(ndsCardHeader);
	}
}

void gbaCartDump(void) {
	int pressed = 0;

	consoleSelect(&bottomConsole);
	consoleClear();
	printf ("\x1B[47m");		// Print foreground white color
	printf("Dump GBA cart ROM to\n");
	printf("\"fat:/gm9i/out\"?\n");
	printf("(<A> yes, <B> no)");

	consoleSelect(&topConsole);
	printf ("\x1B[42m");		// Print green color
	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	do {
		// Move to right side of screen
		printf ("\x1b[0;26H");
		// Print time
		printf ("_%s" ,RetTime().c_str());

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & KEY_A) && !(pressed & KEY_B));

	if (pressed & KEY_A) {
		printf ("\x1b[0;27H");
		printf ("\x1B[42m");		// Print green color
		printf ("_____");	// Clear time
	}

	consoleSelect(&bottomConsole);
	printf ("\x1B[47m");		// Print foreground white color

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
		char destSavPath[256];
		snprintf(destPath, sizeof(destPath), "fat:/gm9i/out/%s_%s%s_%x.gba", gbaHeaderGameTitle, gbaHeaderGameCode, gbaHeaderMakerCode, gbaHeaderSoftwareVersion);
		snprintf(destSavPath, sizeof(destSavPath), "fat:/gm9i/out/%s_%s%s_%x.sav", gbaHeaderGameTitle, gbaHeaderGameCode, gbaHeaderMakerCode, gbaHeaderSoftwareVersion);
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
		// Save file
		remove(destSavPath);
		destinationFile = fopen(destSavPath, "wb");
		fwrite((void*)0x0A000000, 1, 0x10000, destinationFile);
		fclose(destinationFile);
	}
}

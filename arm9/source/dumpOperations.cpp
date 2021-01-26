#include <nds.h>
#include <nds/arm9/dldi.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include "auxspi.h"
#include "date.h"
#include "driveOperations.h"
#include "ndsheaderbanner.h"
#include "read_card.h"
#include "tonccpy.h"

extern u8 copyBuf[];

extern bool expansionPakFound;

extern PrintConsole topConsole, bottomConsole;

static sNDSHeaderExt ndsCardHeader;

//---------------------------------------------------------------------------------
// https://github.com/devkitPro/libnds/blob/master/source/common/cardEeprom.c#L88
// with type 2 fixed if the first word and another % 8192 location are 0x00000000
uint32 cardEepromGetSizeFixed() {
//---------------------------------------------------------------------------------

	int type = cardEepromGetType();

	if(type == -1)
		return 0;
	if(type == 0)
		return 8192;
	if(type == 1)
		return 512;
	if(type == 2) {
		u32 buf1,buf2,buf3 = 0x54536554; // "TeST"
		// Save the first word of the EEPROM
		cardReadEeprom(0,(u8*)&buf1,4,type);

		// Write "TeST" to it
		cardWriteEeprom(0,(u8*)&buf3,4,type);

		// Loop until the EEPROM mirrors and the first word shows up again
		int size = 8192;
		while (1) {
			cardReadEeprom(size,(u8*)&buf2,4,type);
			if ( buf2 == buf3 ) break;
			size += 8192;
		}

		// Restore the first word
		cardWriteEeprom(0,(u8*)&buf1,4,type);

		return size;
	}

	int device;

	if(type == 3) {
		int id = cardEepromReadID();

		device = id & 0xffff;
		
		if ( ((id >> 16) & 0xff) == 0x20 ) { // ST
			
			switch(device) {

			case 0x4014:
				return 1024*1024;		//	8Mbit(1 meg)
				break;
			case 0x4013:
			case 0x8013:				// M25PE40
				return 512*1024;		//	4Mbit(512KByte)
				break;
			case 0x2017:
				return 8*1024*1024;		//	64Mbit(8 meg)
				break;
			}
		}

		if ( ((id >> 16) & 0xff) == 0x62 ) { // Sanyo
			
			if (device == 0x1100)
				return 512*1024;		//	4Mbit(512KByte)

		}

		if ( ((id >> 16) & 0xff) == 0xC2 ) { // Macronix
			
			if (device == 0x2211)
				return 128*1024;		//	1Mbit(128KByte) - MX25L1021E
		}
		

		return 256*1024;		//	2Mbit(256KByte)
	}

	return 0;
}

void ndsCardSaveDump(const char* filename) {
	FILE *out = fopen(filename, "wb");
	if(out) {
		consoleClear();
		printf("Dumping save...\n");
		printf("Do not remove the NDS card.\n");
		unsigned char *buffer;
		auxspi_extra card_type = auxspi_has_extra();
		if(card_type == AUXSPI_INFRARED) {
			int size = auxspi_save_size_log_2(card_type);
			int size_blocks;
			int type = auxspi_save_type(card_type);
			if(size < 16)
				size_blocks = 1;
			else
				size_blocks = 1 << (size - 16);
			u32 LEN = std::min(1 << size, 1 << 16);
			buffer = new unsigned char[LEN*size_blocks];
			auxspi_read_data(0, buffer, LEN*size_blocks, type, card_type);
			fwrite(buffer, 1, LEN*size_blocks, out);
		} else {
			int type = cardEepromGetType();
			int size = cardEepromGetSizeFixed();
			buffer = new unsigned char[size];
			cardReadEeprom(0, buffer, size, type);
			fwrite(buffer, 1, size, out);
		}
		delete[] buffer;
		fclose(out);
	}
}

void ndsCardSaveRestore(const char *filename) {
	consoleSelect(&bottomConsole);
	consoleClear();
	printf ("\x1B[47m"); // Print foreground white color
	printf("Restore the selected save to the"); // Line is 32 chars
	printf("inserted game card?\n"); // Line is 32 chars
	printf("(<A> yes, <B> no)\n");

	consoleSelect(&topConsole);
	printf ("\x1B[30m"); // Print black color
	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	u16 pressed;
	do {
		// Move to right side of screen
		printf ("\x1b[0;26H");
		// Print time
		printf (" %s" ,RetTime().c_str());

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & (KEY_A | KEY_B)));

	if(pressed & KEY_A) {
		consoleSelect(&bottomConsole);
		consoleClear();

		auxspi_extra card_type = auxspi_has_extra();
		bool auxspi = card_type == AUXSPI_INFRARED;
		FILE *in = fopen(filename, "rb");
		if(in) {
			unsigned char *buffer;
			int size;
			int type;
			int length;
			unsigned int num_blocks = 0, shift = 0, LEN = 0;
			if(auxspi) {
				size = auxspi_save_size_log_2(card_type);
				type = auxspi_save_type(card_type);
				switch(type) {
				case 1:
					shift = 4; // 16 bytes
					break;
				case 2:
					shift = 5; // 32 bytes
					break;
				case 3:
					shift = 8; // 256 bytes
					break;
				default:
					return;
				}
				LEN = 1 << shift;
				num_blocks = 1 << (size - shift);
			} else {
				type = cardEepromGetType();
				size = cardEepromGetSizeFixed();
			}
			fseek(in, 0, SEEK_END);
			length = ftell(in);
			fseek(in, 0, SEEK_SET);
			if(length != (auxspi ? (int)(LEN*num_blocks) : size)) {
				fclose(in);
				printf("\x1B[41m"); // Print foreground red color
				printf("The size of this save doesn't\n");
				printf("match the size of the size of\n");
				printf("the inserted game card.\n\n");
				printf("Write cancelled!\n");
				printf("\x1B[47m"); // Print foreground white color
				printf("(<A> OK)\n");

				consoleSelect(&topConsole);
				printf ("\x1B[30m"); // Print black color
				do {
					// Move to right side of screen
					printf ("\x1b[0;26H");
					// Print time
					printf (" %s" ,RetTime().c_str());

					scanKeys();
					pressed = keysDownRepeat();
					swiWaitForVBlank();
				} while (!(pressed & KEY_A));
				return;
			}
			printf("Restoring save...\nDo not remove the NDS card.\n\n\n\n\n\n\nProgress:");
			if(type == 3) {
				if(auxspi)
					auxspi_erase(card_type);
				else
					cardEepromChipErase();
			}
			if(auxspi){
				buffer = new unsigned char[LEN];
				for(unsigned int i = 0; i < num_blocks; i++) {
					printf ("\x1b[9;0H");
					printf ("%d/%d Bytes", i * LEN, length);

					fread(buffer, 1, LEN, in);
					auxspi_write_data(i << shift, buffer, LEN, type, card_type);
				}
			} else {
				int blocks = size / 32;
				int written = 0;
				buffer = new unsigned char[blocks];
				for(unsigned int i = 0; i < 32; i++) {
					printf ("\x1b[9;0H");
					printf ("%d/%d Bytes", i * blocks, length);
					fread(buffer, 1, blocks, in);
					cardWriteEeprom(written, buffer, blocks, type);
					written += blocks;
				}
			}
			delete[] buffer;
			fclose(in);
		}
	}
}

void ndsCardDump(void) {
	int pressed = 0;
	//bool showGameCardMsgAgain = false;

	consoleSelect(&bottomConsole);
	consoleClear();
	printf ("\x1B[47m");		// Print foreground white color
	printf("Dump NDS card ROM to\n");
	printf("\"%s:/gm9i/out\"?\n", (sdMounted ? "sd" : "fat"));
	printf("(<A> yes, <Y> trim, <B> no,\n");
	printf(" <X> save only)");

	consoleSelect(&topConsole);
	printf ("\x1B[30m");		// Print black color
	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	do {
		// Move to right side of screen
		printf ("\x1b[0;26H");
		// Print time
		printf (" %s" ,RetTime().c_str());

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & (KEY_A | KEY_Y | KEY_B | KEY_X)));

	consoleSelect(&bottomConsole);
	printf ("\x1B[47m");		// Print foreground white color

	if (pressed & KEY_X) {
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
		if (cardInit(&ndsCardHeader) != 0) {
			printf("Unable to dump the save.\n");
			for (int i = 0; i < 60*2; i++) {
				swiWaitForVBlank();
			}
			return;
		}
		char gameTitle[13] = {0};
		tonccpy(gameTitle, ndsCardHeader.gameTitle, 12);
		char gameCode[7] = {0};
		tonccpy(gameCode, ndsCardHeader.gameCode, 6);
		char destSavPath[256];
		sprintf(destSavPath, "%s:/gm9i/out/%s_%s_%x.sav", (sdMounted ? "sd" : "fat"), gameTitle, gameCode, ndsCardHeader.romversion);
		ndsCardSaveDump(destSavPath);
	} else
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
		/*if (expansionPakFound) {
			consoleClear();
			printf("Please switch to the\ngame card, then press A.\n");
			//flashcardUnmount();
			io_dldi_data->ioInterface.shutdown();
	
			consoleSelect(&topConsole);
			printf ("\x1B[30m");		// Print black color
			// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
			do {
				// Move to right side of screen
				printf ("\x1b[0;26H");
				// Print time
				printf (" %s" ,RetTime().c_str());

				scanKeys();
				pressed = keysDownRepeat();
				swiWaitForVBlank();
			} while (!(pressed & KEY_A));

			consoleSelect(&bottomConsole);
			printf ("\x1B[47m");		// Print foreground white color
		}*/
		consoleClear();
		if (cardInit(&ndsCardHeader) == 0) {
			printf("Dumping...\n");
			printf("Do not remove the NDS card.\n");
		} else {
			printf("Unable to dump the ROM.\n");
			for (int i = 0; i < 60*2; i++) {
				swiWaitForVBlank();
			}
			return;
		}
		char gameTitle[13] = {0};
		tonccpy(gameTitle, ndsCardHeader.gameTitle, 12);
		char gameCode[7] = {0};
		tonccpy(gameCode, ndsCardHeader.gameCode, 6);
		if (gameTitle[0] == 0 || gameTitle[0] == 0x2E || gameTitle[0] == 0xFF) {
			sprintf(gameTitle, "NO-TITLE");
		}
		if (gameCode[0] == 0 || gameCode[0] == 0x23 || gameCode[0] == 0xFF) {
			sprintf(gameCode, "NONE00");
		}
		bool trimRom = (pressed & KEY_Y);
		char destPath[256];
		sprintf(destPath, "%s:/gm9i/out/%s_%s_%x%s.nds", (sdMounted ? "sd" : "fat"), gameTitle, gameCode, ndsCardHeader.romversion, (trimRom ? "_trim" : ""));
		char destSavPath[256];
		sprintf(destSavPath, "%s:/gm9i/out/%s_%s_%x%s.sav", (sdMounted ? "sd" : "fat"), gameTitle, gameCode, ndsCardHeader.romversion, (trimRom ? "_trim" : ""));
		// Determine ROM size
		u32 romSize = 0;
		if (trimRom) {
			romSize = ((ndsCardHeader.unitCode != 0) && (ndsCardHeader.twlRomSize > 0))
						? ndsCardHeader.twlRomSize : ndsCardHeader.romSize;
		} else switch (ndsCardHeader.deviceSize) {
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
		/*if (expansionPakFound) {
			u32 currentSize = ((expansionPakFound && romSize > 0x800000) ? 0x800000 : romSize);
			u32 src = 0;
			u32 writeSrc = 0;
			FILE* destinationFile;
			bool destinationFileOpened = false;
			while (currentSize > 0) {
				if (showGameCardMsgAgain) {
					printf ("\x1b[8;0H");
					printf ("          \n");

					printf ("\x1b[15;0H");
					printf("Please switch to the\ngame card, then press A.\n");
					//flashcardUnmount();
					io_dldi_data->ioInterface.shutdown();

					consoleSelect(&topConsole);
					printf ("\x1B[30m");		// Print black color
					// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
					do {
						// Move to right side of screen
						printf ("\x1b[0;26H");
						// Print time
						printf (" %s" ,RetTime().c_str());

						scanKeys();
						pressed = keysDownRepeat();
						swiWaitForVBlank();
					} while (!(pressed & KEY_A));

					consoleSelect(&bottomConsole);
					printf ("\x1b[15;0H");
					printf("                    \n                        \n");
					cardInit(&ndsCardHeader);
				}
				showGameCardMsgAgain = true;

				// Read from game card
				for (src = src; src < currentSize; src += 0x200) {
					consoleSelect(&topConsole);
					printf ("\x1B[30m");		// Print black color
					// Move to right side of screen
					printf ("\x1b[0;26H");
					// Print time
					printf (" %s" ,RetTime().c_str());

					consoleSelect(&bottomConsole);
					printf ("\x1B[47m");		// Print foreground white color
					printf ("\x1b[8;0H");
					printf ("Read:\n");
					printf ("%i/%i Bytes                       ", (int)src, (int)romSize);
					cardRead (src, (void*)0x09000000+(src % 0x800000));
				}
				printf ("\x1b[15;0H");
				printf("Please switch to the\nflashcard, then press A.\n");
				consoleSelect(&topConsole);
				printf ("\x1B[30m");		// Print black color
				// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
				do {
					// Move to right side of screen
					printf ("\x1b[0;26H");
					// Print time
					printf (" %s" ,RetTime().c_str());

					scanKeys();
					pressed = keysDownRepeat();
					swiWaitForVBlank();
				} while (!(pressed & KEY_A));

				consoleSelect(&bottomConsole);
				printf ("\x1b[15;0H");
				printf("                    \n                        \n");

				printf ("\x1B[47m");		// Print foreground white color
				printf ("\x1b[11;0H");
				printf ("Written:\n");

				// Write back to flashcard
				cardInit(&ndsCardHeader);
				io_dldi_data->ioInterface.startup();
				//flashcardMounted = flashcardMount();
				if (!destinationFileOpened) {
					destinationFile = fopen(destPath, "wb");
					destinationFileOpened = true;
				}
				for (writeSrc = writeSrc; writeSrc < currentSize; writeSrc += 0x200) {
					consoleSelect(&topConsole);
					printf ("\x1B[30m");		// Print black color
					// Move to right side of screen
					printf ("\x1b[0;26H");
					// Print time
					printf (" %s" ,RetTime().c_str());

					consoleSelect(&bottomConsole);
					printf ("\x1B[47m");		// Print foreground white color
					printf ("\x1b[12;0H");
					printf ("%i/%i Bytes                       ", (int)writeSrc, (int)romSize);
					fwrite((void*)0x09000000+(writeSrc % 0x800000), 1, currentSize, destinationFile);
				}

				currentSize -= 0x800000;
			}
			fclose(destinationFile);
		} else {*/
			remove(destPath);
			u32 currentSize = romSize;
			FILE* destinationFile = fopen(destPath, "wb");
			if (destinationFile) {
				for (u32 src = 0; src < romSize; src += 0x8000) {
					consoleSelect(&topConsole);
					printf ("\x1B[30m");		// Print black color
					// Move to right side of screen
					printf ("\x1b[0;26H");
					// Print time
					printf (" %s" ,RetTime().c_str());

					consoleSelect(&bottomConsole);
					printf ("\x1B[47m");		// Print foreground white color
					printf ("\x1b[8;0H");
					printf ("Progress:\n");
					printf ("%i/%i Bytes", (int)src, (int)romSize);
					for (u32 i = 0; i < 0x8000; i += 0x200) {
						cardRead (src+i, copyBuf+i);
					}
					fwrite(copyBuf, 1, (currentSize>=0x8000 ? 0x8000 : currentSize), destinationFile);
					currentSize -= 0x8000;
				}
				fclose(destinationFile);
			}
			ndsCardSaveDump(destSavPath);
		//}
	}
}


void writeChange(const u32* buffer) {
	// Input registers are at 0x08800000 - 0x088001FF
	*(vu32*) 0x08800184 = buffer[1];
	*(vu32*) 0x08800188 = buffer[2];
	*(vu32*) 0x0880018C = buffer[3];

	*(vu32*) 0x08800180 = buffer[0];
}

void readChange(void) {
	// Output registers are at 0x08800100 - 0x088001FF
	while (*(vu32*) 0x08000180 & 0x1000); // Busy bit
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
	printf ("\x1B[30m");		// Print black color
	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	do {
		// Move to right side of screen
		printf ("\x1b[0;26H");
		// Print time
		printf (" %s" ,RetTime().c_str());

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & KEY_A) && !(pressed & KEY_B));

	if (pressed & KEY_A) {
		printf ("\x1b[0;27H");
		printf ("     ");	// Clear time
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
		if (*(u8*)(0x080000A0) == 0 || *(u8*)(0x080000A0) == 0xFF) {
			sprintf(gbaHeaderGameTitle, "NO-TITLE");
		} else for (int i = 0; i < 12; i++) {
			gbaHeaderGameTitle[i] = *(char*)(0x080000A0+i);
			if (*(u8*)(0x080000A0+i) == 0) {
				break;
			}
		}
		if (*(u8*)(0x080000AC) == 0 || *(u8*)(0x080000AC) == 0xFF) {
			sprintf(gbaHeaderGameCode, "NONE");
		} else for (int i = 0; i < 4; i++) {
			gbaHeaderGameCode[i] = *(char*)(0x080000AC+i);
			if (*(u8*)(0x080000AC+i) == 0) {
				break;
			}
		}
		if (*(u8*)(0x080000B0) == 0 || *(u8*)(0x080000B0) == 0xFF) {
			sprintf(gbaHeaderMakerCode, "00");
		} else for (int i = 0; i < 2; i++) {
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
		// Reset data at virtual address
		u32 rstCmd[4] = {
			0x11, // Command
			0x1000, // ROM address
			0x08001000, // Virtual address
			0x8, // Size (in 0x200 byte blocks)
		};
		writeChange(rstCmd);
		FILE* destinationFile = fopen(destPath, "wb");
		if (destinationFile) {
			fwrite(GBAROM, 1, romSize, destinationFile);
			// Check for 64MB GBA Video ROM
			if (strncmp((char*)0x080000AC, "MSAE", 4)==0	// Shark Tale
			|| strncmp((char*)0x080000AC, "MSKE", 4)==0	// Shrek
			|| strncmp((char*)0x080000AC, "MSTE", 4)==0	// Shrek & Shark Tale
			|| strncmp((char*)0x080000AC, "M2SE", 4)==0	// Shrek 2
			) {
				// Dump last 32MB
				u32 cmd[4] = {
					0x11, // Command
					0, // ROM address
					0x08001000, // Virtual address
					0x8, // Size (in 0x200 byte blocks)
				};

				size_t i;
				for (i = 0x02000000; i < 0x04000000; i += 0x1000) {
					cmd[1] = i,
					writeChange(cmd);
					readChange();
					fwrite(GBAROM + (0x1000 >> 1), 0x1000, 1, destinationFile);
				}
			}
			fclose(destinationFile);
		}
		// Save file
		remove(destSavPath);
		destinationFile = fopen(destSavPath, "wb");
		fwrite((void*)0x0A000000, 1, 0x10000, destinationFile);
		fclose(destinationFile);
	}
}

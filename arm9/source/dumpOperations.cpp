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
// https://github.com/devkitPro/libnds/blob/master/source/common/cardEeprom.c#L74
// with Pokémon Mystery Dungeon - Explorers of Sky (128 KiB EEPROM) fixed
int cardEepromGetTypeFixed(void) {
//---------------------------------------------------------------------------------
	int sr = cardEepromCommand(SPI_EEPROM_RDSR);
	int id = cardEepromReadID();
	
	if (( sr == 0xff && id == 0xffffff) || ( sr == 0 && id == 0 )) return -1;
	if ( sr == 0xf0 && id == 0xffffff ) return 1;
	if ( sr == 0x00 && id == 0xffffff ) return 2;
	if ( id != 0xffffff || ( sr == 0x02 && id == 0xffffff )) return 3;
	
	return 0;
}

//---------------------------------------------------------------------------------
// https://github.com/devkitPro/libnds/blob/master/source/common/cardEeprom.c#L88
// with type 2 fixed if the first word and another % 8192 location are 0x00000000
uint32 cardEepromGetSizeFixed() {
//---------------------------------------------------------------------------------

	int type = cardEepromGetTypeFixed();

	if(type == -1)
		return 0;
	if(type == 0)
		return 8192;
	if(type == 1)
		return 512;
	if(type == 2) {
		u32 buf1,buf2,buf3 = 0x54534554; // "TEST"
		// Save the first word of the EEPROM
		cardReadEeprom(0,(u8*)&buf1,4,type);

		// Write "TEST" to it
		cardWriteEeprom(0,(u8*)&buf3,4,type);

		// Loop until the EEPROM mirrors and the first word shows up again
		int size = 8192;
		while (1) {
			cardReadEeprom(size,(u8*)&buf2,4,type);
			// Check if it matches, if so check again with another value to ensure no false positives
			if (buf2 == buf3) {
				u32 buf4 = 0x74736574; // "test"
				// Write "test" to the first word
				cardWriteEeprom(0,(u8*)&buf4,4,type);

				// Check if it still matches
				cardReadEeprom(size,(u8*)&buf2,4,type);
				if (buf2 == buf4) break;

				// False match, write "TEST" back and keep going
				cardWriteEeprom(0,(u8*)&buf3,4,type);
			}
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

		if (id == 0xffffff) {
			int sr = cardEepromCommand(SPI_EEPROM_RDSR);
			if (sr == 2) { // Pokémon Mystery Dungeon - Explorers of Sky
				return 128*1024; // 1Mbit (128KByte)
			}
		}
		

		return 256*1024;		//	2Mbit(256KByte)
	}

	return 0;
}

void ndsCardSaveDump(const char* filename) {
	FILE *out = fopen(filename, "wb");
	if(out) {
		consoleClear();
		iprintf("Dumping save...\n");
		iprintf("Do not remove the NDS card.\n");
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
			int type = cardEepromGetTypeFixed();
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
	iprintf("\x1B[47m"); // Print foreground white color
	iprintf("Restore the selected save to the"); // Line is 32 chars
	iprintf("inserted game card?\n"); // Line is 32 chars
	iprintf("(<A> yes, <B> no)\n");

	consoleSelect(&topConsole);
	iprintf ("\x1B[30m"); // Print black color
	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	u16 pressed;
	do {
		// Move to right side of screen
		iprintf ("\x1b[0;26H");
		// Print time
		iprintf (" %s" ,RetTime().c_str());

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
				type = cardEepromGetTypeFixed();
				size = cardEepromGetSizeFixed();
			}
			fseek(in, 0, SEEK_END);
			length = ftell(in);
			fseek(in, 0, SEEK_SET);
			if(length != (auxspi ? (int)(LEN*num_blocks) : size)) {
				fclose(in);
				iprintf("\x1B[41m"); // Print foreground red color
				iprintf("The size of this save doesn't\n");
				iprintf("match the size of the size of\n");
				iprintf("the inserted game card.\n\n");
				iprintf("Write cancelled!\n");
				iprintf("\x1B[47m"); // Print foreground white color
				iprintf("(<A> OK)\n");

				consoleSelect(&topConsole);
				iprintf ("\x1B[30m"); // Print black color
				do {
					// Move to right side of screen
					iprintf ("\x1b[0;26H");
					// Print time
					iprintf (" %s" ,RetTime().c_str());

					scanKeys();
					pressed = keysDownRepeat();
					swiWaitForVBlank();
				} while (!(pressed & KEY_A));
				return;
			}
			iprintf("Restoring save...\nDo not remove the NDS card.\n\n\n\n\n\n\nProgress:");
			if(type == 3) {
				if(auxspi)
					auxspi_erase(card_type);
				else
					cardEepromChipErase();
			}
			if(auxspi){
				buffer = new unsigned char[LEN];
				for(unsigned int i = 0; i < num_blocks; i++) {
					iprintf ("\x1b[9;0H");
					iprintf ("%d/%d Bytes", i * LEN, length);

					fread(buffer, 1, LEN, in);
					auxspi_write_data(i << shift, buffer, LEN, type, card_type);
				}
			} else {
				int blocks = size / 32;
				int written = 0;
				buffer = new unsigned char[blocks];
				for(unsigned int i = 0; i < 32; i++) {
					iprintf ("\x1b[9;0H");
					iprintf ("%d/%d Bytes", i * blocks, length);
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

void dumpFailMsg(void) {
	consoleClear();
	iprintf("Failed to dump the ROM.\n");
	for (int i = 0; i < 60*2; i++) {
		swiWaitForVBlank();
	}
}

void ndsCardDump(void) {
	int pressed = 0;
	//bool showGameCardMsgAgain = false;

	consoleSelect(&bottomConsole);
	consoleClear();
	iprintf("\x1B[47m");		// Print foreground white color
	iprintf("Dump NDS card ROM to\n");
	iprintf("\"%s:/gm9i/out\"?\n", (sdMounted ? "sd" : "fat"));
	iprintf("(<A> yes, <Y> trim, <B> no,\n");
	iprintf(" <X> save only)");

	consoleSelect(&topConsole);
	iprintf ("\x1B[30m");		// Print black color
	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	do {
		// Move to right side of screen
		iprintf ("\x1b[0;26H");
		// Print time
		iprintf (" %s" ,RetTime().c_str());

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & (KEY_A | KEY_Y | KEY_B | KEY_X)));

	consoleSelect(&bottomConsole);
	iprintf ("\x1B[47m");		// Print foreground white color

	if (pressed & KEY_X) {
		consoleClear();
		char folderPath[2][256];
		sprintf(folderPath[0], "%s:/gm9i", (sdMounted ? "sd" : "fat"));
		sprintf(folderPath[1], "%s:/gm9i/out", (sdMounted ? "sd" : "fat"));
		if (access(folderPath[0], F_OK) != 0) {
			iprintf("Creating directory...");
			mkdir(folderPath[0], 0777);
		}
		if (access(folderPath[1], F_OK) != 0) {
			iprintf("\x1b[0;0H");
			iprintf("Creating directory...");
			mkdir(folderPath[1], 0777);
		}
		consoleClear();
		if (cardInit(&ndsCardHeader) != 0) {
			iprintf("Unable to dump the save.\n");
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
		sprintf(destSavPath, "%s:/gm9i/out/%s_%s_%02x.sav", (sdMounted ? "sd" : "fat"), gameTitle, gameCode, ndsCardHeader.romversion);
		ndsCardSaveDump(destSavPath);
	} else
	if ((pressed & KEY_A) || (pressed & KEY_Y)) {
		consoleClear();
		bool trimRom = (pressed & KEY_Y);
		char folderPath[2][256];
		sprintf(folderPath[0], "%s:/gm9i", (sdMounted ? "sd" : "fat"));
		sprintf(folderPath[1], "%s:/gm9i/out", (sdMounted ? "sd" : "fat"));
		if (access(folderPath[0], F_OK) != 0) {
			iprintf("Creating directory...");
			mkdir(folderPath[0], 0777);
		}
		if (access(folderPath[1], F_OK) != 0) {
			iprintf("\x1b[0;0H");
			iprintf("Creating directory...");
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
		int cardInited = cardInit(&ndsCardHeader);
		char gameTitle[13] = {0};
		char gameCode[7] = {0};
		char destPath[256] = {0};
		char destSavPath[256] = {0};
		char fileName[32] = {0};
		tonccpy(gameTitle, ndsCardHeader.gameTitle, 12);
		tonccpy(gameCode, ndsCardHeader.gameCode, 6);
		if (gameTitle[0] == 0 || gameTitle[0] == 0x2E || gameTitle[0] == 0xFF) {
			sprintf(gameTitle, "NO-TITLE");
		} else {
			for(uint i = 0; i < sizeof(gameTitle); i++) {
				switch(gameTitle[i]) {
					case '>':
					case '<':
					case ':':
					case '"':
					case '/':
					case '\x5C':
					case '|':
					case '?':
					case '*':
						gameTitle[i] = '_';
				}
			}
		}
		if (gameCode[0] == 0 || gameCode[0] == 0x23 || gameCode[0] == 0xFF) {
			sprintf(gameCode, "NONE00");
		}
		sprintf(fileName, "%s_%s_%02x%s", gameTitle, gameCode, ndsCardHeader.romversion, (trimRom ? "_trim" : ""));
		sprintf(destPath, "%s:/gm9i/out/%s.nds", (sdMounted ? "sd" : "fat"), fileName);
		sprintf(destSavPath, "%s:/gm9i/out/%s.sav", (sdMounted ? "sd" : "fat"), fileName);

		if (cardInited == 0) {
			iprintf("%s.nds\nis dumping...\n", fileName);
			iprintf("Do not remove the NDS card.\n");
		} else {
			iprintf("Unable to dump the ROM.\n");
			for (int i = 0; i < 60*2; i++) {
				swiWaitForVBlank();
			}
			return;
		}
		// Determine ROM size
		u32 romSize = 0;
		if (trimRom) {
			romSize = (isDSiMode() && (ndsCardHeader.unitCode != 0) && (ndsCardHeader.twlRomSize > 0))
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
					iprintf ("\x1b[8;0H");
					iprintf ("          \n");

					iprintf("\x1b[15;0H");
					iprintf("Please switch to the\ngame card, then press A.\n");
					//flashcardUnmount();
					io_dldi_data->ioInterface.shutdown();

					consoleSelect(&topConsole);
					iprintf ("\x1B[30m");		// Print black color
					// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
					do {
						// Move to right side of screen
						iprintf ("\x1b[0;26H");
						// Print time
						iprintf (" %s" ,RetTime().c_str());

						scanKeys();
						pressed = keysDownRepeat();
						swiWaitForVBlank();
					} while (!(pressed & KEY_A));

					consoleSelect(&bottomConsole);
					iprintf ("\x1b[15;0H");
					iprintf("                    \n                        \n");
					cardInit(&ndsCardHeader);
				}
				showGameCardMsgAgain = true;

				// Read from game card
				for (src = src; src < currentSize; src += 0x200) {
					consoleSelect(&topConsole);
					iprintf ("\x1B[30m");		// Print black color
					// Move to right side of screen
					iprintf ("\x1b[0;26H");
					// Print time
					iprintf (" %s" ,RetTime().c_str());

					consoleSelect(&bottomConsole);
					iprintf ("\x1B[47m");		// Print foreground white color
					iprintf ("\x1b[8;0H");
					iprintf ("Read:\n");
					iprintf ("%i/%i Bytes                       ", (int)src, (int)romSize);
					cardRead (src, (void*)0x09000000+(src % 0x800000));
				}
				iprintf("\x1b[15;0H");
				iprintf("Please switch to the\nflashcard, then press A.\n");
				consoleSelect(&topConsole);
				iprintf ("\x1B[30m");		// Print black color
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
				iprintf("\x1b[15;0H");
				iprintf("                    \n                        \n");

				iprintf ("\x1B[47m");		// Print foreground white color
				iprintf ("\x1b[11;0H");
				iprintf ("Written:\n");

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
					iprintf ("\x1B[30m");		// Print black color
					// Move to right side of screen
					iprintf ("\x1b[0;26H");
					// Print time
					iprintf (" %s" ,RetTime().c_str());

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
					iprintf ("\x1B[30m");		// Print black color
					// Move to right side of screen
					iprintf ("\x1b[0;26H");
					// Print time
					iprintf (" %s" ,RetTime().c_str());

					consoleSelect(&bottomConsole);
					iprintf ("\x1B[47m");		// Print foreground white color
					iprintf ("\x1b[8;0H");
					iprintf ("Progress:\n");
					iprintf ("%i/%i Bytes", (int)src, (int)romSize);
					for (u32 i = 0; i < 0x8000; i += 0x200) {
						cardRead (src+i, copyBuf+i);
					}
					if (fwrite(copyBuf, 1, (currentSize>=0x8000 ? 0x8000 : currentSize), destinationFile) < 1) {
						dumpFailMsg();
						break;
					}
					currentSize -= 0x8000;
				}
				fclose(destinationFile);
			} else {
				dumpFailMsg();
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
	iprintf("\x1B[47m");		// Print foreground white color
	iprintf("Dump GBA cart ROM to\n");
	iprintf("\"fat:/gm9i/out\"?\n");
	iprintf("(<A> yes, <B> no)");

	consoleSelect(&topConsole);
	iprintf ("\x1B[30m");		// Print black color
	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	do {
		// Move to right side of screen
		iprintf ("\x1b[0;26H");
		// Print time
		iprintf (" %s" ,RetTime().c_str());

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & KEY_A) && !(pressed & KEY_B));

	if (pressed & KEY_A) {
		iprintf ("\x1b[0;27H");
		iprintf ("     ");	// Clear time
	}

	consoleSelect(&bottomConsole);
	printf ("\x1B[47m");		// Print foreground white color

	if (pressed & KEY_A) {
		consoleClear();
		if (access("fat:/gm9i", F_OK) != 0) {
			iprintf("Creating directory...");
			mkdir("fat:/gm9i", 0777);
		}
		if (access("fat:/gm9i/out", F_OK) != 0) {
			iprintf ("\x1b[0;0H");
			iprintf("Creating directory...");
			mkdir("fat:/gm9i/out", 0777);
		}
		char gbaHeaderGameTitle[13] = "\0";
		tonccpy((u8*)(0x080000A0), &gbaHeaderGameTitle, 12);
		char gbaHeaderGameCode[5] = "\0";
		tonccpy((u8*)(0x080000AC), &gbaHeaderGameCode, 4);
		char gbaHeaderMakerCode[3] = "\0";
		tonccpy((u8*)(0x080000B0), &gbaHeaderMakerCode, 2);
		if (gbaHeaderGameTitle[0] == 0 || gbaHeaderGameTitle[0] == 0xFF) {
			sprintf(gbaHeaderGameTitle, "NO-TITLE");
		} else {
			for(uint i = 0; i < sizeof(gbaHeaderGameTitle); i++) {
				switch(gbaHeaderGameTitle[i]) {
					case '>':
					case '<':
					case ':':
					case '"':
					case '/':
					case '\x5C':
					case '|':
					case '?':
					case '*':
						gbaHeaderGameTitle[i] = '_';
				}
			}
		}
		if (gbaHeaderGameCode[0] == 0 || gbaHeaderGameCode[0] == 0xFF) {
			sprintf(gbaHeaderGameCode, "NONE");
		}
		if (gbaHeaderMakerCode[0] == 0 || gbaHeaderMakerCode[0] == 0xFF) {
			sprintf(gbaHeaderMakerCode, "00");
		}
		u8 gbaHeaderSoftwareVersion = *(u8*)(0x080000BC);
		char fileName[32] = {0};
		sprintf(fileName, "%s_%s%s_%x", gbaHeaderGameTitle, gbaHeaderGameCode, gbaHeaderMakerCode, gbaHeaderSoftwareVersion);
		char destPath[256] = {0};
		char destSavPath[256] = {0};
		sprintf(destPath, "fat:/gm9i/out/%s.gba", fileName);
		sprintf(destSavPath, "fat:/gm9i/out/%s.sav", fileName);
		consoleClear();
		iprintf("%s.gba\nis dumping...\n", fileName);
		iprintf("Do not remove the GBA cart.\n");
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
			if (fwrite(GBAROM, 1, romSize, destinationFile) < 1) {
				dumpFailMsg();
			} else
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
					if (fwrite(GBAROM + (0x1000 >> 1), 0x1000, 1, destinationFile) < 1) {
						dumpFailMsg();
						break;
					}
				}
			}
			fclose(destinationFile);
		} else {
			dumpFailMsg();
		}
		// Save file
		remove(destSavPath);
		destinationFile = fopen(destSavPath, "wb");
		fwrite((void*)0x0A000000, 1, 0x10000, destinationFile);
		fclose(destinationFile);
	}
}

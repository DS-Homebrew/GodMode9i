#include "dumpOperations.h"

#include "auxspi.h"
#include "date.h"
#include "driveOperations.h"
#include "fileOperations.h"
#include "font.h"
#include "gba.h"
#include "lzss.h"
#include "ndsheaderbanner.h"
#include "read_card.h"
#include "tonccpy.h"
#include "language.h"
#include "screenshot.h"
#include "version.h"

#include <dirent.h>
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#include <unistd.h>
#include <vector>

extern u8 copyBuf[];

static sNDSHeaderExt ndsCardHeader;

enum DumpOption {
	none = 0,
	rom = 1,
	romTrimmed = 2,
	save = 4,
	metadata = 8,
	ndsSave = 16,
	all = rom | save | metadata,
	allTrimmed = romTrimmed | save | metadata
};

DumpOption dumpMenu(std::vector<DumpOption> allowedOptions, const char *dumpName) {
	u16 pressed = 0, held = 0;
	int optionOffset = 0;

	char dumpToStr[256];
	if(sdMounted || flashcardMounted)
		snprintf(dumpToStr, sizeof(dumpToStr), STR_DUMP_TO.c_str(), dumpName, sdMounted ? "sd" : "fat");
	else
		snprintf(dumpToStr, sizeof(dumpToStr), STR_DUMP_TO_GBA.c_str(), dumpName);

	int y = font->calcHeight(dumpToStr) + 1;

	while (true) {
		font->clear(false);

		font->print(0, 0, false, dumpToStr);

		int row = y;
		for(DumpOption option : allowedOptions) {
			switch(option) {
				case DumpOption::all:
					font->print(3, row++, false, STR_DUMP_ALL);
					break;
				case DumpOption::allTrimmed:
					font->print(3, row++, false, STR_DUMP_ALL_TRIMMED);
					break;
				case DumpOption::rom:
					font->print(3, row++, false, STR_DUMP_ROM);
					break;
				case DumpOption::romTrimmed:
					font->print(3, row++, false, STR_DUMP_ROM_TRIMMED);
					break;
				case DumpOption::save:
					font->print(3, row++, false, STR_DUMP_SAVE);
					break;
				case DumpOption::ndsSave:
					font->print(3, row++, false, STR_DUMP_DS_SAVE);
					break;
				case DumpOption::metadata:
					font->print(3, row++, false, STR_DUMP_METADATA);
					break;
				case DumpOption::none:
					row++;
					break;
			}
		}

		font->print(3, ++row, false, STR_A_SELECT_B_CANCEL);

		// Show cursor
		font->print(0, y + optionOffset, false, "->");

		font->update(false);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			// Print time
			font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
			font->update(true);

			scanKeys();
			pressed = keysDownRepeat();
			held = keysHeld();
			swiWaitForVBlank();
		} while (!(pressed & (KEY_UP| KEY_DOWN | KEY_A | KEY_B | KEY_L))
#ifdef SCREENSWAP
				&& !(pressed & KEY_TOUCH)
#endif
				);

		if (pressed & KEY_UP)
			optionOffset--;
		if (pressed & KEY_DOWN)
			optionOffset++;

		if (optionOffset < 0) // Wrap around to bottom of list
			optionOffset = allowedOptions.size() - 1;

		if (optionOffset >= (int)allowedOptions.size()) // Wrap around to top of list
			optionOffset = 0;

		if (pressed & KEY_A)
			return allowedOptions[optionOffset];

		if (pressed & KEY_B)
			return DumpOption::none;

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
	}
}

void dumpFailMsg(std::string_view msg) {
	font->clear(false);
	font->print(0, 0, false, msg, Alignment::left, Palette::red);
	font->print(0, font->calcHeight(msg) + 1, false, STR_A_OK);
	font->update(false);

	u16 pressed;
	do {
		// Print time
		font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
		font->update(true);

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & KEY_A));
}

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
// and type 3 with ID 0xC22017 added
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
			
			switch(device) {

			case 0x2211:
				return 128*1024;		//	1Mbit(128KByte) - MX25L1021E
				break;
			case 0x2017:
				return 8*1024*1024;		//	64Mbit(8 meg)
				break;
			}
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

//---------------------------------------------------------------------------------
// https://github.com/devkitPro/libnds/blob/master/source/common/cardEeprom.c#L263
// but using our fixed size function
//---------------------------------------------------------------------------------
void cardEepromChipEraseFixed(void) {
//---------------------------------------------------------------------------------
	int sz, sector;
	sz=cardEepromGetSizeFixed();

	for ( sector = 0; sector < sz; sector+=0x10000) {
		cardEepromSectorErase(sector);
	}
}

u32 cardNandGetSaveSize(void) {
	switch(*(u32*)ndsCardHeader.gameCode & 0x00FFFFFF) {
		case 0x00425855: // 'UXB'
			return 8 << 20; // 8MByte - Jam with the Band
		case 0x00524F55: // 'UOR'
			return 16 << 20; // 16MByte - WarioWare D.I.Y.
		case 0x004B5355: // 'USK'
			return 64 << 20; // 64MByte - Face Training
	}

	return 0;
}

bool writeToGbaSave(const char* fileName, u8* buffer, u32 size) {
	font->clear(false);
	font->print(0, 0, false, STR_COMPRESSING_SAVE);
	font->update(false);
	int compressedSize = 0;
	u8 *compressedBuffer = LZS_Encode(buffer, size, LZS_VFAST, &compressedSize);

	u8 section = 0;
	u32 bytesWritten = 0;
	while((int)bytesWritten < compressedSize) {
		font->clear(false);
		font->print(0, 0, false, STR_LOADING);
		font->update(false);
		saveTypeGBA type = gbaGetSaveType();
		u32 gbaSize = gbaGetSaveSize(type);

		u32 writeSize = std::min(gbaSize - 0x30, (u32)(compressedSize - bytesWritten));

		font->clear(false);
		font->printf(0, 0, false, Alignment::left, Palette::white, (STR_WRITE_TO_GBA + "\n\n" + STR_A_YES_B_NO).c_str(), getBytes(writeSize).c_str(), getBytes(compressedSize - bytesWritten).c_str());
		font->update(false);

		u16 pressed;
		do {
			// Print time
			font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
			font->update(true);

			swiWaitForVBlank();
			scanKeys();
			pressed = keysDownRepeat();
		} while (!(pressed & (KEY_A | KEY_B)) && *(u8*)(0x080000B2) == 0x96);

		if(pressed & KEY_A) {
			font->clear(false);
			font->print(0, 0, false, STR_WRITING_SAVE);
			font->update(false);

			u8* writeBuffer = (u8*)memalign(4, gbaSize);
			// 0x30 byte header
			tonccpy(writeBuffer, "9i", 3); // Magic
			writeBuffer[3] = section; // Section of the save
			tonccpy(writeBuffer + 0x4, &size, 4); // Total original size
			tonccpy(writeBuffer + 0x8, &compressedSize, 4); // Total compressed size
			tonccpy(writeBuffer + 0xC, &writeSize, 4); // Size of current section (excluding header)
			tonccpy(writeBuffer + 0x10, fileName, 0x20); // File name
			// Actual save data
			tonccpy(writeBuffer + 0x30, compressedBuffer + bytesWritten, writeSize);

			gbaFormatSave(type);
			gbaWriteSave(0, writeBuffer, gbaSize, type);
			free(writeBuffer);

			bytesWritten += writeSize;
			section++;
		}

		if(pressed & KEY_B) {
			free(compressedBuffer);
			return false;
		}

		if((int)bytesWritten < compressedSize) {
			font->clear(false);
			font->print(0, 0, false, STR_SWITCH_CART);
			font->update(false);

			// Wait for GBA cart to be removed and reinserted
			if(*(u8*)(0x080000B2) == 0x96)
				while(*(u8*)(0x080000B2) == 0x96) swiWaitForVBlank();
			while(*(u8*)(0x080000B2) != 0x96) swiWaitForVBlank();
		}
	}

	free(compressedBuffer);

	return true;
}

bool readFromGbaCart() {
	u32 size, compressedSize;
	char fileName[0x20] = {0};
	u8 *compressedBuffer = nullptr;

	u8 currentSection = 0;
	u32 bytesRead = 0;
	do {
		font->clear(false);
		font->print(0, 0, false, STR_LOADING);
		font->update(false);

		saveTypeGBA saveType = gbaGetSaveType();
		u32 gbaSize = gbaGetSaveSize(saveType);
		u8 *buffer = new u8[gbaSize];
		gbaReadSave(buffer, 0, gbaSize, saveType);

		int section = -1;
		if(memcmp(buffer, "9i", 3) == 0) {
			// Only load the first time
			if(fileName[0] == 0) {
				tonccpy(&size, buffer + 0x4, 4); // Total original size
				tonccpy(&compressedSize, buffer + 0x8, 4); // Total compressed size
				tonccpy(fileName, buffer + 0x10, 0x20); // File name

				compressedBuffer = new u8[compressedSize];
			}

			u32 compressedSizeTemp;
			tonccpy(&compressedSizeTemp, buffer + 0x8, 4); // Total compressed size
			if(compressedSizeTemp == compressedSize) { // Probably matching DS dump
				section = buffer[3]; // Section of the save

				if(section == currentSection) {
					u32 readSize = 0;
					tonccpy(&readSize, buffer + 0xC, 4); // Size of current section (excluding header)

					// Copy to output buffer
					tonccpy(compressedBuffer + bytesRead, buffer + 0x30, readSize);

					bytesRead += readSize;
					currentSection++;
				}
			} else {
				dumpFailMsg(STR_WRONG_DS_SAVE);
			}
		} else {
			dumpFailMsg(STR_NO_DS_SAVE);
		}

		delete[] buffer;

		if(bytesRead < compressedSize) {
			font->clear(false);
			if(section != -1)
				font->printf(0, 0, false, Alignment::left, Palette::white, (STR_SWITCH_CART_TO_SECTION_THIS_WAS + "\n\n" + STR_B_CANCEL).c_str(), currentSection + 1, section + 1);
			else
				font->printf(0, 0, false, Alignment::left, Palette::white, (STR_SWITCH_CART_TO_SECTION + "\n\n" + STR_B_CANCEL).c_str(), currentSection + 1);
			font->update(false);

			if(*(u8*)(0x080000B2) == 0x96) {
				while(*(u8*)(0x080000B2) == 0x96) {
					// Print time
					font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
					font->update(true);

					swiWaitForVBlank();
					scanKeys();

					if(keysDown() & KEY_B) {
						delete[] compressedBuffer;
						return false;
					}
				}
			}
			while(*(u8*)(0x080000B2) != 0x96) {
				// Print time
				font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
				font->update(true);

				swiWaitForVBlank();
				scanKeys();

				if(keysDown() & KEY_B) {
					delete[] compressedBuffer;
					return false;
				}
			}
		} else {
			u8 *finalBuffer = new u8[size];
			decompress(compressedBuffer, finalBuffer, LZ77);

			char destPath[256];
			sprintf(destPath, "%s:/gm9i/out/%s.sav", (sdMounted ? "sd" : "fat"), fileName);
			FILE *destinationFile = fopen(destPath, "wb");
			if(destinationFile) {
				fwrite(finalBuffer, 1, size, destinationFile);
				fclose(destinationFile);
			}

			delete[] finalBuffer;
		}
	} while(bytesRead < compressedSize);

	delete[] compressedBuffer;

	return true;
}

void ndsCardSaveDump(const char* filename) {
	font->clear(false);
	font->print(0, 0, false, STR_DUMPING_SAVE);
	font->print(0, 1, false, STR_DO_NOT_REMOVE_CARD);
	font->update(false);

	int type = cardEepromGetTypeFixed();

	if(type == -1) { // NAND
		u32 saveSize = cardNandGetSaveSize();

		if(saveSize == 0) {
			dumpFailMsg(STR_FAILED_TO_DUMP_SAVE);
			return;
		}

		u32 currentSize = saveSize;
		FILE* destinationFile = fopen(filename, "wb");
		if (destinationFile) {

			font->print(0, 4, false, STR_PROGRESS);
			font->print(0, 5, false, "[");
			font->print(-1, 5, false, "]");
			for (u32 src = 0; src < saveSize; src += 0x8000) {
				// Print time
				font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
				font->update(true);

				font->print((src / (saveSize / (SCREEN_COLS - 2))) + 1, 5, false, "=");
				font->printf(0, 6, false, Alignment::left, Palette::white, STR_N_OF_N_BYTES.c_str(), src, saveSize);
				font->update(false);

				for (u32 i = 0; i < 0x8000; i += 0x200) {
					cardRead(cardNandRwStart + src + i, copyBuf + i, true);
				}
				if (fwrite(copyBuf, 1, (currentSize >= 0x8000 ? 0x8000 : currentSize), destinationFile) < 1) {
					dumpFailMsg(STR_FAILED_TO_DUMP_SAVE);
					break;
				}
				currentSize -= 0x8000;
			}
			fclose(destinationFile);
		} else {
			dumpFailMsg(STR_FAILED_TO_DUMP_SAVE);
		}
	} else { // SPI
		unsigned char *buffer;
		auxspi_extra card_type = auxspi_has_extra();
		int size = 0;
		if(card_type == AUXSPI_INFRARED) {
			int sizeLog2 = auxspi_save_size_log_2(card_type);
			int size_blocks;
			int type = auxspi_save_type(card_type);
			if(sizeLog2 < 16)
				size_blocks = 1;
			else
				size_blocks = 1 << (sizeLog2 - 16);
			u32 LEN = std::min(1 << sizeLog2, 1 << 16);
			size = LEN * size_blocks;
			buffer = new unsigned char[size];
			auxspi_read_data(0, buffer, size, type, card_type);
		} else {
			size = cardEepromGetSizeFixed();
			buffer = new unsigned char[size];
			cardReadEeprom(0, buffer, size, type);
		}
		if(sdMounted || flashcardMounted) {
			FILE *out = fopen(filename, "wb");
			if(out) {
				fwrite(buffer, 1, size, out);
			}
			fclose(out);
		} else {
			writeToGbaSave(filename, buffer, size);
		}
		delete[] buffer;
	}
}

void ndsCardSaveRestore(const char *filename) {
	font->clear(false);
	font->print(0, 0, false, STR_RESTORE_SELECTED_SAVE_CARD);
	font->update(false);

	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	u16 pressed;
	do {
		// Print time
		font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
		font->update(true);

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & (KEY_A | KEY_B)));

	if(pressed & KEY_A) {
		int type = cardEepromGetTypeFixed();

		if(type == -1) { // NAND
			if (cardInit(&ndsCardHeader) != 0) {
				font->clear(false);
				font->print(0, 0, false, STR_UNABLE_TO_RESTORE_SAVE);
				font->update(false);
				for (int i = 0; i < 60 * 2; i++) {
					swiWaitForVBlank();
				}
				return;
			}

			u32 saveSize = cardNandGetSaveSize();

			if(saveSize == 0) {
				dumpFailMsg(STR_UNABLE_TO_RESTORE_SAVE);
				return;
			}

			FILE* in = fopen(filename, "rb");

			fseek(in, 0, SEEK_END);
			size_t length = ftell(in);
			fseek(in, 0, SEEK_SET);
			if(length != saveSize) {
				fclose(in);

				dumpFailMsg(STR_SAVE_SIZE_MISMATCH_CARD);
				return;
			}

			u32 currentSize = saveSize;
			if (in) {
				font->print(0, 4, false, STR_PROGRESS);
				font->print(0, 5, false, "[");
				font->print(-1, 5, false, "]");
				for (u32 dest = 0; dest < saveSize; dest += 0x8000) {
					// Print time
					font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
					font->update(true);

					font->print((dest / (saveSize / (SCREEN_COLS - 2))) + 1, 5, false, "=");
					font->printf(0, 6, false, Alignment::left, Palette::white, STR_N_OF_N_BYTES.c_str(), dest, saveSize);
					font->update(false);

					fread(copyBuf, 1, 0x8000, in);
					for (u32 i = 0; i < 0x8000; i += 0x800) {
						cardWriteNand(copyBuf + i, cardNandRwStart + dest + i);
					}
					currentSize -= 0x8000;
				}
				fclose(in);
			}
		} else { // SPI
			auxspi_extra card_type = auxspi_has_extra();
			bool auxspi = card_type == AUXSPI_INFRARED;
			FILE *in = fopen(filename, "rb");
			if(in) {
				unsigned char *buffer;
				int size;
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
					size = cardEepromGetSizeFixed();
				}
				fseek(in, 0, SEEK_END);
				length = ftell(in);
				fseek(in, 0, SEEK_SET);
				if(length != (auxspi ? (int)(LEN * num_blocks) : size)) {
					fclose(in);
					dumpFailMsg(STR_SAVE_SIZE_MISMATCH_CARD);
					return;
				}

				font->clear(false);
				font->print(0, 0, false, STR_RESTORING_SAVE);
				font->print(0, 1, false, STR_DO_NOT_REMOVE_CARD);
				font->print(0, 4, false, STR_PROGRESS);
				font->update(false);

				if(type == 3) {
					if(auxspi)
						auxspi_erase(card_type);
					else
						cardEepromChipEraseFixed();
				}
				if(auxspi){
					buffer = new unsigned char[LEN];
					font->print(0, 5, false, "[");
					font->print(-1, 5, false, "]");
					for(unsigned int i = 0; i < num_blocks; i++) {
						font->print((i * (SCREEN_COLS - 2) / num_blocks) + 1, 5, false, "=");
						font->printf(0, 6, false, Alignment::left, Palette::white, STR_N_OF_N_BYTES.c_str(), i * LEN, length);
						font->update(false);

						fread(buffer, 1, LEN, in);
						auxspi_write_data(i << shift, buffer, LEN, type, card_type);
					}
				} else {
					int blocks = size / 32;
					int written = 0;
					buffer = new unsigned char[blocks];
					font->print(0, 5, false, "[");
					font->print(-1, 5, false, "]");
					for(unsigned int i = 0; i < 32; i++) {
						font->print((i * (SCREEN_COLS - 2) / 32) + 1, 5, false, "=");
						font->printf(0, 6, false, Alignment::left, Palette::white, STR_N_OF_N_BYTES.c_str(), written, size);
						font->update(false);

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
}

void ndsCardDump(void) {
	u16 pressed;

	font->clear(false);
	if ((io_dldi_data->ioInterface.features & FEATURE_SLOT_NDS) && flashcardMounted) {
		font->print(0, 0, false, STR_FLASHCARD_WILL_UNMOUNT);
		font->print(0, 3, false, STR_A_YES_B_NO);
		font->update(false);

		while (true) {
			// Print time
			font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
			font->update(true);

			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
			if (pressed & KEY_A) {
				font->clear(false);
				flashcardUnmount();
				break;
			}
			if (pressed & KEY_B) {
				return;
			}
		}
	}

	font->print(0, 0, false, STR_LOADING);
	font->update(false);

	std::vector allowedOptions = {DumpOption::all};
	u8 allowedBitfield = 0;
	char gameTitle[13] = {0};
	char gameCode[7] = {0};
	char fileName[32] = {0};
	bool spiSave = cardEepromGetTypeFixed() != -1;
	bool nandSave = false;

	int cardInited = cardInit(&ndsCardHeader);
	if(cardInited == 0) {
		if(sdMounted || flashcardMounted) {
			allowedOptions.push_back(DumpOption::allTrimmed);
			allowedOptions.push_back(DumpOption::rom);
			allowedOptions.push_back(DumpOption::romTrimmed);
			allowedBitfield |= DumpOption::rom | DumpOption::romTrimmed;

			nandSave = cardNandGetSaveSize() != 0;
		}

		if((spiSave && (sdMounted || flashcardMounted || cardEepromGetSizeFixed() <= (1 << 20))) || (nandSave && (sdMounted || flashcardMounted))) {
			allowedOptions.push_back(DumpOption::save);
			allowedBitfield |= DumpOption::save;
		}
	}
	if(sdMounted || flashcardMounted) {
		allowedBitfield |= DumpOption::metadata;
		allowedOptions.push_back(DumpOption::metadata);
	}

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
	sprintf(fileName, "%s_%s_%02X", gameTitle, gameCode, ndsCardHeader.romversion);

	DumpOption dumpOption = dumpMenu(allowedOptions, fileName);

	if(dumpOption & DumpOption::romTrimmed)
		strcat(fileName, "_trim");

	// Ensure directories exist
	if((dumpOption & allowedBitfield) != DumpOption::none && (sdMounted || flashcardMounted)) {
		char folderPath[2][256];
		sprintf(folderPath[0], "%s:/gm9i", (sdMounted ? "sd" : "fat"));
		sprintf(folderPath[1], "%s:/gm9i/out", (sdMounted ? "sd" : "fat"));
		if (access(folderPath[0], F_OK) != 0) {
			font->clear(false);
			font->print(0, 0, false, STR_CREATING_DIRECTORY);
			font->update(false);
			mkdir(folderPath[0], 0777);
		}
		if (access(folderPath[1], F_OK) != 0) {
			font->clear(false);
			font->print(0, 0, false, STR_CREATING_DIRECTORY);
			font->update(false);
			mkdir(folderPath[1], 0777);
		}
	}

	// Dump ROM
	if((dumpOption & allowedBitfield) & (DumpOption::rom | DumpOption::romTrimmed)) {
		font->clear(false);
		font->printf(0, 0, false, Alignment::left, Palette::white, STR_NDS_IS_DUMPING.c_str(), fileName);
		font->print(0, 2, false, STR_DO_NOT_REMOVE_CARD);
		font->update(false);

		// Determine ROM size
		u32 romSize;
		if (dumpOption & DumpOption::romTrimmed) {
			romSize = (isDSiMode() && (ndsCardHeader.unitCode != 0) && (ndsCardHeader.twlRomSize > 0))
						? ndsCardHeader.twlRomSize : ndsCardHeader.romSize+0x88;
		} else {
			romSize = 0x20000 << ndsCardHeader.deviceSize;
		}

		// Dump!
		char destPath[256];
		sprintf(destPath, "%s:/gm9i/out/%s.nds", (sdMounted ? "sd" : "fat"), fileName);
		u32 currentSize = romSize;
		FILE* destinationFile = fopen(destPath, "wb");
		if (destinationFile) {
			font->print(0, 4, false, STR_PROGRESS);
			font->print(0, 5, false, "[");
			font->print(-1, 5, false, "]");
			for (u32 src = 0; src < romSize; src += 0x8000) {
				// Print time
				font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
				font->update(true);

				font->print((src / (romSize / (SCREEN_COLS - 2))) + 1, 5, false, "=");
				font->printf(0, 6, false, Alignment::left, Palette::white, STR_N_OF_N_BYTES.c_str(), src, romSize);
				font->update(false);

				for (u32 i = 0; i < 0x8000; i += 0x200) {
					cardRead (src+i, copyBuf+i, false);
				}
				if (fwrite(copyBuf, 1, (currentSize>=0x8000 ? 0x8000 : currentSize), destinationFile) < 1) {
					dumpFailMsg(STR_FAILED_TO_DUMP_ROM);
					break;
				}
				currentSize -= 0x8000;
			}
			fclose(destinationFile);
		} else {
			dumpFailMsg(STR_FAILED_TO_DUMP_ROM);
		}
	}

	// Dump save
	if ((dumpOption & allowedBitfield) & DumpOption::save) {
		char destPath[256];
		sprintf(destPath, "%s:/gm9i/out/%s.sav", (sdMounted ? "sd" : "fat"), fileName);
		ndsCardSaveDump((sdMounted || flashcardMounted) ? destPath : fileName);
	}

	// Dump metadata
	if ((dumpOption & allowedBitfield) & DumpOption::metadata) {
		font->clear(false);
		font->print(0, 0, false, STR_DUMPING_METADATA);
		font->update(false);

		char destPath[256];
		sprintf(destPath, "%s:/gm9i/out/%s.txt", (sdMounted ? "sd" : "fat"), fileName);
		FILE* destinationFile = fopen(destPath, "wb");
		if (destinationFile) {
			fprintf(destinationFile,
				"Title String : %.12s\n"
				"Product Code : %.6s\n"
				"Revision     : %u\n"
				"Cart ID      : %08lX\n"
				"Platform     : %s\n"
				"Save Type    : %s\n",
				gameTitle, gameCode, ndsCardHeader.romversion, cardGetId(),
				(ndsCardHeader.unitCode == 0x2) ? "DSi Enhanced" : (ndsCardHeader.unitCode == 0x3) ? "DSi Exclusive" : "DS",
				spiSave ? "SPI" : (nandSave ? "RETAIL_NAND" : "NONE"));

			if(spiSave)
				fprintf(destinationFile, "Save chip ID : 0x%06lX\n", cardEepromReadID());

			fprintf(destinationFile,
				"Timestamp    : %s\n"
				"GM9i Version : " VER_NUMBER "\n",
				RetTime("%Y-%m-%d %H:%M:%S").c_str());

			fclose(destinationFile);
		}
	}
}


void gbaCartSaveDump(const char *filename) {
	font->clear(false);
	font->print(0, 0, false, STR_DUMPING_SAVE);
	font->print(0, 1, false, STR_DO_NOT_REMOVE_CART);
	font->update(false);

	saveTypeGBA type = gbaGetSaveType();
	u32 size = gbaGetSaveSize(type);
	if(size == 0)
		return;

	u8 *buffer = new u8[size];
	gbaReadSave(buffer, 0, size, type);

	FILE *destinationFile = fopen(filename, "wb");
	fwrite(buffer, 1, size, destinationFile);
	fclose(destinationFile);
	delete[] buffer;
}

void gbaCartSaveRestore(const char *filename) {
	font->clear(false);
	font->print(0, 0, false, STR_RESTORE_SELECTED_SAVE_CART);
	font->update(false);

	// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
	u16 pressed;
	do {
		// Print time
		font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
		font->update(true);

		scanKeys();
		pressed = keysDownRepeat();
		swiWaitForVBlank();
	} while (!(pressed & (KEY_A | KEY_B)));

	if (pressed & KEY_A) {
		saveTypeGBA type = gbaGetSaveType();
		u32 size = gbaGetSaveSize(type);
		if(size == 0)
			return;

		FILE *sourceFile = fopen(filename, "rb");
		if(!sourceFile) {
			dumpFailMsg(STR_FAILED_TO_RESTORE_SAVE);
			return;
		}

		fseek(sourceFile, 0, SEEK_END);
		size_t length = ftell(sourceFile);
		fseek(sourceFile, 0, SEEK_SET);
		if(length != size) {
			fclose(sourceFile);

			dumpFailMsg(STR_SAVE_SIZE_MISMATCH_CART);
			return;
		}

		u8 *buffer = new u8[size];
		if (fread(buffer, 1, size, sourceFile) != size) {
			delete[] buffer;
			fclose(sourceFile);

			dumpFailMsg(STR_FAILED_TO_RESTORE_SAVE);
			return;
		}

		font->clear(false);
		font->print(0, 0, false, STR_RESTORING_SAVE);
		font->print(0, 1, false, STR_DO_NOT_REMOVE_CART);
		font->update(false);

		gbaFormatSave(type);
		gbaWriteSave(0, buffer, size, type);

		delete[] buffer;
		fclose(sourceFile);
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
	font->clear(false);
	font->print(0, 0, false, STR_LOADING);
	font->update(false);

	std::vector allowedOptions = {DumpOption::all, DumpOption::rom};
	u8 allowedBitfield = DumpOption::rom | DumpOption::metadata;
	char gameTitle[13] = {0};
	char gameCode[7] = {0};
	char fileName[32] = {0};
	saveTypeGBA saveType = gbaGetSaveType();

	if(saveType != saveTypeGBA::SAVE_GBA_NONE) {
		allowedOptions.push_back(DumpOption::save);
		allowedBitfield |= DumpOption::save;

		u32 size = gbaGetSaveSize(saveType);
		u8 *buffer = new u8[size];
		gbaReadSave(buffer, 0, size, saveType);
		if(memcmp(buffer, "9i", 3) == 0) {
			allowedOptions.push_back(DumpOption::ndsSave);
			allowedBitfield |= DumpOption::ndsSave;
		}
		delete[] buffer;
	}
	allowedOptions.push_back(DumpOption::metadata);

	// Get name
	tonccpy(gameTitle, (u8*)(0x080000A0), 12);
	tonccpy(gameCode, (u8*)(0x080000AC), 6);
	if (gameTitle[0] == 0 || gameTitle[0] == 0xFF) {
		sprintf(gameTitle, "NO-TITLE");
	} else {
		for(uint i = 0; i < sizeof(gameTitle); i++) {
			switch(gameTitle[i]) {
				case '>':
				case '<':
				case ':':
				case '"':
				case '/':
				case '\\':
				case '|':
				case '?':
				case '*':
					gameTitle[i] = '_';
			}
		}
	}
	if (gameCode[0] == 0 || gameCode[0] == 0xFF) {
		sprintf(gameCode, "NONE00");
	}
	u8 romVersion = *(u8*)(0x080000BC);
	sprintf(fileName, "%s_%s_%02X", gameTitle, gameCode, romVersion);

	DumpOption dumpOption = dumpMenu(allowedOptions, fileName);

	// Ensure directories exist
	if((dumpOption & allowedBitfield) != DumpOption::none) {
		char folderPath[2][256];
		sprintf(folderPath[0], "%s:/gm9i", (sdMounted ? "sd" : "fat"));
		sprintf(folderPath[1], "%s:/gm9i/out", (sdMounted ? "sd" : "fat"));
		if (access(folderPath[0], F_OK) != 0) {
			font->clear(false);
			font->print(0, 0, false, STR_CREATING_DIRECTORY);
			font->update(false);
			mkdir(folderPath[0], 0777);
		}
		if (access(folderPath[1], F_OK) != 0) {
			font->clear(false);
			font->print(0, 0, false, STR_CREATING_DIRECTORY);
			font->update(false);
			mkdir(folderPath[1], 0777);
		}
	}

	// Dump ROM
	if ((dumpOption & allowedBitfield) & DumpOption::rom) {
		font->clear(false);
		font->printf(0, 0, false, Alignment::left, Palette::white, STR_GBA_IS_DUMPING.c_str(), fileName);
		font->print(0, 2, false, STR_DO_NOT_REMOVE_CART);
		font->update(false);

		// Determine ROM size
		u32 romSize;
		for (romSize = (1 << 20); romSize < (1 << 25); romSize <<= 1) {
			vu16 *rompos = (vu16*)(0x08000000 + romSize);
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

		// Dump!
		// Reset data at virtual address
		u32 rstCmd[4] = {
			0x11, // Command
			0x1000, // ROM address
			0x08001000, // Virtual address
			0x8, // Size (in 0x200 byte blocks)
		};
		writeChange(rstCmd);

		char destPath[256];
		sprintf(destPath, "fat:/gm9i/out/%s.gba", fileName);
		FILE* destinationFile = fopen(destPath, "wb");
		if (destinationFile) {
			bool failed = false;

			font->print(0, 4, false, STR_PROGRESS);
			font->print(0, 5, false, "[");
			font->print(-1, 5, false, "]");
			for (u32 src = 0; src < romSize; src += 0x8000) {
				// Print time
				font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
				font->update(true);

				font->print((src / (romSize / (SCREEN_COLS - 2))) + 1, 5, false, "=");
				font->printf(0, 6, false, Alignment::left, Palette::white, STR_N_OF_N_BYTES.c_str(), src, romSize);
				font->update(false);

				if (fwrite(GBAROM + src / sizeof(u16), 1, 0x8000, destinationFile) != 0x8000) {
					dumpFailMsg(STR_FAILED_TO_DUMP_ROM);
					failed = true;
					break;
				}
			}

			// Check for 64MB GBA Video ROM
			if ((strncmp((char*)0x080000AC, "MSAE", 4) == 0 // Shark Tale
			|| strncmp((char*)0x080000AC, "MSKE", 4) == 0   // Shrek
			|| strncmp((char*)0x080000AC, "MSTE", 4) == 0   // Shrek & Shark Tale
			|| strncmp((char*)0x080000AC, "M2SE", 4) == 0   // Shrek 2
			) && !failed) {
				// Dump last 32MB
				u32 cmd[4] = {
					0x11, // Command
					0, // ROM address
					0x08001000, // Virtual address
					0x8, // Size (in 0x200 byte blocks)
				};

				font->print(0, 5, false, "[");
				font->print(-1, 5, false, "]");
				for (size_t i = 0x02000000; i < 0x04000000; i += 0x1000) {
					// Print time
					font->print(-1, 0, true, RetTime(), Alignment::right, Palette::blackGreen);
					font->update(true);

					font->print((i / (0x04000000 / (SCREEN_COLS - 2))) + 1, 5, false, "=");
					font->printf(0, 7, false, Alignment::left, Palette::white, STR_N_OF_N_BYTES.c_str(), i - 0x02000000, 0x04000000 - 0x02000000);
					font->update(false);

					cmd[1] = i,
					writeChange(cmd);
					readChange();
					if (fwrite(GBAROM + (0x1000 >> 1), 0x1000, 1, destinationFile) < 1) {
						dumpFailMsg(STR_FAILED_TO_DUMP_ROM);
						break;
					}
				}
			}
			fclose(destinationFile);
		} else {
			dumpFailMsg(STR_FAILED_TO_DUMP_ROM);
			return;
		}
	}

	// Dump save
	if((dumpOption & allowedBitfield) & DumpOption::save) {
		char destPath[256];
		sprintf(destPath, "fat:/gm9i/out/%s.sav", fileName);
		gbaCartSaveDump(destPath);
	}

	// Dump NDS save previously saved to this cart
	if ((dumpOption & allowedBitfield) & DumpOption::ndsSave) {
		readFromGbaCart();
	}

	// Dump metadata
	if ((dumpOption & allowedBitfield) & DumpOption::metadata) {
		font->clear(false);
		font->print(0, 0, false, STR_DUMPING_METADATA);
		font->update(false);

		char destPath[256];
		sprintf(destPath, "%s:/gm9i/out/%s.txt", (sdMounted ? "sd" : "fat"), fileName);
		FILE* destinationFile = fopen(destPath, "wb");
		if (destinationFile) {
			fprintf(destinationFile,
				"Title String : %.12s\n"
				"Product Code : %.6s\n"
				"Revision     : %u\n"
				"Platform     : GBA\n",
				gameTitle, gameCode, romVersion);

			fprintf(destinationFile,
				"Save Type    : %s\n",
				saveType == SAVE_GBA_NONE ? "NONE" :
				saveType == SAVE_GBA_EEPROM_05 ? "EEPROM 4K" :
				saveType == SAVE_GBA_EEPROM_8 ? "EEPROM 64K" :
				saveType == SAVE_GBA_SRAM_32 ? "SRAM" :
				saveType == SAVE_GBA_FLASH_64 ? "FLASH 512K" :
				saveType == SAVE_GBA_FLASH_128 ? "FLASH 1M" : "UNK");

			if(saveType == SAVE_GBA_FLASH_64 || saveType == SAVE_GBA_FLASH_128)
				fprintf(destinationFile, "Save chip ID : 0x%04X\n", gbaGetFlashId());

			fprintf(destinationFile,
				"Timestamp    : %s\n"
				"GM9i Version : " VER_NUMBER "\n",
				RetTime("%Y-%m-%d %H:%M:%S").c_str());

			fclose(destinationFile);
		}
	}
}

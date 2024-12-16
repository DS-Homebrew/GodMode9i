#include "hexEditor.h"

#include "file_browse.h"
#include "font.h"
#include "keyboard.h"
#include "language.h"
#include "my_sd.h"
#include "screenshot.h"
#include "tonccpy.h"

#include <algorithm>
#include <nds.h>
#include <stdio.h>

u32 jumpToOffset(u32 offset) {
	u8 cursorPosition = 0;
	u16 pressed = 0, held = 0;
	while(pmMainLoop()) {
		int y = (ENTRIES_PER_SCREEN - 4) / 2;
		font->clear(false);
		font->print(0, y, false, "--------------------", Alignment::center);
		font->print(0, y + 1, false, STR_JUMP_TO_OFFSET, Alignment::center);
		font->printf(0, y + 3, false, Alignment::center, Palette::blue, "%08lX", offset);
		font->printf(3 - cursorPosition, y + 3, false, Alignment::center, Palette::red, "%lX", (offset >> ((cursorPosition + 1) * 4)) & 0xF);
		font->print(0, y + 4, false, "--------------------", Alignment::center);
		font->update(false);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
		} while(pmMainLoop() && !held);

		if(held & KEY_UP) {
			offset = (offset & ~(0xF0 << cursorPosition * 4)) | ((offset + (0x10 << (cursorPosition * 4))) & (0xF0 << cursorPosition * 4));
		} else if(held & KEY_DOWN) {
			offset = (offset & ~(0xF0 << cursorPosition * 4)) | ((offset - (0x10 << (cursorPosition * 4))) & (0xF0 << cursorPosition * 4));
		} else if(held & KEY_LEFT) {
			if(cursorPosition < 6)
				cursorPosition++;
		} else if(held & KEY_RIGHT) {
			if(cursorPosition > 0)
				cursorPosition--;
		} else if(pressed & (KEY_A | KEY_B)) {
			return offset;
		} else if(keysHeld() & KEY_R && pressed & KEY_L) {
			screenshot();
		}
	}
}

u32 search(u32 offset, FILE *file) {
	u8 cursorPosition = 0;
	u16 pressed = 0, held = 0;
	while(pmMainLoop()) {
		int y = (ENTRIES_PER_SCREEN - 3) / 2;
		font->clear(false);
		font->print(0, y, false, "--------------------", Alignment::center);
		font->printf(0, y + 1, false, Alignment::center, Palette::white, "%c %s %c", cursorPosition == 0 ? '>' : ' ', STR_SEARCH_STRING.c_str(), cursorPosition == 0 ? '<' : ' ');
		font->printf(0, y + 2, false, Alignment::center, Palette::white, "%c %s %c", cursorPosition == 1 ? '>' : ' ', STR_SEARCH_DATA.c_str(), cursorPosition == 1 ? '<' : ' ');
		font->print(0, y + 3, false, "--------------------", Alignment::center);
		font->update(false);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
		} while(pmMainLoop() && !held);

		if(held & (KEY_UP | KEY_DOWN)) {
			cursorPosition ^= 1;
		} else if(pressed & KEY_A) {
			break;
		} else if(pressed & KEY_B) {
			return offset;
		} else if(keysHeld() & KEY_R && pressed & KEY_L) {
			screenshot();
		}
	}

	char str[64] = {0};
	size_t strLen = 1;

	if(cursorPosition == 0) {
		strcpy(str, kbdGetString(STR_SEARCH_FOR, sizeof(str)).c_str());

		strLen = strlen(str);
		if(strLen == 0)
			return offset;
	} else {
		cursorPosition = 0;
		while(pmMainLoop()) {
			int y = (ENTRIES_PER_SCREEN - 4) / 2;
			font->clear(false);
			font->print(0, y, false, "--------------------", Alignment::center);
			font->print(0, y + 1, false, STR_ENTER_VALUE, Alignment::center);
			for(size_t i = 0; i < strLen * 2; i++)
				font->printf(-strLen + i + 1, y + 3, false, Alignment::center, i == cursorPosition ? Palette::red : ((i / 2 % 2) ? Palette::greenAlt : Palette::green), "%X", str[i / 2] >> (!(i % 2) * 4) & 0xF);
			font->print(0, y + 4, false, "--------------------", Alignment::center);
			font->update(false);

			do {
				swiWaitForVBlank();
				scanKeys();
				pressed = keysDown();
				held = keysDownRepeat();
			} while(pmMainLoop() && !held);

			if(held & KEY_UP) {
				char val = str[cursorPosition / 2];
				u8 shift = !(cursorPosition % 2) * 4;
				str[cursorPosition / 2] = (val & (0xF0 >> shift)) | ((val + (1 << shift)) & (0xF << shift));
			} else if(held & KEY_DOWN) {
				char val = str[cursorPosition / 2];
				u8 shift = !(cursorPosition % 2) * 4;
				str[cursorPosition / 2] = (val & (0xF0 >> shift)) | ((val - (1 << shift)) & (0xF << shift));
			} else if(held & KEY_LEFT) {
				if(cursorPosition > 0)
					cursorPosition--;
			} else if(held & KEY_RIGHT) {
				if(cursorPosition < strLen * 2 - 1) {
					cursorPosition++;
				} else if(strLen < 8) {
					strLen++;
					cursorPosition++;
				}
			} else if(pressed & KEY_A) {
				break;
			} else if(pressed & KEY_B) {
				return offset;
			} else if(pressed & KEY_X) {
				if(strLen > 1) {
					str[strLen - 1] = 0;
					strLen--;
					if(cursorPosition > strLen * 2 - 1)
						cursorPosition -= 2;
				}
			} else if(keysHeld() & KEY_R && pressed & KEY_L) {
				screenshot();
			}
		}
	}

	int y = (ENTRIES_PER_SCREEN - 7) / 2;
	font->clear(false);
	font->print(0, y, false, "--------------------", Alignment::center);
	font->print(0, y + 1, false, STR_SEARCHING, Alignment::center);
	font->print(0, y + 6, false, STR_PRESS_B_TO_CANCEL, Alignment::center);
	font->print(0, y + 7, false, "--------------------", Alignment::center);
	font->update(false);

	char progressBar[21] = "[                  ]";

	size_t len = 32 << 10, pos = offset;
	fseek(file, 0, SEEK_END);
	size_t fileLen = ftell(file);
	char *buf = new char[len];
	do {
		scanKeys();
		if(keysDown() & KEY_B) {
			delete[] buf;
			return offset;
		}

		progressBar[pos / (fileLen / 18) + 1] = '=';
		font->print(0, y + 3, false, progressBar, Alignment::center);
		font->printf(0, y + 4, false, Alignment::center, Palette::white, "%d/%d", pos, fileLen);
		font->update(false);

		if(fseek(file, pos, SEEK_SET) != 0)
			break;
		len = fread(buf, 1, len, file);

		for(size_t i = 0; i < len - strLen && len >= strLen; i++) {
			if(memcmp(buf + i, str, strLen) == 0) {
				delete[] buf;
				return (pos + i) & ~7;
			}
		}

		pos += len;
	} while(len == 32 << 10);
	delete[] buf;

	y = (ENTRIES_PER_SCREEN - 3) / 2;
	font->clear(false);
	font->print(0, y, false, "--------------------", Alignment::center);
	font->print(0, y + 1, false, STR_EOF_NO_RESULTS, Alignment::center);
	font->print(0, y + 3, false, "--------------------", Alignment::center);
	font->update(false);

	do {
		swiWaitForVBlank();
		scanKeys();
	} while(pmMainLoop() && !keysDown());

	return offset;
}

void hexEditor(const char *path, Drive drive) {
	FILE *file = fopen(path, driveWritable(drive) ? "rb+" : "rb");

	if(!file)
		return;

	fseek(file, 0, SEEK_END);
	u32 fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	u8 bytesPerLine = font->width() < 5 ? 16 : 8;

	u8 maxLines = std::min((u32)ENTRIES_PER_SCREEN, fileSize / bytesPerLine + (fileSize % bytesPerLine != 0));
	u32 maxSize = ((fileSize - bytesPerLine * maxLines) & ~(bytesPerLine - 1)) + (fileSize & (bytesPerLine - 1) ? bytesPerLine : 0);

	u16 pressed = 0, held = 0;
	u32 offset = 0, cursorPosition = 0, mode = 0;

	char data[bytesPerLine * maxLines];
	fseek(file, offset, SEEK_SET);
	fread(data, 1, sizeof(data), file);

	while(pmMainLoop()) {
		font->clear(false);

		font->printf(0, 0, false, Alignment::left, Palette::blackGreen, "%*c", SCREEN_COLS, ' ');
		font->print(0, 0, false, STR_HEX_EDITOR, Alignment::center, Palette::blackGreen);

		if(bytesPerLine < 16)
			font->printf(0, 0, false, Alignment::left, Palette::blackBlue, "%04lX", offset >> 0x10);

		if(mode < 2) {
			fseek(file, offset, SEEK_SET);
			toncset(data, 0, sizeof(data));
			fread(data, 1, std::min((u32)sizeof(data), fileSize - offset), file);
		}

		for(u32 i = 0; i < maxLines; i++) {
			if(bytesPerLine < 16)
				font->printf(0, i + 1, false, Alignment::left, Palette::blue, "%04lX", (offset + i * bytesPerLine) & 0xFFFF);
			else
				font->printf(0, i + 1, false, Alignment::left, Palette::blue, "%08lX", offset + i * bytesPerLine);

			for(int group = 0; group < bytesPerLine / 4; group++) {
				for(int j = 0; j < 4; j++)
					font->printf(4 * (bytesPerLine / 8) + 1 + (group * 9) + (j * 2), i + 1, false, Alignment::left, (mode > 0 && i * bytesPerLine + (group * 4) + j == cursorPosition) ? (mode > 1 ? Palette::blackRed : Palette::red) : (offset + i * bytesPerLine + (group * 4) + j >= fileSize ? Palette::gray : (j % 2 ? Palette::greenAlt : Palette::green)), "%02X", data[i * bytesPerLine + group * 4 + j]);
			}
			char line[bytesPerLine + 1] = {0};
			for(int j = 0; j < bytesPerLine; j++) {
				char c = data[i * bytesPerLine + j];
				if(c < ' ' || c > 127)
					line[j] = '.';
				else
					line[j] = c;
			}
			font->print(4 * (bytesPerLine / 8) + 1 + bytesPerLine / 4 * 9, i + 1, false, line);
			if(mode > 0 && cursorPosition / bytesPerLine == i) {
				font->printf(4 * (bytesPerLine / 8) + 1 + bytesPerLine / 4 * 9 + cursorPosition % bytesPerLine, i + 1, false, Alignment::left, mode > 1 ? Palette::blackRed : Palette::red, "%c", line[cursorPosition % bytesPerLine]);
			}
		}

		font->update(false);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();

			if(driveRemoved(currentDrive))
				return;
		} while(pmMainLoop() && !held);

		if(mode == 0) {
			if(keysHeld() & KEY_R && held & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
				if(held & KEY_UP) {
					offset = std::max((s64)offset - 0x1000, 0ll);
				} else if(held & KEY_DOWN) {
					offset = std::min(offset + 0x1000, maxSize);
				} else if(held & KEY_LEFT) {
					offset = std::max((s64)offset - 0x10000, 0ll);
				} else if(held & KEY_RIGHT) {
					offset = std::min(offset + 0x10000, maxSize);
				}
			} else if(held & KEY_UP) {
				if(offset >= bytesPerLine)
					offset -= bytesPerLine;
			} else if(held & KEY_DOWN) {
				if(offset < fileSize - bytesPerLine * maxLines && fileSize > bytesPerLine * maxLines)
					offset += bytesPerLine;
			} else if(held & KEY_LEFT) {
				offset = std::max((s64)offset - bytesPerLine * maxLines, 0ll);
			} else if(held & KEY_RIGHT) {
				offset = std::min(offset + bytesPerLine * maxLines, maxSize);
			} else if(pressed & KEY_A) {
				mode = 1;
				cursorPosition = std::min(cursorPosition, fileSize - offset - 1);
			} else if(pressed & KEY_B) {
				break;
			} else if(pressed & KEY_X) {
				offset = std::min(search(offset, file), maxSize);
			} else if(pressed & KEY_Y) {
				offset = std::min(jumpToOffset(offset), maxSize);
			}
		} else if(mode == 1) {
			if(held & KEY_UP) {
				if(cursorPosition >= bytesPerLine)
					cursorPosition -= bytesPerLine;
				else if(offset >= bytesPerLine)
					offset -= bytesPerLine;
			} else if(held & KEY_DOWN) {
				if((int)cursorPosition < bytesPerLine * (maxLines - 1))
					cursorPosition += bytesPerLine;
				else if(offset < fileSize - bytesPerLine * maxLines && fileSize > bytesPerLine * maxLines)
					offset += bytesPerLine;
				cursorPosition = std::min(cursorPosition, fileSize - offset - 1);
			} else if(held & KEY_LEFT) {
				if(cursorPosition > 0)
					cursorPosition--;
			} else if(held & KEY_RIGHT) {
				if((int)cursorPosition < bytesPerLine * maxLines - 1)
					cursorPosition = std::min(cursorPosition + 1, fileSize - offset - 1);
			} else if(pressed & KEY_A) {
				if(driveWritable(drive)) {
					mode = 2;
				}
			} else if(pressed & KEY_B) {
				mode = 0;
			} else if(pressed & KEY_X) {
				offset = std::min(search(offset, file), maxSize);
			} else if(pressed & KEY_Y) {
				offset = std::min(jumpToOffset(offset), maxSize);
			}
		} else if(mode == 2) {
			if(held & KEY_UP) {
				data[cursorPosition]++;
			} else if(held & KEY_DOWN) {
				data[cursorPosition]--;
			} else if(held & KEY_LEFT) {
				data[cursorPosition] -= 0x10;
			} else if(held & KEY_RIGHT) {
				data[cursorPosition] += 0x10;
			} else if(pressed & (KEY_A | KEY_B)) {
				mode = 1;
				fseek(file, offset + cursorPosition, SEEK_SET);
				fwrite(data + cursorPosition, 1, 1, file);
			}
		}

		if(keysHeld() & KEY_R && pressed & KEY_L) {
			screenshot();
		}
	}

	fclose(file);
}

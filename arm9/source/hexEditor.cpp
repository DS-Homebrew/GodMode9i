#include "hexEditor.h"

#include "tonccpy.h"

#include <nds.h>
#include <stdio.h>


u32 jumpToOffset(u32 offset) {
	consoleClear();

	u8 cursorPosition = 0;
	u16 pressed = 0, held = 0;
	while(1) {
		printf("\x1B[9;6H\x1B[47m-------------------");
		printf("\x1B[10;8H\x1B[47mJump to Offset");
		printf("\x1B[12;11H\x1B[33m%08lx", offset);
		printf("\x1B[12;%dH\x1B[43m%lx", 17 - cursorPosition, (offset >> ((cursorPosition + 1) * 4)) & 0xF);
		printf("\x1B[13;6H\x1B[47m-------------------");

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
		} while(!held);

		if(held & KEY_UP) {
			offset += 0x10 << (cursorPosition * 4);
		} else if(held & KEY_DOWN) {
			offset -= 0x10 << (cursorPosition * 4);
		} else if(held & KEY_LEFT) {
			if(cursorPosition < 6)
				cursorPosition++;
		} else if(held & KEY_RIGHT) {
			if(cursorPosition > 0)
				cursorPosition--;
		} else if(pressed & (KEY_A | KEY_B)) {
			return offset;
		}
	}
}

void hexEditor(const char *path) {
	FILE *file = fopen(path, "rb+");

	if(!file)
		return;

	consoleClear();

	fseek(file, 0, SEEK_END);
	u32 fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	u8 cursorPosition = 0, mode = 0;
	u16 pressed = 0, held = 0;
	u32 offset = 0;

	char data[8 * 23];
	fseek(file, offset, SEEK_SET);
	fread(data, 1, sizeof(data), file);

	while(1) {
		printf("\x1B[0;11H\x1B[42mHex Editor");
		printf("\x1B[0;0H\x1B[37m%04lx", offset >> 0x10);

		if(mode < 2 && held & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
			fseek(file, offset, SEEK_SET);
			fread(data, 1, sizeof(data), file);
		}

		for(int i = 0; i < 23; i++) {
			printf("\x1B[%d;0H\x1B[37m%04lx", i + 1, (offset + i * 8) & 0xFFFF);
			for(int j = 0; j < 4; j++)
				printf("\x1B[%d;%dH\x1B[%dm%02x", i + 1, 5 + (j * 2), (mode > 0 && i * 8 + j == cursorPosition) ? (mode > 1 ? 41 : 31) : 33, data[i * 8 + j]);
			for(int j = 0; j < 4; j++)
				printf("\x1B[%d;%dH\x1B[%dm%02x", i + 1, 14 + (j * 2), (mode > 0 && i * 8 + 4 + j == cursorPosition) ? (mode > 1 ? 41 : 31) : 33, data[i * 8 + 4 + j]);
			char line[9] = {0};
			for(int j = 0; j < 8; j++) {
				char c = data[i * 8 + j];
				if(c < ' ' || c > 127)
					line[j] = ' ';
				else
					line[j] = c;
			}
			printf("\x1B[%d;%dH\x1B[47m%.8s", i + 1, 23, line);
		}

		// Change color of selected char
		if(mode > 0)
			printf("\x1B[%d;%dH\x1B[%dm%c", 1 + cursorPosition / 8, 23 + cursorPosition % 8, mode > 1 ? 41 : 31, data[cursorPosition]);

		do {
			swiWaitForVBlank();
			scanKeys();
			pressed = keysDown();
			held = keysDownRepeat();
		} while(!held);

		if(mode == 0) {
			if (held & KEY_UP) {
				if(offset > 8)
					offset -= 8;
				else
					offset = 0;
			} else if (held & KEY_DOWN) {
				if(offset < fileSize - 8 * 23)
					offset += 8;
				else
					offset = fileSize - 8 * 23;
			} else if (held & KEY_LEFT) {
				if(offset > 8 * 23)
					offset -= 8 * 23;
				else
					offset = 0;
			} else if (held & KEY_RIGHT) {
				if(offset < fileSize - 8 * 23)
					offset += 8 * 23;
				else
					offset = fileSize - 8 * 23;
			} else if (pressed & KEY_A) {
				mode = 1;
			} else if (pressed & KEY_B) {
				return;
			} else if(pressed & KEY_Y) {
				offset = jumpToOffset(offset);
				consoleClear();
			}
		} else if(mode == 1) {
			if (held & KEY_UP) {
				if(cursorPosition >= 8)
					cursorPosition -= 8;
				else if(offset > 8)
					offset -= 8;
				else
					offset = 0;
			} else if (held & KEY_DOWN) {
				if(cursorPosition < 8 * 22)
					cursorPosition += 8;
				else if(offset < fileSize - 8 * 23)
					offset += 8;
				else
					offset = fileSize - 8 * 23;
			} else if (held & KEY_LEFT) {
				if(cursorPosition > 0)
					cursorPosition--;
			} else if (held & KEY_RIGHT) {
				if(cursorPosition < 8 * 23 - 1)
					cursorPosition++;
			} else if (pressed & KEY_A) {
				mode = 2;
			} else if (pressed & KEY_B) {
				mode = 0;
			} else if(pressed & KEY_Y) {
				offset = jumpToOffset(offset);
				consoleClear();
			}
		} else if(mode == 2) {
			if (held & KEY_UP) {
				data[cursorPosition]++;
			} else if (held & KEY_DOWN) {
				data[cursorPosition]--;
			} else if (held & KEY_LEFT) {
				data[cursorPosition] += 0x10;
			} else if (held & KEY_RIGHT) {
				data[cursorPosition] -= 0x10;
			} else if (pressed & (KEY_A | KEY_B)) {
				mode = 1;
				fseek(file, offset + cursorPosition, SEEK_SET);
				fwrite(data + cursorPosition, 1, 1, file);
			}
		}
	}

	fclose(file);
}

#include "screenshot.h"

#include "bmp.h"
#include "date.h"
#include "driveOperations.h"
#include "font.h"

#include <dirent.h>
#include <fat.h>
#include <nds.h>
#include <stdio.h>
#include <unistd.h>

void wait();

void write16(void *address, u16 value) {

	u8* first = (u8*)address;
	u8* second = first + 1;

	*first = value & 0xff;
	*second = value >> 8;
}

void write32(void *address, u32 value) {

	u8* first = (u8*)address;
	u8* second = first + 1;
	u8* third = first + 2;
	u8* fourth = first + 3;

	*first = value & 0xff;
	*second = (value >> 8) & 0xff;
	*third = (value >> 16) & 0xff;
	*fourth = (value >> 24) & 0xff;
}

bool screenshotbmp(const char* filename) {
	FILE *file = fopen(filename, "wb");

	if(!file)
		return false;

	REG_DISPCAPCNT = DCAP_BANK(DCAP_BANK_VRAM_D) | DCAP_SIZE(DCAP_SIZE_256x192) | DCAP_ENABLE;
	while(REG_DISPCAPCNT & DCAP_ENABLE);

	u8* temp = new u8[256 * 192 * 2 + sizeof(INFOHEADER) + sizeof(HEADER)];

	if(!temp) {
		fclose(file);
		return false;
	}

	HEADER *header= (HEADER*)temp;
	INFOHEADER *infoheader = (INFOHEADER*)(temp + sizeof(HEADER));

	write16(&header->type, 0x4D42);
	write32(&header->size, 256 * 192 * 2 + sizeof(INFOHEADER) + sizeof(HEADER));
	write32(&header->reserved1, 0);
	write32(&header->reserved2, 0);
	write32(&header->offset, sizeof(INFOHEADER) + sizeof(HEADER));

	write32(&infoheader->size, sizeof(INFOHEADER));
	write32(&infoheader->width, 256);
	write32(&infoheader->height, 192);
	write16(&infoheader->planes, 1);
	write16(&infoheader->bits, 16);
	write32(&infoheader->compression, 3);
	write32(&infoheader->imagesize, 256 * 192 * 2);
	write32(&infoheader->xresolution, 2835);
	write32(&infoheader->yresolution, 2835);
	write32(&infoheader->ncolours, 0);
	write32(&infoheader->importantcolours, 0);
	write32(&infoheader->redBitmask, 0xF800);
	write32(&infoheader->greenBitmask, 0x07E0);
	write32(&infoheader->blueBitmask, 0x001F);
	write32(&infoheader->reserved, 0);

	u16 *ptr = (u16*)(temp + sizeof(HEADER) + sizeof(INFOHEADER));
	for(int y = 0; y < 192; y++) {
		for(int x = 0; x < 256; x++) {
			u16 color = VRAM_D[256 * 191 - y * 256 + x];
			*(ptr++) = ((color >> 10) & 0x1F) | (color & (0x1F << 5)) << 1 | ((color & 0x1F) << 11);
		}
	}

	DC_FlushAll();
	fwrite(temp, 1, 256 * 192 * 2 + sizeof(INFOHEADER) + sizeof(HEADER), file);
	fclose(file);
	delete[] temp;
	return true;
}

bool screenshot(void) {
	if (!((sdMounted && driveWritable(Drive::sdCard)) || (flashcardMounted && driveWritable(Drive::flashcard))))
		return false;

	bool sdWritable = sdMounted && driveWritable(Drive::sdCard);
	if (access((sdWritable ? "sd:/gm9i" : "fat:/gm9i"), F_OK) != 0) {
		mkdir((sdWritable ? "sd:/gm9i" : "fat:/gm9i"), 0777);
	}
	if (access((sdWritable ? "sd:/gm9i/out" : "fat:/gm9i/out"), F_OK) != 0) {
		mkdir((sdWritable ? "sd:/gm9i/out" : "fat:/gm9i/out"), 0777);
	}

	std::string fileTimeText = RetTime("%H%M%S");
	char snapPath[40];
	// Take top screenshot
	snprintf(snapPath, sizeof(snapPath), "%s:/gm9i/out/snap_%s_top.bmp", (sdWritable ? "sd" : "fat"), fileTimeText.c_str());
	if(!screenshotbmp(snapPath))
		return false;

	// Seamlessly swap top and bottom screens
	font->mainOnTop(false);
	font->update(false);
	font->update(true);
	lcdMainOnBottom();

	// Take bottom screenshot
	snprintf(snapPath, sizeof(snapPath), "%s:/gm9i/out/snap_%s_bot.bmp", (sdWritable ? "sd" : "fat"), fileTimeText.c_str());
	if(!screenshotbmp(snapPath))
		return false;

	font->mainOnTop(true);
	font->update(true);
	font->update(false);
	lcdMainOnTop();

	return true;
}
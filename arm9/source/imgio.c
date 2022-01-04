#include "tonccpy.h"

#include <nds.h>
#include <nds/disc_io.h>
#include <stdio.h>
#include <dirent.h>

#define SECTOR_SIZE 512
// #define WRITABLE

char currentImgName[PATH_MAX];
static FILE* imgFile;

bool img_startup() {
#ifdef WRITABLE
	imgFile = fopen(currentImgName, "rb+");
#else
	imgFile = fopen(currentImgName, "rb");
#endif
	return imgFile != NULL;
}

bool img_is_inserted() {
	return imgFile != NULL;
}

bool img_read_sectors(sec_t sector, sec_t numSectors, void *buffer) {
	if (!imgFile) return false;

	fseek(imgFile, (sector << 9), SEEK_SET);
	fread(buffer, 1, (numSectors << 9), imgFile);

	return true;
}

bool dsiware_read_sectors(sec_t sector, sec_t numSectors, void *buffer) {
	if(!img_read_sectors(sector, numSectors, buffer)) return false;

	// DSiWare saves are FAT12 images, however they don't have the 'FAT' text
	// in the bootsector so libfat rejects them
	if (sector == 0) tonccpy(buffer + 0x36, "FAT", 3);
	return true;
}

bool img_write_sectors(sec_t sector, sec_t numSectors, const void *buffer) {
#ifdef WRITABLE
	if (!imgFile) return false;

	fseek(imgFile, (sector << 9), SEEK_SET);
	fwrite(buffer, 1, (numSectors << 9), imgFile);
	return true;
#else
	return false;
#endif
}

bool img_clear_status() {
	return true;
}

bool img_shutdown() {
	if (imgFile) {
		fclose(imgFile);
		imgFile = NULL;
	}
	return true;
}

const DISC_INTERFACE io_img = {
	('I' << 24) | ('M' << 16) | ('G' << 8) | 'F',
#ifdef WRITABLE
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
#else
	FEATURE_MEDIUM_CANREAD,
#endif
	img_startup,
	img_is_inserted,
	img_read_sectors,
	img_write_sectors,
	img_clear_status,
	img_shutdown
};

const DISC_INTERFACE io_dsiware_save = {
	('I' << 24) | ('M' << 16) | ('G' << 8) | 'F',
#ifdef WRITABLE
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
#else
	FEATURE_MEDIUM_CANREAD,
#endif
	img_startup,
	img_is_inserted,
	dsiware_read_sectors,
	img_write_sectors,
	img_clear_status,
	img_shutdown
};

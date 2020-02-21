
#include <nds.h>
#include <nds/disc_io.h>
#include <stdio.h>

#define SECTOR_SIZE 512

const char* currentImgName;
static FILE* imgFile[2];

bool img_startup() {
	imgFile[0] = fopen(currentImgName, "rb");
	if (imgFile[0]) {
		//imgFile[1] = fopen(currentImgName, "wb");
		return true;
	}
	return false;
}

bool img_is_inserted() {
	if (imgFile[0]) {
		return true;
	}
	return false;
}

bool img_read_sectors(sec_t sector, sec_t numSectors, void *buffer) {
	if (!imgFile[0]) return false;

	fseek(imgFile[0], (sector << 9), SEEK_SET);
	fread(buffer, 1, (numSectors << 9), imgFile[0]);
	return true;
}

bool img_write_sectors(sec_t sector, sec_t numSectors, const void *buffer) {
	/*if (!imgFile[1]) return false;

	fseek(imgFile[1], (sector << 9), SEEK_SET);
	fwrite(buffer, 1, (numSectors << 9), imgFile[1]);
	return true;*/
	return false;
}

bool img_clear_status() {
	return true;
}

bool img_shutdown() {
	fclose(imgFile[0]);
	fclose(imgFile[1]);
	return true;
}

const DISC_INTERFACE io_img = {
	('I' << 24) | ('M' << 16) | ('G' << 8) | 'F',
	//FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
	FEATURE_MEDIUM_CANREAD,
	img_startup,
	img_is_inserted,
	img_read_sectors,
	img_write_sectors,
	img_clear_status,
	img_shutdown
};

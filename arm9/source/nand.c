
#include <nds.h>
#include <nds/disc_io.h>
#include <stdio.h>

#define SECTOR_SIZE 512

static FILE* nandFile;

bool nand_startup() {
	nandFile = fopen("sd:/nand.bin", "rb");
	if (nandFile) {
		return true;
	}
	return false;
}

bool nand_is_inserted() {
	if (nandFile) {
		return true;
	}
	return false;
}

bool nand_read_sectors(sec_t sector, sec_t numSectors, void *buffer) {
	if (!nandFile) return false;

	fseek(nandFile, (sector << 9), SEEK_SET);
	fread(buffer, 1, (numSectors << 9), nandFile);
	return true;
}

bool nand_write_sectors(sec_t sector, sec_t numSectors, const void *buffer) {
	return false;
}

bool nand_clear_status() {
	return true;
}

bool nand_shutdown() {
	fclose(nandFile);
	return true;
}

const DISC_INTERFACE io_nand = {
	('N' << 24) | ('A' << 16) | ('N' << 8) | 'D',
	FEATURE_MEDIUM_CANREAD,
	nand_startup,
	nand_is_inserted,
	nand_read_sectors,
	nand_write_sectors,
	nand_clear_status,
	nand_shutdown
};

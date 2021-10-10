
#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/disc_io.h>
#include "tonccpy.h"

#define SECTOR_SIZE 512

u8* ramdLoc = (u8*)NULL;

bool ramd_startup() {
	return true;
}

bool ramd_is_inserted() {
	return true;
}

bool ramd_read_sectors(sec_t sector, sec_t numSectors, void *buffer) {
	tonccpy(buffer, ramdLoc+(sector << 9), numSectors << 9);
	return true;
}

bool ramd_write_sectors(sec_t sector, sec_t numSectors, const void *buffer) {
	tonccpy(ramdLoc+(sector << 9), buffer, numSectors << 9);
	return true;
}

bool ramd2_read_sectors(sec_t sector, sec_t numSectors, void *buffer) {
	tonccpy(buffer, (void*)0x0D000000+(sector << 9), numSectors << 9);
	return true;
}

bool ramd2_write_sectors(sec_t sector, sec_t numSectors, const void *buffer) {
	tonccpy((void*)0x0D000000+(sector << 9), buffer, numSectors << 9);
	return true;
}

bool ramd_clear_status() {
	return true;
}

bool ramd_shutdown() {
	return true;
}

const DISC_INTERFACE io_ram_drive = {
	('R' << 24) | ('A' << 16) | ('M' << 8) | '1',
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
	ramd_startup,
	ramd_is_inserted,
	ramd_read_sectors,
	ramd_write_sectors,
	ramd_clear_status,
	ramd_shutdown
};

const DISC_INTERFACE io_ram_drive2 = {
	('R' << 24) | ('A' << 16) | ('M' << 8) | '2',
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
	ramd_startup,
	ramd_is_inserted,
	ramd2_read_sectors,
	ramd2_write_sectors,
	ramd_clear_status,
	ramd_shutdown
};

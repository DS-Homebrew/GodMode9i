
#include <nds.h>
#include <nds/ndstypes.h>
#include <nds/disc_io.h>
#include "tonccpy.h"

#define SECTOR_SIZE 512

const static u8 bootSector[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, SECTOR_SIZE & 0xFF, SECTOR_SIZE >> 8, 0x04, 0x01, 0x00,
	0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'R', 'A', 'M', 'D', 'R',
	'I', 'V', 'E', ' ', ' ', ' ', 'F', 'A', 'T'
};

u32 ramdSectors = 0;
u8* ramdLoc = (u8*)NULL;
u8* ramdLocMep = (u8*)NULL;

bool ramd_startup() {
	if(isDSiMode() || REG_SCFG_EXT != 0) {
		ramdLoc = (u8*)malloc(0x6000 * SECTOR_SIZE);
	} else {
		ramdLoc = (u8*)malloc(0x8 * SECTOR_SIZE);
		toncset(ramdLocMep, 0, (ramdSectors - 0x8) * SECTOR_SIZE); // Fill MEP with 00 to avoid displaying weird files
	}

	tonccpy(ramdLoc, bootSector, sizeof(bootSector));
	toncset32(ramdLoc + 0x20, ramdSectors, 1);
	toncset16(ramdLoc + 0x1FE, 0xAA55, 1);

	return true;
}

bool ramd_is_inserted() {
	return true;
}

bool ramd_read_sectors(sec_t sector, sec_t numSectors, void *buffer) {
	if(isDSiMode() || REG_SCFG_EXT != 0) {
		if(sector < 0x6000) {
			tonccpy(buffer, ramdLoc + (sector << 9), numSectors << 9);
			return true;
		} else if(sector <= 0xE000) {
			tonccpy(buffer, (void*)0x0D000000 + ((sector - 0x6000) << 9), numSectors << 9);
			return true;
		}
	} else if(sector < 0x8) {
		tonccpy(buffer, ramdLoc + (sector << 9), numSectors << 9);
		return true;
	} else if(sector <= ramdSectors - 0x8) {
		tonccpy(buffer, ramdLocMep + ((sector - 0x8) << 9), numSectors << 9);
		return true;
	}

	return false;
}

bool ramd_write_sectors(sec_t sector, sec_t numSectors, const void *buffer) {
	if(isDSiMode() || REG_SCFG_EXT != 0) {
		if(sector < 0x6000) {
			tonccpy(ramdLoc + (sector << 9), buffer, numSectors << 9);
			return true;
		} else if(sector <= 0xE000) {
			tonccpy((void*)0x0D000000 + ((sector - 0x6000) << 9), buffer, numSectors << 9);
			return true;
		}
	} else if(sector < 0x8) {
		tonccpy(ramdLoc + (sector << 9), buffer, numSectors << 9);
		return true;
	} else if(sector <= ramdSectors - 0x8) {
		tonccpy(ramdLocMep + ((sector - 0x8) << 9), buffer, numSectors << 9);
		return true;
	}

	return false;
}

bool ramd_clear_status() {
	return true;
}

bool ramd_shutdown() {
	if((isDSiMode() || REG_SCFG_EXT != 0) && ramdLoc) {
		free(ramdLoc);
		ramdLoc = NULL;
	}

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

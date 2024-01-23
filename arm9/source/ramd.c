
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

u32 baseSectors = 0;
u32 ramdSectors = 0;
u8* ramdLoc = (u8*)NULL;
u8* ramdLocMep = (u8*)NULL;
const u16 bootSectorSignature = 0xAA55;

bool ramd_startup() {
	if(isDSiMode() || REG_SCFG_EXT != 0) {
		ramdLoc = (u8*)malloc(baseSectors * SECTOR_SIZE);
	} else {
		ramdLoc = (u8*)calloc(baseSectors * SECTOR_SIZE, 1);
		if(ramdLoc == NULL)
			return false;
		if(ramdLocMep != NULL)
			toncset(ramdLocMep, 0, (ramdSectors - baseSectors) * SECTOR_SIZE); // Fill MEP with 00 to avoid displaying weird files
	}

	tonccpy(ramdLoc, bootSector, sizeof(bootSector));
	tonccpy(ramdLoc + 0x20, &ramdSectors, 4);
	tonccpy(ramdLoc + 0x1FE, &bootSectorSignature, 2);

	return true;
}

bool ramd_is_inserted() {
	return ramdLoc != NULL;
}

bool ramd_read_sectors(sec_t sector, sec_t numSectors, void *buffer) {
	for(int i = 0; i < numSectors; i++, sector++) {
		if(sector < baseSectors) {
			tonccpy(buffer + (i * SECTOR_SIZE), ramdLoc + (sector * SECTOR_SIZE), SECTOR_SIZE);
		} else if(sector < ramdSectors) {
			tonccpy(buffer + (i * SECTOR_SIZE), ramdLocMep + ((sector - baseSectors) * SECTOR_SIZE), SECTOR_SIZE);
		} else {
			return false;
		}
	}

	return true;
}

bool ramd_write_sectors(sec_t sector, sec_t numSectors, const void *buffer) {
	for(int i = 0; i < numSectors; i++, sector++) {
		if(sector < baseSectors) {
			tonccpy(ramdLoc + (sector * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
		} else if(sector < ramdSectors) {
			tonccpy(ramdLocMep + ((sector - baseSectors) * SECTOR_SIZE), buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
		} else {
			return false;
		}
	}

	return true;
}

bool ramd_clear_status() {
	return true;
}

bool ramd_shutdown() {
	if(ramdLoc) {
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

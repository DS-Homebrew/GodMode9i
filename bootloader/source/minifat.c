/*-----------------------------------------------------------------

	minifat.c -- Minimal FAT filesystem driver

	Copyright (c) 2023 fincs
	Loosely based on code by Michael "Chishm" Chisholm

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.
	3. This notice may not be removed or altered from any source
		distribution.

------------------------------------------------------------------*/

#include "minifat.h"
#include <calico/arm/common.h>

#ifdef MINIFAT_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

// First FAT cluster number used to indicate invalid/unusable cluster numbers
#define FAT12_BAD_CLUSTER 0x0ff7
#define FAT16_BAD_CLUSTER 0xfff7
#define FAT32_BAD_CLUSTER 0x0ffffff7

// Maximum number of clusters representable in the given FAT type
#define MAX_CLUSTERS(_type) (_type##_BAD_CLUSTER-MINIFAT_CLUSTER_FIRST)

// MBR partition information
typedef struct {
	u8  status;
	u8  startHead;
	u16 startCySc;
	u8  type;
	u8  endHead;
	u16 endCySc;
	u16 lbaStartLo;
	u16 lbaStartHi;
	u16 lbaSizeLo;
	u16 lbaSizeHi;
} PARTITION;

_Static_assert(sizeof(PARTITION) == 0x10);

// BIOS Parameter Block
typedef struct {
	u16 bytesPerSector;
	u8  sectorsPerCluster;
	u16 reservedSectors;
	u8  numFATs;
	u16 rootEntries;
	u16 numSectorsSmall;
	u8  mediaDesc;
	u16 sectorsPerFAT;
	u16 sectorsPerTrk;
	u16 numHeads;
	u32 numHiddenSectors;
	u32 numSectors;
} MK_PACKED BIOS_BPB;

// Boot Sector - must be packed
typedef struct {
	u8 jmpBoot[3];
	u8 OEMName[8];
	BIOS_BPB bpb;

	union { // Different types of extended BIOS Parameter Block for FAT16 and FAT32
		struct {
			// Ext BIOS Parameter Block for FAT16
			u8  driveNumber;
			u8  reserved1;
			u8  extBootSig;
			u32 volumeID;
			u8  volumeLabel[11];
			u8  fileSysType[8];
			// Bootcode
			u8  bootCode[448];
		} MK_PACKED fat16;

		struct {
			// FAT32 extended block
			u32 sectorsPerFAT32;
			u16 extFlags;
			u16 fsVer;
			u32 rootClus;
			u16 fsInfo;
			u16 bkBootSec;
			u8  reserved[12];
			// Ext BIOS Parameter Block for FAT16
			u8  driveNumber;
			u8  reserved1;
			u8  extBootSig;
			u32 volumeID;
			u8  volumeLabel[11];
			u8  fileSysType[8];
			// Bootcode
			u8  bootCode[420];
		} MK_PACKED fat32;
	} extBlock;

	u16 bootSig;
} BOOT_SEC;

_Static_assert(sizeof(BOOT_SEC) == MINIFAT_SECTOR_SZ);

MK_INLINE u32 _minifatClustToSec(MiniFat* fat, u32 cluster)
{
	return fat->dataStart + ((cluster-MINIFAT_CLUSTER_FIRST) << fat->clusterShift);
}

MK_CONSTEXPR char _minifatToUpper(char c)
{
	return c >= 'a' && c <= 'z' ? c + 'A' - 'a' : c;
}

MK_CONSTEXPR bool _minifatIsNonZeroPo2(unsigned x)
{
	return x != 0 && (x & (x-1)) == 0;
}

static bool _minifatLoadBuffer(MiniFat* fat, u32 sector)
{
	bool ok = true;
	if (fat->bufferPos != sector) {
		ok = fat->discReadFn(sector, 1, fat->buffer);
		if (ok) {
			fat->bufferPos = sector;
		}
	}
	return ok;
}

static bool _minifatInitParams(MiniFat* fat)
{
	BOOT_SEC* buf = (BOOT_SEC*)fat->buffer;

	// Check for a valid boot signature
	if (buf->bootSig != 0xaa55) {
		dietPrint("Bad boot sig\n");
		return false;
	}

	// Check for a valid Microsoft VBR
	unsigned jmp = buf->jmpBoot[0];
	if (jmp != 0xeb && jmp != 0xe9 && jmp != 0xe8) {
		dietPrint("Bad jump opcode\n");
		goto _isUnknown;
	}

	/* Not needed
	static const u8 fat32_ident[] = { 'F', 'A', 'T', '3', '2', ' ', ' ', ' ' };
	if (memcmp(buf->extBlock.fat32.fileSysType, fat32_ident, 8) == 0) {
		//... etc
	}
	*/

	// Retrieve FAT size
	u32 sectorsPerFat = buf->bpb.sectorsPerFAT;
	if (!sectorsPerFat) {
		sectorsPerFat = buf->extBlock.fat32.sectorsPerFAT32;
	}

	// Validate basic FAT VBR parameters
	if (
		buf->bpb.bytesPerSector != MINIFAT_SECTOR_SZ || // Only supporting 512-byte sectors
		!_minifatIsNonZeroPo2(buf->bpb.sectorsPerCluster) ||
		buf->bpb.reservedSectors == 0 ||
		(buf->bpb.numFATs != 1 && buf->bpb.numFATs != 2) ||
		(buf->bpb.numSectorsSmall < 0x80 && buf->bpb.numSectors < 0x10000) ||
		sectorsPerFat == 0
	) {
		dietPrint("Bad FAT validation\n");
		goto _isUnknown;
	}

	// Calculate total volume size in sectors
	u32 totalSectors = buf->bpb.numSectorsSmall;
	if (!totalSectors) {
		totalSectors = buf->bpb.numSectors;
	}

	// Initialize FAT parameters
	fat->clusterSectors = buf->bpb.sectorsPerCluster;
	fat->clusterShift = 0;
	fat->fatStart = fat->bufferPos + buf->bpb.reservedSectors;
	fat->rootDirStart = fat->fatStart + buf->bpb.numFATs*sectorsPerFat;
	fat->dataStart = fat->rootDirStart + (buf->bpb.rootEntries*sizeof(MiniFatDirEnt) + MINIFAT_SECTOR_SZ-1) / MINIFAT_SECTOR_SZ;

	// Calculate cluster sector size shift (log2)
	while ((1U << fat->clusterShift) != fat->clusterSectors) {
		fat->clusterShift ++;
	}

	// Calculate number of clusters
	u32 numClusters = (fat->bufferPos + totalSectors - fat->dataStart) >> fat->clusterShift;
	dietPrint("! FAT num clus 0x%lx\n", numClusters);

	// Discriminate FAT version according to number of clusters
	// XX: Note that the official FAT specification is off-by-one here.
	// In any case, well-behaved formatting tools will avoid volume sizes
	// dangerously close to the problematic decision boundaries.
	if (numClusters > MAX_CLUSTERS(FAT32)) {
		goto _isUnknown;
	} else if (numClusters > MAX_CLUSTERS(FAT16)) {
		// FAT32 - validate parameters
		if (
			buf->bpb.rootEntries != 0 ||
			buf->extBlock.fat32.fsVer != 0 ||
			buf->extBlock.fat32.rootClus < MINIFAT_CLUSTER_FIRST ||
			buf->extBlock.fat32.rootClus >= (MINIFAT_CLUSTER_FIRST + numClusters)
		) {
			dietPrint("Bad FAT32 params\n");
			goto _isUnknown;
		}

		// Select the correct FAT if mirroring is disabled
		// XX: FatFs does not implement this for some reason
		if (buf->extBlock.fat32.extFlags & 0x80) {
			fat->fatStart += (buf->extBlock.fat32.extFlags & 0x0f)*sectorsPerFat;
		}

		fat->fsType = MiniFatType_Fat32;
		fat->rootDirCluster = buf->extBlock.fat32.rootClus;
		fat->rootDirStart = _minifatClustToSec(fat, fat->rootDirCluster);
	} else {
		// FAT12/16 - validate parameters
		if (buf->bpb.rootEntries == 0) {
			dietPrint("Bad FAT12/16 params\n");
			goto _isUnknown;
		}

		fat->fsType = numClusters > MAX_CLUSTERS(FAT12) ? MiniFatType_Fat16 : MiniFatType_Fat12;
		fat->rootDirCluster = MINIFAT_CLUSTER_FREE; // FAT12/16 store the root dir outside of the data area
	}

	return true;

_isUnknown:
	fat->fsType = MiniFatType_Unknown;
	return true;
}

static bool _minifatInitPartition(MiniFat* fat, u32 sector)
{
	return
		sector != 0 &&
		_minifatLoadBuffer(fat, sector) &&
		_minifatInitParams(fat) &&
		fat->fsType != MiniFatType_Unknown;
}

bool minifatInit(MiniFat* fat, MiniFatDiscReadFn discReadFn, unsigned partitionIndex)
{
	if (partitionIndex > 4) {
		return false;
	}

	fat->discReadFn = discReadFn;
	fat->bufferPos = UINT32_MAX;

	// Read and parse first sector of card
	if (!_minifatLoadBuffer(fat, 0) || !_minifatInitParams(fat)) {
		return false;
	}

	// If we have a valid boot sector but not a valid filesystem, check MBR
	if (fat->fsType == MiniFatType_Unknown) {
		// Extract partition offsets from MBR
		u32 partOffsets[4];
		for (unsigned i = 0; i < 4; i ++) {
			PARTITION* part = &((PARTITION*)&fat->buffer[0x1be])[i];

			// Fail if MBR is malformed
			if (part->status != 0x80 && part->status != 0x00) {
				return false;
			}

			// Ignore unpopulated/extended partitions
			if (part->type == 0x00 || part->type == 0x05 || part->type == 0x0f) {
				partOffsets[i] = 0;
			} else {
				partOffsets[i] = part->lbaStartLo | (part->lbaStartHi << 16);
			}
		}

		// Try to initialize the specified partition, or the first partition that succeeds
		bool ok = false;
		if (partitionIndex) {
			ok = _minifatInitPartition(fat, partOffsets[partitionIndex-1]);
		} else for (unsigned i = 0; !ok && i < 4; i ++) {
			ok = _minifatInitPartition(fat, partOffsets[i]);
		}

		// If above did not successfully initialize a partition, fail
		if (!ok) {
			return false;
		}
	}

	dietPrint("! FAT at 0x%lx type%u\n", fat->bufferPos, fat->fsType);
	dietPrint("  clusSect=%u clusShift=%u\n", fat->clusterSectors, fat->clusterShift);
	return true;
}

u32 minifatNextCluster(MiniFat* fat, u32 cluster)
{
	u32 sector;
	u32 offset;
	u32 ret = MINIFAT_CLUSTER_FREE;

	if (cluster < MINIFAT_CLUSTER_FIRST) {
		return cluster;
	}

	switch (fat->fsType) {
		default:
		case MiniFatType_Unknown:
			break;

		case MiniFatType_Fat12:
			sector = fat->fatStart + (((cluster * 3) / 2) / MINIFAT_SECTOR_SZ);
			offset = ((cluster * 3) / 2) % MINIFAT_SECTOR_SZ;
			if (!_minifatLoadBuffer(fat, sector)) {
				break;
			}

			ret = fat->buffer[offset];
			offset++;

			if (offset >= MINIFAT_SECTOR_SZ) {
				offset = 0;
				sector++;
			}

			if (!_minifatLoadBuffer(fat, sector)) {
				break;
			}

			ret |= fat->buffer[offset] << 8;

			if (cluster & 1) {
				ret >>= 4;
			} else {
				ret &= 0x0fff;
			}

			if (ret >= FAT12_BAD_CLUSTER) {
				ret = MINIFAT_CLUSTER_EOF;
			}
			break;

		case MiniFatType_Fat16:
			sector = fat->fatStart + ((cluster << 1) / MINIFAT_SECTOR_SZ);
			offset = cluster % (MINIFAT_SECTOR_SZ >> 1);
			if (!_minifatLoadBuffer(fat, sector)) {
				break;
			}

			// read the next cluster value
			ret = ((u16*)fat->buffer)[offset];

			if (ret >= FAT16_BAD_CLUSTER) {
				ret = MINIFAT_CLUSTER_EOF;
			}
			break;

		case MiniFatType_Fat32:
			sector = fat->fatStart + ((cluster << 2) / MINIFAT_SECTOR_SZ);
			offset = cluster % (MINIFAT_SECTOR_SZ >> 2);
			if (!_minifatLoadBuffer(fat, sector)) {
				break;
			}

			// read the next cluster value
			ret = ((u32*)fat->buffer)[offset] & 0x0fffffff;

			if (ret >= FAT32_BAD_CLUSTER) {
				ret = MINIFAT_CLUSTER_EOF;
			}
			break;
	}

	return ret;
}

MK_INLINE void _minifatMake8dot3(char* buf, const char* name)
{
	unsigned i = 0;

	// Copy filename
	for (; i < 8 && *name && *name != '.'; i ++, name ++) {
		buf[i] = _minifatToUpper(*name);
	}

	// Pad filename
	for (; i < 8; i ++) {
		buf[i] = ' ';
	}

	// Skip extension dot
	if (*name == '.') name++;

	// Copy extension
	for (; i < 8+3 && *name; i ++, name ++) {
		buf[i] = _minifatToUpper(*name);
	}

	// Pad extension
	for (; i < 8+3; i ++) {
		buf[i] = ' ';
	}
}

u32 minifatFind(MiniFat* fat, u32 dirCluster, const char* name, MiniFatDirEnt* out_entry)
{
	char name8dot3[8+3];
	_minifatMake8dot3(name8dot3, name);

	u32 baseSector;
	if (dirCluster < MINIFAT_CLUSTER_FIRST) {
		dirCluster = fat->rootDirCluster;
		baseSector = fat->rootDirStart;
	} else {
		baseSector = _minifatClustToSec(fat, dirCluster);
	}

	u32 sectorOff = 0;
	for (;;) {
		if (!_minifatLoadBuffer(fat, baseSector + sectorOff)) {
			return MINIFAT_CLUSTER_EOF;
		}

		MiniFatDirEnt* entries = (void*)fat->buffer;
		for (unsigned i = 0; i < MINIFAT_SECTOR_SZ / sizeof(MiniFatDirEnt); i ++) {
			MiniFatDirEnt* entry = &entries[i];

			if (entry->name[0] == 0x00) {
				return MINIFAT_CLUSTER_EOF;
			}

			if (entry->name[0] == 0xe5 || (entry->attrib & MINIFAT_ATTRIB_VOLUME)) {
				continue;
			}

			if (__builtin_memcmp(entry->name_ext, name8dot3, 8+3) == 0) {
				if (out_entry) *out_entry = *entry;
				return entry->startCluster | (entry->startClusterHigh << 16);
			}
		}

		sectorOff ++;

		if (dirCluster >= MINIFAT_CLUSTER_FIRST) {
			if (sectorOff == fat->clusterSectors) {
				dirCluster = minifatNextCluster(fat, dirCluster);
				if (dirCluster < MINIFAT_CLUSTER_FIRST) {
					return MINIFAT_CLUSTER_EOF;
				}

				baseSector = _minifatClustToSec(fat, dirCluster);
				sectorOff = 0;
			}
		} else if (baseSector + sectorOff == fat->dataStart) {
			return MINIFAT_CLUSTER_EOF;
		}
	}
}

MK_INLINE bool _minifatLargeRead(MiniFat* fat, void** pBuffer, u32* pTotalReadLen, u32 readPos, u32 readLen)
{
	bool ok = fat->discReadFn(readPos, readLen, *pBuffer);
	if (ok) {
		u32 readBytes = readLen * MINIFAT_SECTOR_SZ;
		*pBuffer = (u8*)*pBuffer + readBytes;
		*pTotalReadLen += readBytes;
	}
	return ok;
}

u32 minifatRead(MiniFat* fat, u32 objCluster, void* buffer, u32 offset, u32 length)
{
	const u32 clusterSectors = fat->clusterSectors;
	const u32 clusterBytes = MINIFAT_SECTOR_SZ << fat->clusterShift;

	// All offsets must be word aligned
	if (((uptr)buffer | offset | length) & 3) {
		return 0;
	}

	// Skip over initial clusters
	while (objCluster >= MINIFAT_CLUSTER_FIRST && offset >= clusterBytes) {
		offset -= clusterBytes;
		objCluster = minifatNextCluster(fat, objCluster);
	}

	// Fail early if EOF
	if (objCluster < MINIFAT_CLUSTER_FIRST) {
		return 0;
	}

	// Initialize variables
	u32 totalReadLen = 0;
	u32 baseSector = _minifatClustToSec(fat, objCluster);
	u32 sectorOff = offset / MINIFAT_SECTOR_SZ;
	offset &= MINIFAT_SECTOR_SZ-1;

	// Read the head if needed
	if (offset || length < MINIFAT_SECTOR_SZ) {
		if (!_minifatLoadBuffer(fat, baseSector+sectorOff)) {
			return 0;
		}

		u32 maxLen = MINIFAT_SECTOR_SZ - offset;
		totalReadLen = length < maxLen ? length : maxLen;
		armCopyMem32(buffer, &fat->buffer[offset], totalReadLen);

		buffer = (u8*)buffer + totalReadLen;
		sectorOff ++;
		length -= totalReadLen;
	}

	// Main body read loop, iterating through clusters
	u32 accumReadPos = 0;
	u32 accumReadLen = 0;
	while (length >= MINIFAT_SECTOR_SZ) {
		u32 maxSectors = clusterSectors - sectorOff;
		u32 remSectors = length / MINIFAT_SECTOR_SZ;

		u32 readLen = remSectors < maxSectors ? remSectors : maxSectors;
		if_likely (readLen) {
			u32 readPos = baseSector+sectorOff;
			if_likely (accumReadLen) {
				u32 nextReadPos = accumReadPos + accumReadLen;
				if_likely (nextReadPos == readPos) {
					accumReadLen += readLen;
				} else if (!_minifatLargeRead(fat, &buffer, &totalReadLen, accumReadPos, accumReadLen)) {
					return totalReadLen;
				} else {
					goto _startNewRead;
				}
			} else {
_startNewRead:
				accumReadPos = readPos;
				accumReadLen = readLen;
			}

			sectorOff += readLen;
			length -= readLen * MINIFAT_SECTOR_SZ;
		}

		if (length && sectorOff >= clusterSectors) {
			objCluster = minifatNextCluster(fat, objCluster);
			if (objCluster < MINIFAT_CLUSTER_FIRST) {
				return totalReadLen;
			}

			baseSector = _minifatClustToSec(fat, objCluster);
			sectorOff = 0;
		}
	}

	if (accumReadLen && !_minifatLargeRead(fat, &buffer, &totalReadLen, accumReadPos, accumReadLen)) {
		return totalReadLen;
	}

	// Read the tail if needed
	if (length) {
		if (!_minifatLoadBuffer(fat, baseSector+sectorOff)) {
			return totalReadLen;
		}

		armCopyMem32(buffer, &fat->buffer[0], length);
		totalReadLen += length;
	}

	return totalReadLen;
}

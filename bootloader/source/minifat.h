/*-----------------------------------------------------------------

	minifat.h -- Minimal FAT filesystem driver

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
/*! \file minifat.h
\brief Minimal FAT filesystem driver
*/

#pragma once
#include <nds/ndstypes.h>

#define MINIFAT_SECTOR_SZ 512 //!< Size of a sector in bytes

#define MINIFAT_CLUSTER_FREE  0 //!< Free cluster
#define MINIFAT_CLUSTER_EOF   1 //!< End-of-file marker
#define MINIFAT_CLUSTER_FIRST 2 //!< First valid cluster

#define MINIFAT_ATTRIB_ARCHIVE  0x20 //!< Archive attribute
#define MINIFAT_ATTRIB_DIR      0x10 //!< Directory attribute
#define MINIFAT_ATTRIB_LFN      0x0f //!< Long File Name attribute combination
#define MINIFAT_ATTRIB_VOLUME   0x08 //!< Volume attribute
#define MINIFAT_ATTRIB_HIDDEN   0x02 //!< Hidden attribute
#define MINIFAT_ATTRIB_SYSTEM   0x04 //!< System attribute
#define MINIFAT_ATTRIB_READONLY 0x01 //!< Read-only attribute

//! Disc read callback function type
typedef bool (*MiniFatDiscReadFn)(u32 sector, u32 numSectors, void* buffer);

//! FAT filesystem type
typedef enum MiniFatType {
	MiniFatType_Unknown,
	MiniFatType_Fat12,
	MiniFatType_Fat16,
	MiniFatType_Fat32,
} MiniFatType;

//! FAT filesystem driver state
typedef struct MiniFat {
	alignas(4) u8 buffer[MINIFAT_SECTOR_SZ];

	MiniFatDiscReadFn discReadFn;
	u32 bufferPos;

	MiniFatType fsType;
	u8 clusterSectors;
	u8 clusterShift;

	u32 fatStart;
	u32 rootDirStart;
	u32 rootDirCluster;
	u32 dataStart;
} MiniFat;

//! FAT directory entry
typedef struct MiniFatDirEnt {
	union {
		char name_ext[8+3];
		struct {
			char name[8];
			char ext[3];
		};
	};
	u8  attrib;
	u8  reserved;
	u8  cTime_ms;
	u16 cTime;
	u16 cDate;
	u16 aDate;
	u16 startClusterHigh;
	u16 mTime;
	u16 mDate;
	u16 startCluster;
	u32 fileSize;
} MiniFatDirEnt;

//! Initialize
bool minifatInit(MiniFat* fat, MiniFatDiscReadFn discReadFn, unsigned partitionIndex);

//! Return
u32 minifatNextCluster(MiniFat* fat, u32 cluster);

//! Find
u32 minifatFind(MiniFat* fat, u32 dirCluster, const char* name, MiniFatDirEnt* out_entry);

//! Read
u32 minifatRead(MiniFat* fat, u32 objCluster, void* buffer, u32 offset, u32 length);

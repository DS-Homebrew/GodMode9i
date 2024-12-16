/*-----------------------------------------------------------------
 boot.c

 BootLoader
 Loads a file into memory and runs it

 All resetMemory and startBinary functions are based
 on the MultiNDS loader by Darkain.
 Original source available at:
 http://cvs.sourceforge.net/viewcvs.py/ndslib/ndslib/examples/loader/boot/main.cpp

License:
 Copyright (C) 2005  Michael "Chishm" Chisholm

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 If you use this code, please give due credit and email me about your
 project at chishm@hotmail.com

Helpful information:
 This code runs from VRAM bank C on ARM7
------------------------------------------------------------------*/

#include <nds/ndstypes.h>
#include <nds/dma.h>
#include <nds/system.h>
#include <nds/interrupts.h>
#include <nds/timers.h>
#include <nds/memory.h>
#include <nds/arm7/audio.h>
#include <calico/nds/env.h>
#include <calico/nds/arm7/aes.h>
#include "boot.h"
#include "io_dldi.h"
#include "sdmmc.h"
#include "minifat.h"

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Important things
#define TEMP_MEM MM_ENV_FREE_D000
#define ARM9_START_ADDRESS (*(vu32*)&g_envAppNdsHeader->arm9_entrypoint)
#define DEFAULT_BOOT_NAME "BOOT.NDS"

extern volatile uptr __irq_vector;

extern unsigned long _start;
extern unsigned long storedFileCluster;
extern unsigned long wantToPatchDLDI;
extern unsigned long argStart;
extern unsigned long argSize;
extern unsigned long dsiSD;
extern unsigned long dsiMode;

static uptr temp_arm9_start_address;

static char boot_nds[] = "fat:/boot.nds";
static unsigned long argbuf[4];

static MiniFat fatState;

/*-------------------------------------------------------------------------
passArgs_ARM7
Copies the command line arguments to the end of the ARM9 binary,
then sets a flag in memory for the loaded NDS to use
--------------------------------------------------------------------------*/
static void passArgs_ARM7 (void) {
	void* argSrc;
	void* argDst;

	if (!argStart || !argSize) {
		char *arg = boot_nds;
		argSize = __builtin_strlen(boot_nds);

		if (dsiSD) {
			arg++;
			arg[0] = 's';
			arg[1] = 'd';
		}
		__builtin_memcpy(argbuf,arg,argSize+1);
		argSrc = argbuf;
	} else {
		argSrc = (void*)(argStart + (uptr)&_start);
	}

	argDst = (void*)((g_envAppNdsHeader->arm9_ram_address + g_envAppNdsHeader->arm9_size + 3) & ~3);		// Word aligned

	if (dsiMode && (g_envAppNdsHeader->unitcode & BIT(1)) && g_envAppTwlHeader->arm9i_size)
	{
		void* argDst2 = (void*)((g_envAppTwlHeader->arm9i_ram_address + g_envAppTwlHeader->arm9i_size + 3) & ~3);		// Word aligned
		if (argDst2 > argDst)
			argDst = argDst2;
	}

	armCopyMem32(argDst, argSrc, (argSize + 3) &~ 3);

	g_envNdsArgvHeader->magic = ENV_NDS_ARGV_MAGIC;
	g_envNdsArgvHeader->args_str = argDst;
	g_envNdsArgvHeader->args_str_size = argSize;
}

/*-------------------------------------------------------------------------
resetMemory_ARM7
Clears all of the NDS's RAM that is visible to the ARM7
Written by Darkain.
Modified by Chishm:
 * Added STMIA clear mem loop
--------------------------------------------------------------------------*/
static void resetMemory_ARM7 (void)
{
	// Reset the interrupt controller & related BIOS variables
	REG_IME = 0;
	REG_IE = 0;
	REG_IF = ~0;
	__irq_vector = 0;
	__irq_flags = ~0;
	__irq_flags2 = ~0;

	REG_POWCNT = POWCNT_SOUND;  //turn off power to stuff

	for (unsigned i=0; i<16; i++) {
		SCHANNEL_CR(i) = 0;
		SCHANNEL_TIMER(i) = 0;
		SCHANNEL_SOURCE(i) = 0;
		SCHANNEL_LENGTH(i) = 0;
	}

	REG_SOUNDCNT = 0;

	//clear out ARM7 DMA channels and timers
	for (unsigned i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
	}

	// Clear most of ARM7 exclusive WRAM
	extern char __sys_start[];
	armFillMem32((void*)MM_A7WRAM, 0, __sys_start - (char*)MM_A7WRAM);

	// Clear most of main RAM
	uptr main_ram_clr_begin = dsiMode ? MM_ENV_TWL_AUTOLOAD_EXT : MM_MAINRAM;
	uptr main_ram_clr_end = MM_ENV_HB_BOOTSTUB - (dsiMode ? 0 : (MM_MAINRAM_SZ_TWL-MM_MAINRAM_SZ_NTR));
	armFillMem32((void*)main_ram_clr_begin, 0, main_ram_clr_end-main_ram_clr_begin);

	// Repair ARM7 mirror of SCFG regs if they have been previously cleared out
	if (dsiMode && !__scfg_buf.ext && !__scfg_buf.other) {
		__scfg_buf.ext = g_scfgBackup->ext;
		__scfg_buf.other = g_scfgBackup->other;
	}

	// XX: Previously we would read user settings from NVRAM here. However,
	// either the previously loaded app or the app we want to load are
	// guaranteed to be hbmenu, which already does this on startup by virtue
	// of being compiled with a modern enough version of libnds.

	// Repair AES keyslot used by NAND encryption
	if (dsiMode) {
		REG_AES_SLOTxY(3).data[2] = 0x202DDD1D;
		REG_AES_SLOTxY(3).data[3] = 0xE1A00005;
		aesBusyWaitReady();
	}
}

static void loadBinary_ARM7 (u32 fileCluster)
{
	EnvNdsHeader ndsHeader;

	// read NDS header
	minifatRead(&fatState, fileCluster, &ndsHeader, 0, sizeof(ndsHeader));

	// Load binaries into memory
	minifatRead(&fatState, fileCluster, (void*)ndsHeader.arm9_ram_address, ndsHeader.arm9_rom_offset, ndsHeader.arm9_size);
	minifatRead(&fatState, fileCluster, (void*)ndsHeader.arm7_ram_address, ndsHeader.arm7_rom_offset, ndsHeader.arm7_size);

	// first copy the header to its proper location, excluding
	// the ARM9 start address, so as not to start it
	temp_arm9_start_address = ndsHeader.arm9_entrypoint;		// Store for later
	ndsHeader.arm9_entrypoint = 0;
	dmaCopyWords(3, &ndsHeader, g_envAppNdsHeader, sizeof(EnvNdsHeader));

	if (dsiMode && (ndsHeader.unitcode & BIT(1)))
	{
		// Read full TWL header
		minifatRead(&fatState, fileCluster, g_envAppTwlHeader, 0, sizeof(EnvTwlHeader));

		// Load TWL binaries into memory
		if (g_envAppTwlHeader->arm9i_size)
			minifatRead(&fatState, fileCluster, (void*)g_envAppTwlHeader->arm9i_ram_address, g_envAppTwlHeader->arm9i_rom_offset, g_envAppTwlHeader->arm9i_size);
		if (g_envAppTwlHeader->arm7i_size)
			minifatRead(&fatState, fileCluster, (void*)g_envAppTwlHeader->arm7i_ram_address, g_envAppTwlHeader->arm7i_rom_offset, g_envAppTwlHeader->arm7i_size);
	}
}

/*-------------------------------------------------------------------------
startBinary_ARM7
Jumps to the ARM7 NDS binary in sync with the display and ARM9
Written by Darkain.
Modified by Chishm:
 * Removed MultiNDS specific stuff
--------------------------------------------------------------------------*/
static void startBinary_ARM7 (void) {
	REG_IME=0;
	while(REG_VCOUNT!=191);
	while(REG_VCOUNT==191);
	// copy NDS ARM9 start address into the header, starting ARM9
	ARM9_START_ADDRESS = temp_arm9_start_address;
	ARM9_START_FLAG = 1;
	// Start ARM7
	VoidFn arm7code = (VoidFn)g_envAppNdsHeader->arm7_entrypoint;
	arm7code();
}

extern const char mpu_reset[];
extern const char mpu_reset_end[];

int main (void) {
#ifdef NO_DLDI
	dsiSD = true;
	dsiMode = true;
#endif

	bool ok = false;
	MiniFatDiscReadFn readFn;

#ifndef NO_SDMMC
	if (dsiSD && dsiMode) {
		sdmmc_controller_init(true);
		ok = sdmmc_sdcard_init() == 0;
		readFn = sdmmc_sdcard_readsectors;
	}
#ifndef NO_DLDI
	else
#endif
#endif
#ifndef NO_DLDI
	{
		ok = _io_dldi.startup();
		readFn = _io_dldi.readSectors;
	}
#endif

	u32 fileCluster = storedFileCluster;
	// Init card
	if(!ok || !minifatInit(&fatState, readFn, 0))
	{
		return -1;
	}
	if (fileCluster < MINIFAT_CLUSTER_FIRST) 	/* Invalid file cluster specified */
	{
		MiniFatDirEnt ent;
		fileCluster = minifatFind(&fatState, 0, DEFAULT_BOOT_NAME, &ent);
		if (fileCluster < MINIFAT_CLUSTER_FIRST || (ent.attrib & MINIFAT_ATTRIB_DIR))
		{
			return -1;
		}
	}

	// ARM9 clears its memory part 2
	// copy ARM9 function to RAM, and make the ARM9 jump to it
	armCopyMem32((void*)TEMP_MEM, resetMemory2_ARM9, resetMemory2_ARM9_size);
	ARM9_START_ADDRESS = TEMP_MEM;	// Make ARM9 jump to the function
	// Wait until the ARM9 has completed its task
	while (ARM9_START_ADDRESS == TEMP_MEM);

	// ARM9 sets up mpu
	// copy ARM9 function to RAM, and make the ARM9 jump to it
	armCopyMem32((void*)TEMP_MEM, mpu_reset, mpu_reset_end - mpu_reset);
	ARM9_START_ADDRESS = TEMP_MEM;	// Make ARM9 jump to the function
	// Wait until the ARM9 has completed its task
	while (ARM9_START_ADDRESS == TEMP_MEM);

	// Get ARM7 to clear RAM
	resetMemory_ARM7();

	// ARM9 enters a wait loop
	// copy ARM9 function to RAM, and make the ARM9 jump to it
	armCopyMem32((void*)TEMP_MEM, startBinary_ARM9, startBinary_ARM9_size);
	ARM9_START_ADDRESS = TEMP_MEM;	// Make ARM9 jump to the function

	// Load the NDS file
	loadBinary_ARM7(fileCluster);

#ifndef NO_DLDI
	// Patch with DLDI if desired
	if (wantToPatchDLDI) {
		dldiPatchBinary((void*)g_envAppNdsHeader->arm9_ram_address, g_envAppNdsHeader->arm9_size, &_dldi_start);
	}
#endif

#ifndef NO_SDMMC
	if (dsiSD && dsiMode) {
		sdmmc_controller_init(true);
		*(vu16*)(SDMMC_BASE + REG_SDDATACTL32) &= 0xFFFDu;
		*(vu16*)(SDMMC_BASE + REG_SDDATACTL) &= 0xFFDDu;
		*(vu16*)(SDMMC_BASE + REG_SDBLKLEN32) = 0;
	}
#endif

	// Pass command line arguments to loaded program
	passArgs_ARM7();

	startBinary_ARM7();

	return 0;
}

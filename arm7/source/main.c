/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include <nds.h>
#include <string.h>

#include "gba.h"

#define SD_IRQ_STATUS (*(vu32*)0x400481C)

void my_installSystemFIFO(void);
void my_sdmmc_get_cid(int devicenumber, u32 *cid);

u8 my_i2cReadRegister(u8 device, u8 reg);
u8 my_i2cWriteRegister(u8 device, u8 reg, u8 data);

//---------------------------------------------------------------------------------
void ReturntoDSiMenu() {
//---------------------------------------------------------------------------------
	if (isDSiMode()) {
		i2cWriteRegister(0x4A, 0x70, 0x01);		// Bootflag = Warmboot/SkipHealthSafety
		i2cWriteRegister(0x4A, 0x11, 0x01);		// Reset to DSi Menu
	} else {
		u8 readCommand = readPowerManagement(0x10);
		readCommand |= BIT(0);
		writePowerManagement(0x10, readCommand);
	}
}

//---------------------------------------------------------------------------------
void VblankHandler(void) {
//---------------------------------------------------------------------------------
	if(fifoCheckValue32(FIFO_USER_02)) {
		ReturntoDSiMenu();
	}
}

//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

volatile bool exitflag = false;

//---------------------------------------------------------------------------------
void powerButtonCB() {
//---------------------------------------------------------------------------------
	exitflag = true;
}

void set_ctr(u32* ctr){
	for (int i = 0; i < 4; i++) REG_AES_IV[i] = ctr[3-i];
}

// 10 11  22 23 24 25
void aes(void* in, void* out, void* iv, u32 method){ //this is sort of a bodged together dsi aes function adapted from this 3ds function
	REG_AES_CNT = ( AES_CNT_MODE(method) |           //https://github.com/TiniVi/AHPCFW/blob/master/source/aes.c#L42
					AES_WRFIFO_FLUSH |				 //as long as the output changes when keyslot values change, it's good enough.
					AES_RDFIFO_FLUSH | 
					AES_CNT_KEY_APPLY | 
					AES_CNT_KEYSLOT(3) |
					AES_CNT_DMA_WRITE_SIZE(2) |
					AES_CNT_DMA_READ_SIZE(1)
					);

	if (iv != NULL) set_ctr((u32*)iv);
	REG_AES_BLKCNT = (1 << 16);
	REG_AES_CNT |= 0x80000000;
	
	for (int j = 0; j < 0x10; j+=4) REG_AES_WRFIFO = *((u32*)(in+j));
	while(((REG_AES_CNT >> 0x5) & 0x1F) < 0x4); //wait for every word to get processed
	for (int j = 0; j < 0x10; j+=4) *((u32*)(out+j)) = REG_AES_RDFIFO;
	//REG_AES_CNT &= ~0x80000000;
	//if (method & (AES_CTR_DECRYPT | AES_CTR_ENCRYPT)) add_ctr((u8*)iv);
}

//---------------------------------------------------------------------------------
// Read the firmware flash JEDEC ID (RDID, 9Fh) over the firmware SPI. Returns the
// three ID bytes (manufacturer, device type, capacity per GBATEK) packed big-endian
// (id[0]<<16 | id[1]<<8 | id[2]). Informational for the hardware-info screen only; the
// firmware flash is second-sourced so the value varies by chip and is NOT used for
// detection. IRQs are masked for the transaction because the VCOUNT handler reads the
// touchscreen over the same SPI bus and would otherwise interleave and corrupt it.
static u32 readFirmwareJEDEC(void) {
//---------------------------------------------------------------------------------
	u32 id = 0;
	int oldIME = REG_IME;
	REG_IME = 0;

	while (REG_SPICNT & SPI_BUSY);
	REG_SPICNT = SPI_ENABLE | SPI_CONTINUOUS | SPI_DEVICE_NVRAM;
	REG_SPIDATA = 0x9F;			// RDID
	while (REG_SPICNT & SPI_BUSY);
	for (int i = 0; i < 3; i++) {
		REG_SPIDATA = 0;		// clock out a dummy byte to read one in
		while (REG_SPICNT & SPI_BUSY);
		id = (id << 8) | (REG_SPIDATA & 0xFF);
	}
	REG_SPICNT = 0;				// release chip-select

	REG_IME = oldIME;
	return id;
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	*(vu32*)0x400481C = 0;				// Clear SD IRQ stat register
	*(vu32*)0x4004820 = 0;				// Clear SD IRQ mask register

	REG_MBK9 = 0; // Allow full DSi WRAM access to ARM9

	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	REG_SOUNDCNT |= SOUND_ENABLE;
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	powerOn(POWER_SOUND);

	readUserSettings();
	ledBlink(0);

	irqInit();
	// Start the RTC tracking IRQ
	initClockIRQ();

	touchInit();

	fifoInit();
	
	SetYtrigger(80);
	
	my_installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT );

	setPowerButtonCB(powerButtonCB);

	// Check for 3DS
	if(isDSiMode() || (REG_SCFG_EXT & BIT(22))) {
		u8 byteBak = my_i2cReadRegister(0x4A, 0x71);
		my_i2cWriteRegister(0x4A, 0x71, 0xD2);
		fifoSendValue32(FIFO_USER_05, my_i2cReadRegister(0x4A, 0x71));
		my_i2cWriteRegister(0x4A, 0x71, byteBak);
	}

	u8 *out=(u8*)0x02F00000;

	if (isDSiMode()) {
		memset(out, 0, 17);

		// Save whether this is a dev unit or not. For 3DS NAND reading...
		// This does not imply 32 MBs of RAM!
		out[16] = (*((uint16_t*)0x04004024)) & 0x13; // Is this a dev unit?
	}

	if (isDSiMode() || ((REG_SCFG_EXT & BIT(17)) && (REG_SCFG_EXT & BIT(18)))) {
		// first check whether we can read the console ID directly and it was not hidden by SCFG
		if (((*(vu16*)0x04004000) & (1u << 10)) == 0 && ((*(vu8*)0x04004D08) & 0x1) == 0)
		{
			// The console id registers are readable, so use them!
			memcpy(out, (u8*)0x04004D00, 8);
		}
		if(out[0] == 0 || out[1] == 0) {
			// For getting ConsoleID without reading from 0x4004D00...
			u8 base[16]={0};
			u8 in[16]={0};
			u8 iv[16]={0};
			u8 *scratch=(u8*)0x02F00200; 
			u8 *key3=(u8*)0x40044D0;
			
			aes(in, base, iv, 2);

			//write consecutive 0-255 values to any byte in key3 until we get the same aes output as "base" above - this reveals the hidden byte. this way we can uncover all 16 bytes of the key3 normalkey pretty easily.
			//greets to Martin Korth for this trick https://problemkaputt.de/gbatek.htm#dsiaesioports (Reading Write-Only Values)
			for(int i=0;i<16;i++){  
				for(int j=0;j<256;j++){
					*(key3+i)=j & 0xFF;
					aes(in, scratch, iv, 2);
					if(!memcmp(scratch, base, 16)){
						out[i]=j;
						//hit++;
						break;
					}
				}
			}
		}
	}

	fifoSendValue32(FIFO_USER_03, REG_SCFG_EXT);

	// Signals for the hardware-info screen.
	// SCFG_OP (4004024h) is a DSi ARM7 register; bit0-1 = debugger type (0=retail). SCFG does
	// not exist on a plain DS, so read it only where SCFG is present and otherwise send FFh as a
	// "not applicable" sentinel. WiFi board (firmware 1FDh: 01h=DWM-W015, 02h=W024, 03h=W028;
	// FFh on the original DS) and the 48-bit WiFi MAC (firmware 036h) come from the wifi firmware
	// via the safe libnds readFirmware path.
	u8 scfgOp = 0xFF;
	if (isDSiMode() || REG_SCFG_EXT)
		scfgOp = *(vu16*)0x04004024 & 0x3;
	u8 wifiBoard = 0xFF;
	readFirmware(0x1FD, &wifiBoard, 1);
	u8 mac[6] = {0};
	readFirmware(0x36, mac, 6);
	fifoSendValue32(FIFO_USER_07, ((u32)scfgOp << 24) | ((u32)wifiBoard << 16) | (*(u16*)(0x4004700) & 0xFFFF));

	// DS Lite detection: the WiFi controller chip ID (W_ID, 4808000h) is 1440h on the original
	// DS and C340h on the DS Lite. It lives in WiFi silicon, so unlike the firmware console-type
	// byte (offset 1Dh) it can't be reflashed or spoofed. POWCNT2 (4000304h) bit1 gates the WiFi
	// port region 4800000h-480FFFFh, so enable it before reading the ID.
	*(volatile u16*)0x04000304 |= (1 << 1);
	for (volatile int i = 0; i < 0x4000; i++) { /* let the WiFi region settle */ }
	fifoSendValue32(FIFO_USER_08, *(volatile u16*)0x04808000);

	// WiFi MAC, queued after W_ID: bytes 0-3 then bytes 4-5 (little-endian in each word).
	fifoSendValue32(FIFO_USER_08, mac[0] | (mac[1] << 8) | (mac[2] << 16) | ((u32)mac[3] << 24));
	fifoSendValue32(FIFO_USER_08, mac[4] | (mac[5] << 8));

	// Firmware flash JEDEC ID, queued last on the channel (hardware-info screen only).
	fifoSendValue32(FIFO_USER_08, readFirmwareJEDEC());

	fifoSendValue32(FIFO_USER_06, 1);

	// Keep the ARM7 mostly idle
	while (!exitflag) {
		if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
			exitflag = true;
		}
		if (*(u32*)(0x2FFFD0C) == 0x454D4D43) {
			my_sdmmc_get_cid(true, (u32*)0x2FFD7BC);	// Get eMMC CID
			*(u32*)(0x2FFFD0C) = 0;
		}
		resyncClock();

		// Send SD status
		if(isDSiMode() || *(u16*)(0x4004700) != 0)
			fifoSendValue32(FIFO_USER_04, SD_IRQ_STATUS);

		// Dump EEPROM save
		if(fifoCheckAddress(FIFO_USER_01)) {
			switch(fifoGetValue32(FIFO_USER_01)) {
				case 0x44414552: // 'READ'
					readEeprom((u8 *)fifoGetAddress(FIFO_USER_01), fifoGetValue32(FIFO_USER_01), fifoGetValue32(FIFO_USER_01));
					break;
				case 0x54495257: // 'WRIT'
					writeEeprom(fifoGetValue32(FIFO_USER_01), (u8 *)fifoGetAddress(FIFO_USER_01), fifoGetValue32(FIFO_USER_01));
					break;
			}
		}

		swiWaitForVBlank();
	}
	return 0;
}


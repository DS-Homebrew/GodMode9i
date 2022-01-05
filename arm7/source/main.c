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
int main() {
//---------------------------------------------------------------------------------
	*(vu32*)0x400481C = 0;				// Clear SD IRQ stat register
	*(vu32*)0x4004820 = 0;				// Clear SD IRQ mask register

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
	if(isDSiMode() || REG_SCFG_EXT != 0) {
		u8 byteBak = my_i2cReadRegister(0x4A, 0x71);
		my_i2cWriteRegister(0x4A, 0x71, 0xD2);
		fifoSendValue32(FIFO_USER_05, my_i2cReadRegister(0x4A, 0x71));
		my_i2cWriteRegister(0x4A, 0x71, byteBak);
	}

	if (isDSiMode() /*|| ((REG_SCFG_EXT & BIT(17)) && (REG_SCFG_EXT & BIT(18)))*/) {
		/*for (int i = 0; i < 8; i++) {
			*(u8*)(0x2FFFD00+i) = *(u8*)(0x4004D07-i);	// Get ConsoleID
		}*/

		// For getting ConsoleID without reading from 0x4004D00...

		u8 base[16]={0};
		u8 in[16]={0};
		u8 iv[16]={0};
		u8 *scratch=(u8*)0x02300200; 
		u8 *out=(u8*)0x02300000;
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

	fifoSendValue32(FIFO_USER_03, REG_SCFG_EXT);
	fifoSendValue32(FIFO_USER_07, *(u16*)(0x4004700));
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


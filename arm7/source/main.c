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

//static u8 aesIvValues[0x10] = {0x80,0x6B,0xCF,0x4F,0x93,0xEE,0x6F,0x21,0xF9,0x86,0xDF,0x98,0x7D,0xE7,0xFD,0x07};

unsigned int * SCFG_EXT=(unsigned int*)0x4004008;

//---------------------------------------------------------------------------------
void ReturntoDSiMenu() {
//---------------------------------------------------------------------------------
	i2cWriteRegister(0x4A, 0x70, 0x01);		// Bootflag = Warmboot/SkipHealthSafety
	i2cWriteRegister(0x4A, 0x11, 0x01);		// Reset to DSi Menu
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

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
    nocashMessage("ARM7 main.c main");
	
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
	
	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT );

	setPowerButtonCB(powerButtonCB);

	for (int i = 0; i < 8; i++) {
		*(u8*)(0x2FFFD00+i) = *(u8*)(0x4004D07-i);	// Get ConsoleID
	}
	// Get ConsoleID
	/*for (int i = 0; i < 0x10; i++) {
		REG_AES_IV[i] = aesIvValues[i];
	}
	*(vu16*)0x4004406 = 1;*/

	/* *(u32*)(0x4004104+(0*0x1C)) = REG_AES_RDFIFO;
	*(u32*)(0x4004108+(0*0x1C)) = 0x2FFFD00;
	
	*(u32*)(0x4004110+(0*0x1C)) = 2;	
	
    *(u32*)(0x4004114+(0*0x1C)) = 0x1;
	
	*(u32*)(0x400411C+(0*0x1C)) = (1<<19 | 11<<28); */

	/*REG_AES_WRFIFO = 0xFFFFFFFF;
	REG_AES_WRFIFO = 0xEEEEEEEE;
	REG_AES_WRFIFO = 0xDDDDDDDD;
	REG_AES_WRFIFO = 0xCCCCCCCC;
	REG_AES_CNT = (AES_RDFIFO_FLUSH | AES_CNT_DMA_READ_SIZE(1) | AES_CNT_KEY_APPLY | AES_CNT_KEYSLOT(3) | AES_CNT_MODE(0) | AES_CNT_IRQ | AES_CNT_ENABLE);
	*/
	/* *(u32*)(0x2FFFD00) = REG_AES_RDFIFO;
	for (int i = 0; i < 3; i++) {
		*(u32*)(0x2FFFD04) = REG_AES_RDFIFO;
	}*/
	/*for (int i = 0; i < 4; i++) {
		*(u8*)(0x2FFFD04+i) = *(u8*)(0x40044EC+i);
	}*/

	fifoSendValue32(FIFO_USER_03, *SCFG_EXT);
	fifoSendValue32(FIFO_USER_07, *(u16*)(0x4004700));
	fifoSendValue32(FIFO_USER_06, 1);

	// Keep the ARM7 mostly idle
	while (!exitflag) {
		if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
			exitflag = true;
		}
		if (*(u32*)(0x2FFFD0C) == 0x454D4D43) {
			sdmmc_nand_cid((u32*)0x2FFD7BC);	// Get eMMC CID
			*(u32*)(0x2FFFD0C) = 0;
		}
		resyncClock();
		swiWaitForVBlank();
	}
	return 0;
}


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

unsigned int * SCFG_EXT=(unsigned int*)0x4004008;

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

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT );

	setPowerButtonCB(powerButtonCB);
	
	fifoSendValue32(FIFO_USER_03, *SCFG_EXT);
	fifoSendValue32(FIFO_USER_07, *(u16*)(0x4004700));
	fifoSendValue32(FIFO_USER_06, 1);

	// Keep the ARM7 mostly idle
	while (!exitflag) {

		swiIntrWait(1, IRQ_FIFO_NOT_EMPTY);

		if (fifoCheckValue32(FIFO_USER_01)) {
			int command = fifoGetValue32(FIFO_USER_01);

			switch (command) {
			case 1: {
				fifoSendValue32(FIFO_USER_01, sdmmc_nand_init());
			} break;
			case 4: {
				u32 nand_cid[4];
				sdmmc_nand_cid(nand_cid);
				fifoSendDatamsg(FIFO_USER_01, 16, (u8*)nand_cid);
			} break;
			case 5: {
				u64 consoleid;
				consoleid = REG_CONSOLEID;
				fifoSendDatamsg(FIFO_USER_01, 8, (u8*)&consoleid);
			} break;
			}
		}

		if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R))) {
			exitflag = true;
		}
		resyncClock();
		swiWaitForVBlank();
	}
	return 0;
}


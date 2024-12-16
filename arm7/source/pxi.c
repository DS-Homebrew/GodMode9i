#include "pxi.h"
#include "gba.h"
#include "my_i2c.h"

#include <nds.h>
#include <stdlib.h>

// Management structure and stack space for PXI server thread
Thread s_pxiThread;
alignas(8) u8 s_pxiThreadStack[1024];
Mailbox mb;
u32 mb_slots[4];

u32 *pxiData = NULL;

// void set_ctr(u32* ctr){
// 	for (int i = 0; i < 4; i++) REG_AES_IV.data[i] = ctr[3-i];
// }

// // 10 11 22 23 24 25
// void aes(void* in, void* out, void* iv, u32 method){ //this is sort of a bodged together dsi aes function adapted from this 3ds function
// 	REG_AES_CNT = ( AES_CNT_MODE(method) |           //https://github.com/TiniVi/AHPCFW/blob/master/source/aes.c#L42
// 					AES_WRFIFO_FLUSH |               //as long as the output changes when keyslot values change, it's good enough.
// 					AES_RDFIFO_FLUSH |
// 					AES_CNT_KEY_APPLY |
// 					AES_CNT_KEYSLOT(3) |
// 					AES_CNT_DMA_WRITE_SIZE(2) |
// 					AES_CNT_DMA_READ_SIZE(1)
// 					);

// 	if(iv != NULL) set_ctr((u32*)iv);
// 	REG_AES_BLKCNT = (1 << 16);
// 	REG_AES_CNT |= 0x80000000;
	
// 	for (int j = 0; j < 0x10; j+=4) REG_AES_WRFIFO = *((u32*)(in+j));
// 	while(((REG_AES_CNT >> 0x5) & 0x1F) < 0x4); //wait for every word to get processed
// 	for (int j = 0; j < 0x10; j+=4) *((u32*)(out+j)) = REG_AES_RDFIFO;
// 	//REG_AES_CNT &= ~0x80000000;
// 	//if(method & (AES_CTR_DECRYPT | AES_CTR_ENCRYPT)) add_ctr((u8*)iv);
// }

static void pxiHandler(void* user, u32 data)
{
	static unsigned data_words = 0, data_total = 0, header = 0;
	// static u32 header = 0;

	if_likely(data_words == 0) {
		if(pxiData)
			free(pxiData);

		data_words = data >> 26;
		data_total = data_words;

		pxiData = malloc(data_words * 4);
		if_likely(data_words == 0) {
			mailboxTrySend(&mb, data);
			return;
		}

		header = data & ((1U<<26)-1);
		return;
	}

	pxiData[data_total - (data_words--)] = data;
	if (data_words == 0) {
		mailboxTrySend(&mb, header);
	}
}

void getArm7Vars(u32 **vars) {
	*vars[0] = REG_SCFG_EXT;
	*vars[1] = REG_SNDEXCNT;

	// Check for 3DS
	if(isDSiMode() || (REG_SCFG_EXT & BIT(22))) {
		u8 byteBak = my_i2cReadRegister8(0x4A, 0x71);
		my_i2cWriteRegister8(0x4A, 0x71, 0xD2);
		*vars[2] = my_i2cReadRegister8(0x4A, 0x71) == 0xD2;
		my_i2cWriteRegister8(0x4A, 0x71, byteBak);
	}

	/* TODO: Was used by nandio, remove or use
	if(isDSiMode() || ((REG_SCFG_EXT & BIT(17)) && (REG_SCFG_EXT & BIT(18)))) {
		u8 *out=(u8*)0x02F00000;
		memset(out, 0, 16);

		// first check whether we can read the console ID directly and it was not hidden by SCFG
		if(((*(vu16*)0x04004000) & (1u << 10)) == 0 && ((*(vu8*)0x04004D08) & 0x1) == 0) {
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
	*/
}

//---------------------------------------------------------------------------------
int pxiThreadMain(void* arg) {
//---------------------------------------------------------------------------------
	// Set up PXI mailbox, used to receive PXI command words
	mailboxPrepare(&mb, mb_slots, sizeof(mb_slots)/4);
	pxiSetHandler(PXI_MAIN, pxiHandler, NULL);

	// Main PXI message loop
	for (;;) {
		// Receive a message
		u32 msg = mailboxRecv(&mb);
		u32 retval = 0;

		switch (msg) {
			case GET_ARM7_VARS:
				getArm7Vars((u32 **)pxiData);
				retval = 77;
				break;

			case GBA_READ_EEPROM:
				if(pxiData)
					retval = gbaReadEeprom((u8 *)pxiData[0], pxiData[1], pxiData[2]);
				break;

			case GBA_WRITE_EEPROM:
				if(pxiData)
					retval = gbaWriteEeprom(pxiData[0], (u8 *)pxiData[1], pxiData[2]);
				break;
				
			default:
				break;
		}

		// Send a reply back to the ARM9
		pxiReply(PXI_MAIN, retval);
	}

	return 0;
}

// void powerValueHandler(u32 value, void* user_data);
// void my_sdmmcMsgHandler(int bytes, void *user_data);
// void my_sdmmcValueHandler(u32 value, void* user_data);
// void firmwareMsgHandler(int bytes, void *user_data);

// //---------------------------------------------------------------------------------
// void my_installSystemFIFO(void) {
// //---------------------------------------------------------------------------------

// 	fifoSetValue32Handler(FIFO_PM, powerValueHandler, 0);
// 	//if (isDSiMode() || (REG_SCFG_EXT & BIT(18))) {
// 		fifoSetValue32Handler(FIFO_SDMMC, my_sdmmcValueHandler, 0);
// 		fifoSetDatamsgHandler(FIFO_SDMMC, my_sdmmcMsgHandler, 0);
// 	//}
// 	fifoSetDatamsgHandler(FIFO_FIRMWARE, firmwareMsgHandler, 0);
	
// }



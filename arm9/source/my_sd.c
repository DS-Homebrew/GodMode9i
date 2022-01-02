#include "my_sd.h"

#include <nds/fifocommon.h>
#include <nds/fifomessages.h>
#include <nds/system.h>
#include <nds/arm9/cache.h>

volatile bool sdRemoved = false;
volatile bool sdWriteLocked = false;

void sdStatusHandler(u32 sdIrqStatus, void *userdata) {
	sdRemoved = (sdIrqStatus & BIT(5)) == 0;
	sdWriteLocked = (sdIrqStatus & BIT(7)) == 0;
}

//---------------------------------------------------------------------------------
bool my_sdio_Startup() {
//---------------------------------------------------------------------------------
	fifoSendValue32(FIFO_SDMMC,SDMMC_HAVE_SD);
	while(!fifoCheckValue32(FIFO_SDMMC));
	int result = fifoGetValue32(FIFO_SDMMC);

	if(result==0) return false;

	fifoSendValue32(FIFO_SDMMC,SDMMC_SD_START);

	fifoWaitValue32(FIFO_SDMMC);

	result = fifoGetValue32(FIFO_SDMMC);

	return result == 0;
}

//---------------------------------------------------------------------------------
bool my_sdio_IsInserted() {
//---------------------------------------------------------------------------------
	fifoSendValue32(FIFO_SDMMC,SDMMC_SD_IS_INSERTED);

	fifoWaitValue32(FIFO_SDMMC);

	int result = fifoGetValue32(FIFO_SDMMC);

	return result == 1;
}

//---------------------------------------------------------------------------------
bool my_sdio_ReadSectors(sec_t sector, sec_t numSectors,void* buffer) {
//---------------------------------------------------------------------------------
	FifoMessage msg;

	DC_FlushRange(buffer,numSectors * 512);

	msg.type = SDMMC_SD_READ_SECTORS;
	msg.sdParams.startsector = sector;
	msg.sdParams.numsectors = numSectors;
	msg.sdParams.buffer = buffer;
	
	fifoSendDatamsg(FIFO_SDMMC, sizeof(msg), (u8*)&msg);

	fifoWaitValue32(FIFO_SDMMC);

	int result = fifoGetValue32(FIFO_SDMMC);
	
	return result == 0;
}

//---------------------------------------------------------------------------------
bool my_sdio_WriteSectors(sec_t sector, sec_t numSectors,const void* buffer) {
//---------------------------------------------------------------------------------
	if(sdWriteLocked)
		return false;

	FifoMessage msg;

	DC_FlushRange(buffer,numSectors * 512);

	msg.type = SDMMC_SD_WRITE_SECTORS;
	msg.sdParams.startsector = sector;
	msg.sdParams.numsectors = numSectors;
	msg.sdParams.buffer = (void*)buffer;
	
	fifoSendDatamsg(FIFO_SDMMC, sizeof(msg), (u8*)&msg);

	fifoWaitValue32(FIFO_SDMMC);

	int result = fifoGetValue32(FIFO_SDMMC);
	
	return result == 0;
}


//---------------------------------------------------------------------------------
bool my_sdio_ClearStatus() {
//---------------------------------------------------------------------------------
	return true;
}

//---------------------------------------------------------------------------------
bool my_sdio_Shutdown() {
//---------------------------------------------------------------------------------
	fifoSendValue32(FIFO_SDMMC,SDMMC_SD_STOP);

	fifoWaitValue32(FIFO_SDMMC);

	int result = fifoGetValue32(FIFO_SDMMC);

	return result == 1;
}

const DISC_INTERFACE __my_io_dsisd_rw = {
	DEVICE_TYPE_DSI_SD,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
	(FN_MEDIUM_STARTUP)&my_sdio_Startup,
	(FN_MEDIUM_ISINSERTED)&my_sdio_IsInserted,
	(FN_MEDIUM_READSECTORS)&my_sdio_ReadSectors,
	(FN_MEDIUM_WRITESECTORS)&my_sdio_WriteSectors,
	(FN_MEDIUM_CLEARSTATUS)&my_sdio_ClearStatus,
	(FN_MEDIUM_SHUTDOWN)&my_sdio_Shutdown
};

const DISC_INTERFACE __my_io_dsisd_r = {
	DEVICE_TYPE_DSI_SD,
	FEATURE_MEDIUM_CANREAD,
	(FN_MEDIUM_STARTUP)&my_sdio_Startup,
	(FN_MEDIUM_ISINSERTED)&my_sdio_IsInserted,
	(FN_MEDIUM_READSECTORS)&my_sdio_ReadSectors,
	(FN_MEDIUM_WRITESECTORS)&my_sdio_WriteSectors,
	(FN_MEDIUM_CLEARSTATUS)&my_sdio_ClearStatus,
	(FN_MEDIUM_SHUTDOWN)&my_sdio_Shutdown
};

const DISC_INTERFACE *__my_io_dsisd() {
	return sdWriteLocked ? &__my_io_dsisd_r : &__my_io_dsisd_rw;
}

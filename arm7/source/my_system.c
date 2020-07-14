/*---------------------------------------------------------------------------------

  Copyright (C) 2005 - 2010
    Michael Noland (joat)
	Jason Rogers (Dovoto)
	Dave Murphy (WinterMute)

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any
  damages arising from the use of this software.

  Permission is granted to anyone to use this software for any
  purpose, including commercial applications, and to alter it and
  redistribute it freely, subject to the following restrictions:

  1.  The origin of this software must not be misrepresented; you
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.
  2.  Altered source versions must be plainly marked as such, and
      must not be misrepresented as being the original software.
  3.  This notice may not be removed or altered from any source
      distribution.

---------------------------------------------------------------------------------*/
#include <nds/ndstypes.h>
#include <nds/system.h>
#include <nds/fifocommon.h>
#include <nds/fifomessages.h>
#include <nds/interrupts.h>
#include <nds/bios.h>
#include <nds/arm7/clock.h>
#include <nds/arm7/i2c.h>

void powerValueHandler(u32 value, void* user_data);
void my_sdmmcMsgHandler(int bytes, void *user_data);
void my_sdmmcValueHandler(u32 value, void* user_data);
void firmwareMsgHandler(int bytes, void *user_data);

//---------------------------------------------------------------------------------
void my_installSystemFIFO(void) {
//---------------------------------------------------------------------------------

	fifoSetValue32Handler(FIFO_PM, powerValueHandler, 0);
	//if (isDSiMode()) {
		fifoSetValue32Handler(FIFO_SDMMC, my_sdmmcValueHandler, 0);
		fifoSetDatamsgHandler(FIFO_SDMMC, my_sdmmcMsgHandler, 0);
	//}
	fifoSetDatamsgHandler(FIFO_FIRMWARE, firmwareMsgHandler, 0);
	
}



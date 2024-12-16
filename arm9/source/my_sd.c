#include "my_sd.h"

#include <nds.h>

volatile bool sdRemoved = false;
volatile bool sdWriteLocked = false;

void sdStatusHandler(u32 sdIrqStatus, void *userdata) {
	sdRemoved = (sdIrqStatus & BIT(5)) == 0;
	sdWriteLocked = (sdIrqStatus & BIT(7)) == 0;
}

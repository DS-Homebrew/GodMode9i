// SPDX-License-Identifier: ZPL-2.1
// SPDX-FileCopyrightText: Copyright fincs, devkitPro
#include <calico/system/mutex.h>
#include <calico/nds/bios.h>
#include <calico/nds/arm7/i2c.h>

static Mutex my_i2cMutex;
static s32 my_i2cDelay, my_i2cMcuDelay;

MK_INLINE void _my_i2cWaitBusy(void)
{
	while (REG_I2C_CNT & 0x80);
}

MK_INLINE void _my_i2cSetDelay(I2cDevice dev)
{
	if (dev == I2cDev_MCU) {
		my_i2cDelay = my_i2cMcuDelay;
	} else {
		my_i2cDelay = 0;
	}
}

void _my_i2cSetMcuDelay(s32 delay)
{
	my_i2cMcuDelay = delay;
}

MK_INLINE void _my_i2cDelay()
{
	_my_i2cWaitBusy();
	if (my_i2cDelay > 0) {
		svcWaitByLoop(my_i2cDelay);
		// !!! WARNING !!! Calling a BIOS function from within IRQ mode
		// will use up 4 words of SVC and 2 words of SYS stack from the **CURRENT** thread,
		// which may be the idle thread; which does NOT have enough space for nested syscall
		// frames!
		// For now, and because I am lazy, let's do this in shittily written C.
		//for (volatile s32 i = my_i2cDelay; i; i --);
	}
}

static void _my_i2cStop(u8 arg0)
{
	if (my_i2cDelay) {
		REG_I2C_CNT = (arg0 << 5) | 0xC0;
		_my_i2cDelay();
		REG_I2C_CNT = 0xC5;
	} else {
		REG_I2C_CNT = (arg0 << 5) | 0xC1;
	}
}

MK_INLINE bool _my_i2cGetResult()
{
	_my_i2cWaitBusy();
	return (REG_I2C_CNT >> 4) & 0x01;
}

MK_INLINE u8 _my_i2cGetData()
{
	_my_i2cWaitBusy();
	return REG_I2C_DATA;
}

static bool _my_i2cSelectDevice(I2cDevice dev)
{
	_my_i2cWaitBusy();
	REG_I2C_DATA = dev;
	REG_I2C_CNT = 0xC2;
	return _my_i2cGetResult();
}

static bool _my_i2cSelectRegister(u8 reg)
{
	_my_i2cDelay();
	REG_I2C_DATA = reg;
	REG_I2C_CNT = 0xC0;
	return _my_i2cGetResult();
}

bool my_i2cWriteRegister8(I2cDevice dev, u8 reg, u8 data)
{
	if_unlikely (!mutexIsLockedByCurrentThread(&my_i2cMutex)) {
		return false;
	}

	_my_i2cSetDelay(dev);
	for (unsigned i = 0; i < 8; i ++) {
		if (_my_i2cSelectDevice(dev) && _my_i2cSelectRegister(reg)) {
			_my_i2cDelay();
			REG_I2C_DATA = data;
			_my_i2cStop(0);

			if (_my_i2cGetResult()) {
				return true;
			}
		}
		REG_I2C_CNT = 0xC5;
	}

	return false;
}

u8 my_i2cReadRegister8(I2cDevice dev, u8 reg)
{
	if_unlikely (!mutexIsLockedByCurrentThread(&my_i2cMutex)) {
		return 0xff;
	}

	_my_i2cSetDelay(dev);
	for (unsigned i = 0; i < 8; i++) {
		if (_my_i2cSelectDevice(dev) && _my_i2cSelectRegister(reg)) {
			_my_i2cDelay();
			if (_my_i2cSelectDevice(dev | 1)) {
				_my_i2cDelay();
				_my_i2cStop(1);
				return _my_i2cGetData();
			}
		}

		REG_I2C_CNT = 0xC5;
	}

	return 0xff;
}

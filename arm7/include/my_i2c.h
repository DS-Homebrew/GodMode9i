#define MY_I2C_H
#ifdef MY_I2C_H

#include <nds/ndstypes.h>

u8 my_i2cReadRegister8(u8 device, u8 reg);
u8 my_i2cWriteRegister8(u8 device, u8 reg, u8 data);

#endif // MY_I2C_H

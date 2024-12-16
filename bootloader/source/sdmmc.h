#ifndef __SDMMC_H__
#define __SDMMC_H__

#include <nds/ndstypes.h>

#define DATA32_SUPPORT

#define SDMMC_BASE	0x04004800


#define REG_SDCMD       0x00
#define REG_SDPORTSEL   0x02
#define REG_SDCMDARG    0x04
#define REG_SDCMDARG0   0x04
#define REG_SDCMDARG1  	0x06
#define REG_SDSTOP      0x08
#define REG_SDRESP      0x0c
#define REG_SDBLKCOUNT  0x0a

#define REG_SDRESP0     0x0c
#define REG_SDRESP1     0x0e
#define REG_SDRESP2     0x10
#define REG_SDRESP3     0x12
#define REG_SDRESP4     0x14
#define REG_SDRESP5     0x16
#define REG_SDRESP6     0x18
#define REG_SDRESP7     0x1a

#define REG_SDSTATUS0   0x1c
#define REG_SDSTATUS1   0x1e

#define REG_SDIRMASK0   0x20
#define REG_SDIRMASK1   0x22
#define REG_SDCLKCTL    0x24

#define REG_SDBLKLEN    0x26
#define REG_SDOPT       0x28
#define REG_SDFIFO      0x30

#define REG_SDDATACTL   0xd8
#define REG_SDRESET     0xe0
#define REG_SDPROTECTED 0xf6 //bit 0 determines if sd is protected or not?

#define REG_SDDATACTL32         0x100
#define REG_SDBLKLEN32          0x104
#define REG_SDBLKCOUNT32        0x108
#define REG_SDFIFO32            0x10C

#define REG_CLK_AND_WAIT_CTL    0x138
#define REG_RESET_SDIO          0x1e0
//The below defines are from linux kernel drivers/mmc tmio_mmc.h.
/* Definitions for values the CTRL_STATUS register can take. */
#define TMIO_STAT0_CMDRESPEND    0x0001
#define TMIO_STAT0_DATAEND       0x0004
#define TMIO_STAT0_CARD_REMOVE   0x0008
#define TMIO_STAT0_CARD_INSERT   0x0010
#define TMIO_STAT0_SIGSTATE      0x0020
#define TMIO_STAT0_WRPROTECT     0x0080
#define TMIO_STAT0_CARD_REMOVE_A 0x0100
#define TMIO_STAT0_CARD_INSERT_A 0x0200
#define TMIO_STAT0_SIGSTATE_A    0x0400

#define TMIO_STAT1_CMD_IDX_ERR   0x0001
#define TMIO_STAT1_CRCFAIL       0x0002
#define TMIO_STAT1_STOPBIT_ERR   0x0004
#define TMIO_STAT1_DATATIMEOUT   0x0008
#define TMIO_STAT1_RXOVERFLOW    0x0010
#define TMIO_STAT1_TXUNDERRUN    0x0020
#define TMIO_STAT1_CMDTIMEOUT    0x0040
#define TMIO_STAT1_RXRDY         0x0100
#define TMIO_STAT1_TXRQ          0x0200
#define TMIO_STAT1_ILL_FUNC      0x2000
#define TMIO_STAT1_CMD_BUSY      0x4000
#define TMIO_STAT1_ILL_ACCESS    0x8000

#define SDMC_NORMAL              0x00000000
#define SDMC_ERR_COMMAND         0x00000001
#define SDMC_ERR_CRC             0x00000002
#define SDMC_ERR_END             0x00000004
#define SDMC_ERR_TIMEOUT         0x00000008
#define SDMC_ERR_FIFO_OVF        0x00000010
#define SDMC_ERR_FIFO_UDF        0x00000020
#define SDMC_ERR_WP              0x00000040
#define SDMC_ERR_ABORT           0x00000080
#define SDMC_ERR_FPGA_TIMEOUT    0x00000100
#define SDMC_ERR_PARAM           0x00000200
#define SDMC_ERR_R1_STATUS       0x00000800
#define SDMC_ERR_NUM_WR_SECTORS  0x00001000
#define SDMC_ERR_RESET           0x00002000
#define SDMC_ERR_ILA             0x00004000
#define SDMC_ERR_INFO_DETECT     0x00008000

#define SDMC_STAT_ERR_UNKNOWN    0x00080000
#define SDMC_STAT_ERR_CC         0x00100000
#define SDMC_STAT_ERR_ECC_FAILED 0x00200000
#define SDMC_STAT_ERR_CRC        0x00800000
#define SDMC_STAT_ERR_OTHER      0xf9c70008

#define TMIO_MASK_ALL           0x837f031d

#define TMIO_MASK_GW      (TMIO_STAT1_ILL_ACCESS | TMIO_STAT1_CMDTIMEOUT | TMIO_STAT1_TXUNDERRUN | TMIO_STAT1_RXOVERFLOW | \
                           TMIO_STAT1_DATATIMEOUT | TMIO_STAT1_STOPBIT_ERR | TMIO_STAT1_CRCFAIL | TMIO_STAT1_CMD_IDX_ERR)

#define TMIO_MASK_READOP  (TMIO_STAT1_RXRDY | TMIO_STAT1_DATAEND)
#define TMIO_MASK_WRITEOP (TMIO_STAT1_TXRQ | TMIO_STAT1_DATAEND)

typedef struct mmcdevice {
    u8* data;
    u32 size;
    u32 error;
    u16 stat0;
    u16 stat1;
    u32 ret[4];
    u32 initarg;
    u32 isSDHC;
    u32 clk;
    u32 SDOPT;
    u32 devicenumber;
    u32 total_size; //size in sectors of the device
    u32 res;
} mmcdevice;

enum {
    MMC_DEVICE_SDCARD,
    MMC_DEVICE_NAND,
};

void sdmmc_controller_init(bool force_init);
void sdmmc_initirq();
int sdmmc_cardinserted();

int sdmmc_sdcard_init();
int sdmmc_nand_init();
void sdmmc_get_cid(int devicenumber, u32 *cid);

static inline void sdmmc_nand_cid( u32 *cid) {
    sdmmc_get_cid(MMC_DEVICE_NAND,cid);
}

static inline void sdmmc_sdcard_cid( u32 *cid) {
    sdmmc_get_cid(MMC_DEVICE_SDCARD,cid);
}

bool sdmmc_sdcard_readsectors(u32 sector_no, u32 numsectors, void *out);
/*
int sdmmc_sdcard_writesectors(u32 sector_no, u32 numsectors, void *in);
int sdmmc_nand_readsectors(u32 sector_no, u32 numsectors, void *out);
int sdmmc_nand_writesectors(u32 sector_no, u32 numsectors, void *in);
*/

extern u32 sdmmc_cid[];
extern int sdmmc_curdevice;

//---------------------------------------------------------------------------------
static inline u16 sdmmc_read16(u16 reg) {
//---------------------------------------------------------------------------------
	return *(vu16*)(SDMMC_BASE + reg);
}

//---------------------------------------------------------------------------------
static inline void sdmmc_write16(u16 reg, u16 val) {
//---------------------------------------------------------------------------------
	*(vu16*)(SDMMC_BASE + reg) = val;
}

//---------------------------------------------------------------------------------
static inline u32 sdmmc_read32(u16 reg) {
//---------------------------------------------------------------------------------
    return *(vu32*)(SDMMC_BASE + reg);
}

//---------------------------------------------------------------------------------
static inline void sdmmc_write32(u16 reg, u32 val) {
//---------------------------------------------------------------------------------
    *(vu32*)(SDMMC_BASE + reg) = val;
}

//---------------------------------------------------------------------------------
static inline void sdmmc_mask16(u16 reg, u16 clear, u16 set) {
//---------------------------------------------------------------------------------
	u16 val = sdmmc_read16(reg);
	val &= ~clear;
	val |= set;
	sdmmc_write16(reg, val);
}


//---------------------------------------------------------------------------------
static inline void setckl(u32 data) {
//---------------------------------------------------------------------------------
    sdmmc_mask16(REG_SDCLKCTL, 0x100, 0);
    sdmmc_mask16(REG_SDCLKCTL, 0x2FF, data & 0x2FF);
    sdmmc_mask16(REG_SDCLKCTL, 0x0, 0x100);
}

#endif

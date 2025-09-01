#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
.SECONDARY:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

#---------------------------------------------------------------------------------
# External tools
#---------------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
MAKECIA 	?= make_cia.exe

else
MAKECIA 	?= make_cia

endif
#---------------------------------------------------------------------------------

export TARGET := GodMode9i

export GAME_TITLE := $(TARGET)

export NITRODATA := nitrofiles

.PHONY: all bootloader bootstub clean arm7/$(TARGET).elf arm9/$(TARGET).elf

all:	libfat4 bootloader bootstub $(TARGET).nds $(TARGET).dsi

$(TARGET).nds:	arm7/$(TARGET).elf arm9/$(TARGET).elf
	ndstool	-c $(TARGET).nds -7 arm7/$(TARGET).elf -9 arm9/$(TARGET).elf -d $(NITRODATA) \
			-b icon.bmp "GodMode9i;Rocket Robz" \
			-z 80040000

$(TARGET).dsi:	arm7/$(TARGET).elf arm9/$(TARGET).elf
	ndstool	-c $(TARGET).dsi -7 arm7/$(TARGET).elf -9 arm9/$(TARGET).elf -d $(NITRODATA) \
			-b icon.bmp "GodMode9i;Rocket Robz" \
			-g HGMA 00 "GODMODE9I" -z 80040000 -u 00030004

	@$(TOPDIR)/$(MAKECIA) --srl=$(TARGET).dsi

#---------------------------------------------------------------------------------
arm7/$(TARGET).elf:
	@$(MAKE) -C arm7

#---------------------------------------------------------------------------------
arm9/$(TARGET).elf:
	@$(MAKE) -C arm9

#---------------------------------------------------------------------------------
#$(BUILD):
	#@[ -d $@ ] || mkdir -p $@
	#@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr data/*.bin
	@$(MAKE) -C libs/libfat4 clean
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).dsi $(TARGET).cia
	@rm -fr $(TARGET).arm7.elf
	@rm -fr $(TARGET).arm9.elf
	@$(MAKE) -C bootloader clean
	@$(MAKE) -C bootstub clean
	@$(MAKE) -C arm9 clean
	@$(MAKE) -C arm7 clean

libfat4:
	$(MAKE) -C libs/libfat4

bootloader: data
	@$(MAKE) -C bootloader LOADBIN=$(CURDIR)/data/load.bin

bootstub: data
	@$(MAKE) -C bootstub

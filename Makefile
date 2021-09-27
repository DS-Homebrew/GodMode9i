#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
.SECONDARY:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

export TARGET := GodMode9i

export GAME_TITLE := $(TARGET)

.PHONY: all bootloader bootstub clean dsi arm7/$(TARGET).elf arm9/$(TARGET).elf

all:	bootloader bootstub $(TARGET).nds

dsi:	$(TARGET).dsi

$(TARGET).nds:	arm7/$(TARGET).elf arm9/$(TARGET).elf
	ndstool	-c $(TARGET).nds -7 arm7/$(TARGET).elf -9 arm9/$(TARGET).elf \
			-b icon.bmp "GodMode9i;RocketRobz" \
			-z 80040000 -u 00030004
	python fix_ndsheader.py $(CURDIR)/$(TARGET).nds

$(TARGET).dsi:	arm7/$(TARGET).elf arm9/$(TARGET).elf
	ndstool	-c $(TARGET).dsi -7 arm7/$(TARGET).elf -9 arm9/$(TARGET).elf \
			-b icon.bmp "GodMode9i;RocketRobz" \
			-g HGMA 00 "GODMODE9I" -z 80040000 -u 00030004
	python fix_ndsheader.py $(CURDIR)/$(TARGET).dsi

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
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds
	@rm -fr $(TARGET).arm7.elf
	@rm -fr $(TARGET).arm9.elf
	@$(MAKE) -C bootloader clean
	@$(MAKE) -C bootstub clean
	@$(MAKE) -C arm9 clean
	@$(MAKE) -C arm7 clean

bootloader: data
	@$(MAKE) -C bootloader LOADBIN=$(CURDIR)/data/load.bin

bootstub: data
	@$(MAKE) -C bootstub

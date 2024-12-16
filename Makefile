#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

export TARGET := GodMode9i
export TOPDIR := $(CURDIR)

# specify a directory which contains the nitro filesystem
# this is relative to the Makefile
NITRO_FILES := nitrofiles

# These set the information text in the nds file
GAME_TITLE        := GodMode9i
GAME_SUBTITLE     :=
GAME_AUTHOR       := Rocket Robz
GAME_CODE         := HGMA
GAME_HEADER_TITLE := GODMODE9I

GAME_ICON     := icon.bmp

ifeq ($(strip $(GAME_SUBTITLE)),)
    GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_AUTHOR)
else
    GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_SUBTITLE);$(GAME_AUTHOR)
endif

include $(DEVKITARM)/ds_rules

.PHONY: arm7/$(TARGET).elf bootloader bootstub clean

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all: $(TARGET).nds $(TARGET).dsi

#---------------------------------------------------------------------------------
$(TARGET).nds : $(NITRO_FILES) arm7/$(TARGET).elf arm9/$(TARGET).elf
	ndstool	-c $(TARGET).nds -7 arm7/$(TARGET).elf -9 arm9/$(TARGET).elf \
			-b $(GAME_ICON) "$(GAME_FULL_TITLE)" -z 80040000 \
			$(_ADDFILES)

$(TARGET).dsi : $(NITRO_FILES)  arm7/$(TARGET).elf arm9/$(TARGET).elf
	ndstool	-c $(TARGET).dsi -7 arm7/$(TARGET).elf -9 arm9/$(TARGET).elf \
			-b $(GAME_ICON) "$(GAME_FULL_TITLE)" \
			-g $(GAME_CODE) 00 "$(GAME_HEADER_TITLE)" -z 80040000 -u 00030004 \
			$(_ADDFILES)

#---------------------------------------------------------------------------------
arm7/$(TARGET).elf:
	$(MAKE) -C arm7

#---------------------------------------------------------------------------------
arm9/$(TARGET).elf: bootloader bootstub
	$(MAKE) -C arm9

bootloader: data
	@$(MAKE) -C bootloader LOADBIN=$(CURDIR)/data/load.bin

bootstub: data
	@$(MAKE) -C bootstub

clean:
	@rm -fr data/*.bin
	@rm -fr $(TARGET).nds
	@$(MAKE) -C bootloader clean
	@$(MAKE) -C bootstub clean
	@$(MAKE) -C arm9 clean
	@$(MAKE) -C arm7 clean

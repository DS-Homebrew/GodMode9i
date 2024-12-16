#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

# If on a tagged commit, use just tag
ifneq ($(shell echo $(shell git tag -l --points-at HEAD) | head -c 1),)
GIT_VER := $(shell git tag -l --points-at HEAD)
else
GIT_VER := $(shell git describe --abbrev=0 --tags)-$(shell git rev-parse --short=7 HEAD)
endif

# Ensure version.h exists
ifeq (,$(wildcard include/version.h))
$(shell mkdir -p include)
$(shell touch include/version.h)
endif

# Print new version if changed
ifeq (,$(findstring $(GIT_VER), $(shell cat include/version.h)))
$(shell printf "#ifndef VERSION_HPP\n#define VERSION_HPP\n\n#define VER_NUMBER \"$(GIT_VER)\"\n\n#endif\n" > include/version.h)
endif

#---------------------------------------------------------------------------------
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories containing binary files embedded using bin2o
# GRAPHICS is a list of directories containing image files to be converted with grit
# all directories are relative to this makefile
#---------------------------------------------------------------------------------
BUILD    := build
SOURCES  := source source/flashcard source/graphics dldi-include
INCLUDES := include dldi-include source source/flashcard source/graphics ../shared/include
DATA     := ../data
GRAPHICS := ../gfx

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH := -march=armv5te -mtune=arm946e-s

CFLAGS   := -g -Wall -O2 -ffunction-sections -fdata-sections\
            $(ARCH) $(INCLUDE) -DARM9
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++20
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS := -lfat -lnds9

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS := $(LIBNDS) $(PORTLIBS)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export ARM9ELF := $(CURDIR)/$(TARGET).elf

export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))\
                $(foreach dir,$(DATA),$(CURDIR)/$(dir))\
                $(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PNGFILES := $(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.png)))
BMPFILES :=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.bmp)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
  export LD := $(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
  export LD := $(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN   :=	$(addsuffix .o,$(BINFILES))

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES := $(PNGFILES:.png=.o) $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(PNGFILES:.png=.h) $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE  := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir))\
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include)\
                   -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds *.bin include/version.h


#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(ARM9ELF) : $(OFILES)

#---------------------------------------------------------------------------------
%.bin.o %_bin.h : %.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
%.frf.o	:	%.frf
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

#---------------------------------------------------------------------------------
# This rule creates assembly source files using grit
# grit takes an image file and a .grit describing how the file is to be processed
# add additional rules like this for each image extension
# you use in the graphics folders
#---------------------------------------------------------------------------------
%.s %.h	: %.bmp %.grit
#---------------------------------------------------------------------------------
	grit $< -fts -o$*

#---------------------------------------------------------------------------------
%.s %.h   : %.png %.grit
#---------------------------------------------------------------------------------
	grit $< -fts -o$*

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------

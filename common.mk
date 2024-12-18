# #############################################################################
# Common Oscillator Makefile
# #############################################################################

SDKDIR ?= ../logue-sdk

PROJECTDIR ?= .

INSTALLDIR ?= ..

TOOLSDIR ?= $(SDKDIR)/tools

EXTDIR ?= $(SDKDIR)/platform/ext

LDDIR ?= $(SDKDIR)/platform/ld

ZIP ?= /usr/bin/zip
ZIP_ARGS := -r -m -q

# #############################################################################
# configure cross compilation
# #############################################################################

MCU ?= cortex-m4

#MCU_MODEL ?= STM32F401xC

GCC_TARGET := arm-none-eabi-
GCC_BIN_PATH ?= $(TOOLSDIR)/gcc/gcc-arm-none-eabi-10-2020-q4-major/bin

CC   := $(GCC_BIN_PATH)/$(GCC_TARGET)gcc
CXXC := $(GCC_BIN_PATH)/$(GCC_TARGET)g++
LD   := $(GCC_BIN_PATH)/$(GCC_TARGET)gcc
#LD  := $(GCC_BIN_PATH)/$(GCC_TARGET)g++
CP   := $(GCC_BIN_PATH)/$(GCC_TARGET)objcopy
AS   := $(GCC_BIN_PATH)/$(GCC_TARGET)gcc -x assembler-with-cpp
AR   := $(GCC_BIN_PATH)/$(GCC_TARGET)ar
OD   := $(GCC_BIN_PATH)/$(GCC_TARGET)objdump
SZ   := $(GCC_BIN_PATH)/$(GCC_TARGET)size

HEX  := $(CP) -O ihex
BIN  := $(CP) -O binary

RULESPATH := $(LDDIR)
LDSCRIPT := $(LDDIR)/userosc.ld
DLIBS := -lm

DADEFS := -D$(MCU_MODEL) -DUSER_TARGET_PLATFORM=$(USER_TARGET_PLATFORM) -DCORTEX_USE_FPU=TRUE -DARM_MATH_CM4
DDEFS := -D$(MCU_MODEL) -DUSER_TARGET_PLATFORM=$(USER_TARGET_PLATFORM) -DCORTEX_USE_FPU=TRUE -DARM_MATH_CM4 -D__FPU_PRESENT

COPT := -std=c11
CXXOPT := -std=c++11 -fno-rtti -fno-exceptions -fno-non-call-exceptions

LDOPT := -Xlinker --just-symbols=$(LDDIR)/osc_api.syms

CWARN := -W -Wall -Wextra -Wdouble-promotion
CXXWARN := $(CWARN)
# -Wconversion generates a lot of warnings in logue sdk headers
#CWARN += -Wconversion
#CXXWARN += -Wconversion

FPU_OPTS := -mfloat-abi=hard -mfpu=fpv4-sp-d16 -fsingle-precision-constant -fcheck-new

OPT := -O3 -mlittle-endian 
OPT += $(FPU_OPTS)

# Warning: -ffast-math may generate numerical errors!
# see: https://kristerw.github.io/2021/10/19/fast-math/
OPT += -ffast-math

TOPT := -mthumb -mno-thumb-interwork -DTHUMB_NO_INTERWORKING -DTHUMB_PRESENT


# #############################################################################
# set targets and directories
# #############################################################################

MANIFEST := manifest.json
PAYLOAD := payload.bin

BUILDDIR := ./build
OBJDIR := $(BUILDDIR)/obj
LSTDIR := $(BUILDDIR)/lst

ASMSRC := $(UASMSRC)

ASMXSRC := $(UASMXSRC)

CSRC := $(SDKDIR)/platform/tpl/_unit.c $(UCSRC)

CXXSRC := $(UCXXSRC)

vpath %.s $(sort $(dir $(ASMSRC)))
vpath %.S $(sort $(dir $(ASMXSRC)))
vpath %.c $(sort $(dir $(CSRC)))
vpath %.cpp $(sort $(dir $(CXXSRC)))

ASMOBJS := $(addprefix $(OBJDIR)/, $(notdir $(ASMSRC:.s=.o)))
ASMXOBJS := $(addprefix $(OBJDIR)/, $(notdir $(ASMXSRC:.S=.o)))
COBJS := $(addprefix $(OBJDIR)/, $(notdir $(CSRC:.c=.o)))
CXXOBJS := $(addprefix $(OBJDIR)/, $(notdir $(CXXSRC:.cpp=.o)))

OBJS := $(ASMXOBJS) $(ASMOBJS) $(COBJS) $(CXXOBJS)

SDKDIR ?= ../logue-sdk

DINCDIR := $(SDKDIR)/platform/inc \
	$(SDKDIR)/platform/inc/dsp \
	$(SDKDIR)/platform/inc/utils \
	$(SDKDIR)/platform/inc/CMSIS

INCDIR := $(patsubst %,-I%,$(DINCDIR) $(UINCDIR))

DEFS := $(DDEFS) $(UDEFS)
ADEFS := $(DADEFS) $(UADEFS)

LIBS := $(DLIBS) $(ULIBS)

LIBDIR := $(patsubst %,-I%,$(DLIBDIR) $(ULIBDIR))

HEADERDEPS = $(wildcard ../src/*.h)

# #############################################################################
# compiler flags
# #############################################################################

MCFLAGS   := -mcpu=$(MCU)
ODFLAGS	  := -x --syms
ASFLAGS   = $(MCFLAGS) -g $(TOPT) -Wa,-alms=$(LSTDIR)/$(notdir $(<:.s=.lst)) $(ADEFS)
ASXFLAGS  = $(MCFLAGS) -g $(TOPT) -Wa,-alms=$(LSTDIR)/$(notdir $(<:.S=.lst)) $(ADEFS)
CFLAGS    = $(MCFLAGS) $(TOPT) $(OPT) $(COPT) $(CWARN) -Wa,-alms=$(LSTDIR)/$(notdir $(<:.c=.lst)) $(DEFS)
CXXFLAGS  = $(MCFLAGS) $(TOPT) $(OPT) $(CXXOPT) $(CXXWARN) -Wa,-alms=$(LSTDIR)/$(notdir $(<:.cpp=.lst)) $(DEFS)
LDFLAGS   := $(MCFLAGS) $(TOPT) $(OPT) -nostartfiles $(LIBDIR) -Wl,-Map=$(BUILDDIR)/$(PROJECT).map,--cref,--no-warn-mismatch,--library-path=$(RULESPATH),--script=$(LDSCRIPT) $(LDOPT)

OUTFILES := $(BUILDDIR)/$(PROJECT).elf \
	    $(BUILDDIR)/$(PROJECT).hex \
	    $(BUILDDIR)/$(PROJECT).bin \
	    $(BUILDDIR)/$(PROJECT).dmp \
	    $(BUILDDIR)/$(PROJECT).list

###############################################################################
# targets
###############################################################################

all: PRE_ALL $(OBJS) $(OUTFILES) POST_ALL
	@echo Done
	@echo

PRE_ALL:

POST_ALL:

$(OBJS): | $(BUILDDIR) $(OBJDIR) $(LSTDIR)

$(BUILDDIR):
	@echo Compiler Options
	@echo $(CC) -c $(CFLAGS) -I. $(INCDIR)
	@echo
	@mkdir -p $(BUILDDIR)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(LSTDIR):
	@mkdir -p $(LSTDIR)

$(ASMOBJS) : $(OBJDIR)/%.o : %.s Makefile
	@echo Assembling $(<F)
	@$(AS) -c $(ASFLAGS) -I. $(INCDIR) $< -o $@

$(ASMXOBJS) : $(OBJDIR)/%.o : %.S Makefile
	@echo Assembling $(<F)
	@$(CC) -c $(ASXFLAGS) -I. $(INCDIR) $< -o $@

$(COBJS) : $(OBJDIR)/%.o : %.c $(HEADERDEPS) Makefile
	@echo Compiling $(<F)
	@$(CC) -c $(CFLAGS) -I. $(INCDIR) $< -o $@

$(CXXOBJS) : $(OBJDIR)/%.o : %.cpp $(HEADERDEPS) Makefile
	@echo Compiling $(<F)
	@$(CXXC) -c $(CXXFLAGS) -I. $(INCDIR) $< -o $@

$(BUILDDIR)/%.elf: $(OBJS) $(LDSCRIPT)
	@echo Linking $@
	@$(LD) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

%.hex: %.elf
	@echo Creating $@
	@$(HEX) $< $@

%.bin: %.elf
	@echo Creating $@
	@$(BIN) $< $@

%.dmp: %.elf
	@echo Creating $@
	@$(OD) $(ODFLAGS) $< > $@
	@echo
	@$(SZ) $<
	@echo

%.list: %.elf
	@echo Creating $@
	@$(OD) -S $< > $@

clean:
	@echo Cleaning
	-rm -fR $(PROJECTDIR)/.dep $(BUILDDIR) $(PROJECTDIR)/$(PKGARCH)
	@echo Done
	@echo

$(BUILDDIR)/$(PKGARCH): | $(OBJS) $(OUTFILES)
	@echo Packaging to $(BUILDDIR)/$(PKGARCH)
	@mkdir -p $(BUILDDIR)/$(PROJECT)
	@cp -a $(PROJECTDIR)/$(MANIFEST) $(BUILDDIR)/$(PROJECT)/
	@cp -a $(BUILDDIR)/$(PROJECT).bin $(BUILDDIR)/$(PROJECT)/$(PAYLOAD)
	@cd $(BUILDDIR) && $(ZIP) $(ZIP_ARGS) $(PROJECT).zip $(PROJECT)
	@mv $(BUILDDIR)/$(PROJECT).zip $(BUILDDIR)/$(PKGARCH)

install: $(BUILDDIR)/$(PKGARCH)
	@echo Deploying to $(INSTALLDIR)/$(PKGARCH)
	@mv $(BUILDDIR)/$(PKGARCH) $(INSTALLDIR)/$(PKGARCH)
	@echo Done
	@echo

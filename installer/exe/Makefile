# phiola Installer

ROOT := ../..
PHIOLA := $(ROOT)/phiola
FFPACK := $(ROOT)/ffpack
FFGUI := $(ROOT)/ffgui
FFSYS := $(ROOT)/ffsys
FFBASE := $(ROOT)/ffbase

include $(FFBASE)/conf.mk
FFPACK_BIN := $(FFPACK)/_windows-$(CPU)
FFPACK_FLAGS := OS=$(OS) SYS=$(SYS) COMPILER=$(COMPILER) CPU=$(CPU)
EXE := phiola-$(PHI_VERSION_STR)-windows-x64-setup.exe

CFLAGS += -DFFBASE_MEM_ASSERT -DFFBASE_OPT_SIZE \
	-MMD -MP \
	-I$(FFGUI) -I$(FFSYS) -I$(FFBASE) \
	-fdata-sections -ffunction-sections \
	-g

OBJ := windows.o crc32.o

OBJ += \
	ffgui-winapi.o \
	ffgui-winapi-loader.o \
	str-format.o
LINKFLAGS += -Wl,--gc-sections \
	-lshell32 -luxtheme -lcomctl32 -lcomdlg32 -lgdi32 -lole32 -luuid

ifeq "$(DEBUG)" "1"
	CFLAGS += -DFF_DEBUG -O0 -Werror -Wno-deprecated-declarations
else
	CFLAGS += -O3 -fno-strict-aliasing
	LINKFLAGS += -mwindows
endif

LINKXXFLAGS := $(LINKFLAGS)
CXXFLAGS := -std=c++11 $(CFLAGS) -fno-exceptions -fno-rtti

default: $(EXE)
ifneq "$(DEBUG)" "1"
	$(SUBMAKE) strip-debug
endif

-include $(wildcard *.d)

%.o: $(FFBASE)/ffbase/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: $(FFGUI)/ffgui/winapi/%.c
	$(C) $(CFLAGS) $< -o $@

crc32.o: $(FFPACK)/crc/crc.c
	$(C) $(CFLAGS) $< -o $@

CXXFLAGS_INST := $(CXXFLAGS) \
	-DFFPACK_ZIPREAD_ZLIB -DPHI_VERSION_STR=\"$(PHI_VERSION_STR)\" \
	-I$(PHIOLA)/installer/exe -I$(PHIOLA)/src -I$(FFPACK)
%.o: $(PHIOLA)/installer/exe/%.cpp
	$(CXX) $(CXXFLAGS_INST) $< -o $@

installer-config.h: phiola-$(PHI_VERSION_STR)-windows-x64.zip
	echo '#define __PACKAGE__  "phiola-$(PHI_VERSION_STR)-windows-x64.zip"' >$@

installer.coff: $(PHIOLA)/installer/exe/exe.rc \
		installer-config.h \
		$(PHIOLA)/installer/exe/windows.ui \
		$(PHIOLA)/res/exe.manifest \
		$(PHIOLA)/res/phiola.ico \
		phiola-$(PHI_VERSION_STR)-windows-x64.zip
	$(WINDRES) -I. -I$(PHIOLA)/res $< $@

$(FFPACK_BIN)/libz.a: $(FFPACK)/zlib/Makefile | $(FFPACK_BIN)
	$(MAKE)  -C $(FFPACK_BIN)  -f ../zlib/Makefile  -I ..  FFPACK=.. $(FFPACK_FLAGS)

$(FFPACK_BIN):
	$(MKDIR) $@

$(EXE): $(OBJ) installer.coff | $(FFPACK_BIN)/libz.a
	$(LINKXX) $+ $(LINKXXFLAGS) $(FFPACK_BIN)/libz.a -o $@

strip-debug: $(addsuffix .debug,$(EXE))
%.debug: %
	$(OBJCOPY) --only-keep-debug $< $@
	$(STRIP) $<
	$(OBJCOPY) --add-gnu-debuglink=$@ $<
	touch $@

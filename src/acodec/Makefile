# phiola audio codecs

# Inherit:
# C
# CFLAGS
# CPU
# LINK
# LINK_RPATH_ORIGIN
# LINKFLAGS
# OS
# PHIOLA
# ROOT_DIR
# SO
# SYS

# Modify:
# MODS
# LIBS3

AVPACK := $(ROOT_DIR)/avpack
ALIB3 := $(PHIOLA)/alib3
ALIB3_BIN := $(ALIB3)/_$(SYS)-$(CPU)
ALIB3_FLAGS := OS=$(OS) SYS=$(SYS) COMPILER=$(COMPILER) CPU=$(CPU)
ACPFX := ac-

%.o: $(PHIOLA)/src/acodec/%.c
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@

# LOSSY

MODS += $(ACPFX)aac.$(SO)
LIBS3 += $(ALIB3_BIN)/libfdk-aac-phi.$(SO)

$(ALIB3_BIN)/libfdk-aac-phi.$(SO): $(ALIB3)/fdk-aac/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../fdk-aac/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)aac.$(SO): aac.o | $(ALIB3_BIN)/libfdk-aac-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lfdk-aac-phi -o $@


MODS += $(ACPFX)mpeg.$(SO)
LIBS3 += $(ALIB3_BIN)/libmpg123-phi.$(SO)

$(ALIB3_BIN)/libmpg123-phi.$(SO): $(ALIB3)/mpg123/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../mpg123/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)mpeg.$(SO): mpeg.o | $(ALIB3_BIN)/libmpg123-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmpg123-phi -o $@


MODS += $(ACPFX)mp3lame.$(SO)
LIBS3 += $(ALIB3_BIN)/libmp3lame-phi.$(SO)

$(ALIB3_BIN)/libmp3lame-phi.$(SO): $(ALIB3)/mp3lame/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../mp3lame/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)mp3lame.$(SO): mp3lame.o | $(ALIB3_BIN)/libmp3lame-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmp3lame-phi -o $@


MODS += $(ACPFX)vorbis.$(SO)
LIBS3 += $(ALIB3_BIN)/libvorbis-phi.$(SO)
VORBIS_ENC_LIB :=
VORBIS_ENC_FILE :=
ifneq "$(SYS)" "android"
	LIBS3 += $(ALIB3_BIN)/libvorbisenc-phi.$(SO)
	VORBIS_ENC_LIB := -lvorbisenc-phi
vorbis.o: CFLAGS += -DPHI_VORBIS_ENC=1
endif

$(ALIB3_BIN)/libvorbis-phi.$(SO): $(ALIB3)/vorbis/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../vorbis/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)vorbis.$(SO): vorbis.o | $(ALIB3_BIN)/libvorbis-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lvorbis-phi $(VORBIS_ENC_LIB) -o $@


MODS += $(ACPFX)opus.$(SO)
LIBS3 += $(ALIB3_BIN)/libopus-phi.$(SO)

$(ALIB3_BIN)/libopus-phi.$(SO): $(ALIB3)/opus/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../opus/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)opus.$(SO): opus.o | $(ALIB3_BIN)/libopus-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lopus-phi -o $@


ifneq "$(SYS)" "android"
MODS += $(ACPFX)mpc.$(SO)
LIBS3 += $(ALIB3_BIN)/libmusepack-phi.$(SO)
endif

$(ALIB3_BIN)/libmusepack-phi.$(SO): $(ALIB3)/musepack/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../musepack/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)mpc.$(SO): mpc.o | $(ALIB3_BIN)/libmusepack-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmusepack-phi -o $@


# LOSSLESS

MODS += $(ACPFX)alac.$(SO)
LIBS3 += $(ALIB3_BIN)/libALAC-phi.$(SO)

$(ALIB3_BIN)/libALAC-phi.$(SO): $(ALIB3)/ALAC/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../ALAC/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)alac.$(SO): alac.o | $(ALIB3_BIN)/libALAC-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lALAC-phi -o $@


ifneq "$(SYS)" "android"
MODS += $(ACPFX)ape.$(SO)
LIBS3 += $(ALIB3_BIN)/libMAC-phi.$(SO)
endif

$(ALIB3_BIN)/libMAC-phi.$(SO): $(ALIB3)/MAC/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../MAC/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)ape.$(SO): ape.o | $(ALIB3_BIN)/libMAC-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lMAC-phi -o $@


MODS += $(ACPFX)flac.$(SO)
LIBS3 += $(ALIB3_BIN)/libFLAC-phi.$(SO)

$(ALIB3_BIN)/libFLAC-phi.$(SO): $(ALIB3)/FLAC/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../FLAC/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)flac.$(SO): flac.o | $(ALIB3_BIN)/libFLAC-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lFLAC-phi -o $@


ifneq "$(SYS)" "android"
MODS += $(ACPFX)wavpack.$(SO)
LIBS3 += $(ALIB3_BIN)/libwavpack-phi.$(SO)
endif

$(ALIB3_BIN)/libwavpack-phi.$(SO): $(ALIB3)/wavpack/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../wavpack/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

$(ACPFX)wavpack.$(SO): wavpack.o | $(ALIB3_BIN)/libwavpack-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lwavpack-phi -o $@

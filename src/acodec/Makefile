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
ACPFX := ac-

%.o: $(PHIOLA)/src/acodec/%.c
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@

$(ACPFX)%.$(SO): %.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

# LOSSY

MODS += $(ACPFX)aac.$(SO)
LIBS3 += $(ALIB3_BIN)/libfdk-aac-phi.$(SO)
$(ACPFX)aac.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lfdk-aac-phi

MODS += $(ACPFX)mpeg.$(SO)
LIBS3 += $(ALIB3_BIN)/libmpg123-phi.$(SO)
$(ACPFX)mpeg.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmpg123-phi

MODS += $(ACPFX)mp3lame.$(SO)
LIBS3 += $(ALIB3_BIN)/libmp3lame-phi.$(SO)
$(ACPFX)mp3lame.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmp3lame-phi

MODS += $(ACPFX)vorbis.$(SO)
LIBS3 += $(ALIB3_BIN)/libvorbis-phi.$(SO)
VORBIS_ENC_LIB :=
ifneq "$(SYS)" "android"
	LIBS3 += $(ALIB3_BIN)/libvorbisenc-phi.$(SO)
	VORBIS_ENC_LIB := -lvorbisenc-phi
vorbis.o: CFLAGS += -DPHI_VORBIS_ENC=1
endif
$(ACPFX)vorbis.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lvorbis-phi $(VORBIS_ENC_LIB)

MODS += $(ACPFX)opus.$(SO)
LIBS3 += $(ALIB3_BIN)/libopus-phi.$(SO)
$(ACPFX)opus.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lopus-phi

ifneq "$(SYS)" "android"
MODS += $(ACPFX)mpc.$(SO)
LIBS3 += $(ALIB3_BIN)/libmusepack-phi.$(SO)
endif
$(ACPFX)mpc.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmusepack-phi

# LOSSLESS

MODS += $(ACPFX)alac.$(SO)
LIBS3 += $(ALIB3_BIN)/libALAC-phi.$(SO)
$(ACPFX)alac.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lALAC-phi

ifneq "$(SYS)" "android"
MODS += $(ACPFX)ape.$(SO)
LIBS3 += $(ALIB3_BIN)/libMAC-phi.$(SO)
endif
$(ACPFX)ape.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lMAC-phi

MODS += $(ACPFX)flac.$(SO)
LIBS3 += $(ALIB3_BIN)/libFLAC-phi.$(SO)
$(ACPFX)flac.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lFLAC-phi

ifneq "$(SYS)" "android"
MODS += $(ACPFX)wavpack.$(SO)
LIBS3 += $(ALIB3_BIN)/libwavpack-phi.$(SO)
endif
$(ACPFX)wavpack.$(SO): LINKFLAGS += $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lwavpack-phi

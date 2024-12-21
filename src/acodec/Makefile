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

AVPACK := $(ROOT_DIR)/avpack
ALIB3 := $(PHIOLA)/alib3
ALIB3_BIN := $(ALIB3)/_$(SYS)-$(CPU)
ACPFX := ac-

# LOSSY

%.o: $(PHIOLA)/src/acodec/%.c
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@

MODS += $(ACPFX)aac.$(SO)
LIBS3 += $(ALIB3_BIN)/libfdk-aac-phi.$(SO)
$(ACPFX)aac.$(SO): aac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lfdk-aac-phi -o $@

MODS += $(ACPFX)mpeg.$(SO)
LIBS3 += $(ALIB3_BIN)/libmpg123-phi.$(SO)
$(ACPFX)mpeg.$(SO): mpeg.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmpg123-phi -o $@

MODS += $(ACPFX)vorbis.$(SO)
LIBS3 += $(ALIB3_BIN)/libvorbis-phi.$(SO) $(ALIB3_BIN)/libvorbisenc-phi.$(SO)
vorbis.o: $(PHIOLA)/src/acodec/vorbis.c
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
$(ACPFX)vorbis.$(SO): vorbis.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lvorbis-phi -lvorbisenc-phi -o $@

MODS += $(ACPFX)opus.$(SO)
LIBS3 += $(ALIB3_BIN)/libopus-phi.$(SO)
opus.o: $(PHIOLA)/src/acodec/opus.c
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
$(ACPFX)opus.$(SO): opus.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lopus-phi -o $@

ifneq "$(SYS)" "android"
MODS += $(ACPFX)mpc.$(SO)
LIBS3 += $(ALIB3_BIN)/libmusepack-phi.$(SO)
endif
$(ACPFX)mpc.$(SO): mpc.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmusepack-phi -o $@

# LOSSLESS

MODS += $(ACPFX)alac.$(SO)
LIBS3 += $(ALIB3_BIN)/libALAC-phi.$(SO)
$(ACPFX)alac.$(SO): alac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lALAC-phi -o $@

ifneq "$(SYS)" "android"
MODS += $(ACPFX)ape.$(SO)
LIBS3 += $(ALIB3_BIN)/libMAC-phi.$(SO)
endif
ape.o: $(PHIOLA)/src/acodec/ape.c
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
$(ACPFX)ape.$(SO): ape.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lMAC-phi -o $@

MODS += $(ACPFX)flac.$(SO)
LIBS3 += $(ALIB3_BIN)/libFLAC-phi.$(SO)
flac.o: $(PHIOLA)/src/acodec/flac.c
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
$(ACPFX)flac.$(SO): flac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lFLAC-phi -o $@

ifneq "$(SYS)" "android"
MODS += $(ACPFX)wavpack.$(SO)
LIBS3 += $(ALIB3_BIN)/libwavpack-phi.$(SO)
endif
$(ACPFX)wavpack.$(SO): wavpack.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lwavpack-phi -o $@

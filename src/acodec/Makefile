# phiola audio codecs

# Inherit:
# ALIB3
# ALIB3_BIN
# AVPACK
# C
# CFLAGS
# DEPS
# LINK
# LINK_RPATH_ORIGIN
# LINKFLAGS
# PHIOLA
# SO

# Modify:
# MODS

# LOSSY

MODS += aac.$(SO)
LIBS3 += $(ALIB3_BIN)/libfdk-aac-phi.$(SO)
aac.o: $(PHIOLA)/src/acodec/aac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/aac-*.h) \
		$(PHIOLA)/src/acodec/alib3-bridge/aac.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
aac.$(SO): aac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lfdk-aac-phi -o $@

MODS += mpeg.$(SO)
LIBS3 += $(ALIB3_BIN)/libmpg123-phi.$(SO)
mpeg.o: $(PHIOLA)/src/acodec/mpeg.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/mpeg-*.h)
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
mpeg.$(SO): mpeg.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmpg123-phi -o $@

MODS += vorbis.$(SO)
LIBS3 += $(ALIB3_BIN)/libvorbis-phi.$(SO) $(ALIB3_BIN)/libvorbisenc-phi.$(SO) $(ALIB3_BIN)/libogg-phi.$(SO)
vorbis.o: $(PHIOLA)/src/acodec/vorbis.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/vorbis-*.h) \
		$(PHIOLA)/src/acodec/alib3-bridge/vorbis.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
vorbis.$(SO): vorbis.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -logg-phi -lvorbis-phi -lvorbisenc-phi -o $@

MODS += opus.$(SO)
LIBS3 += $(ALIB3_BIN)/libopus-phi.$(SO)
opus.o: $(PHIOLA)/src/acodec/opus.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/opus-*.h) \
		$(PHIOLA)/src/acodec/alib3-bridge/opus.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
opus.$(SO): opus.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lopus-phi -o $@

MODS += mpc.$(SO)
LIBS3 += $(ALIB3_BIN)/libmusepack-phi.$(SO)
mpc.o: $(PHIOLA)/src/acodec/mpc.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/mpc-*.h) \
		$(PHIOLA)/src/acodec/alib3-bridge/musepack.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
mpc.$(SO): mpc.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmusepack-phi -o $@

# LOSSLESS

MODS += alac.$(SO)
LIBS3 += $(ALIB3_BIN)/libALAC-phi.$(SO)
alac.o: $(PHIOLA)/src/acodec/alac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/alac-*.h) \
		$(PHIOLA)/src/acodec/alib3-bridge/alac.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
alac.$(SO): alac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lALAC-phi -o $@

MODS += ape.$(SO)
LIBS3 += $(ALIB3_BIN)/libMAC-phi.$(SO)
ape.o: $(PHIOLA)/src/acodec/ape.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/ape-*.h) \
		$(PHIOLA)/src/acodec/alib3-bridge/ape.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
ape.$(SO): ape.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lMAC-phi -o $@

MODS += flac.$(SO)
LIBS3 += $(ALIB3_BIN)/libFLAC-phi.$(SO)
flac.o: $(PHIOLA)/src/acodec/flac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/flac-*.h) \
		$(PHIOLA)/src/acodec/alib3-bridge/flac.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
flac.$(SO): flac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lFLAC-phi -o $@

MODS += wavpack.$(SO)
LIBS3 += $(ALIB3_BIN)/libwavpack-phi.$(SO)
wavpack.o: $(PHIOLA)/src/acodec/wavpack.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/wavpack-*.h) \
		$(PHIOLA)/src/acodec/alib3-bridge/wavpack.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
wavpack.$(SO): wavpack.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lwavpack-phi -o $@

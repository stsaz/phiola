# phiola audio filters

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
# LIBS3
# MODS

FFAUDIO := $(ROOT_DIR)/ffaudio
ALIB3 := $(PHIOLA)/alib3
ALIB3_BIN := $(ALIB3)/_$(SYS)-$(CPU)
ALIB3_FLAGS := OS=$(OS) SYS=$(SYS) COMPILER=$(COMPILER) CPU=$(CPU)
AFPFX := af-

MODS += afilter.$(SO)
%.o: $(PHIOLA)/src/afilter/%.c
	$(C) $(CFLAGS) -DFFBASE_OPT_SIZE $< -o $@
conv.o: CFLAGS += -I$(FFAUDIO)
gain.o: CFLAGS += -I$(FFAUDIO)
auto-norm.o: CFLAGS += -I$(FFAUDIO)
afilter.$(SO): afilter.o \
		peaks.o \
		gain.o \
		rtpeak.o \
		auto-norm.o \
		conv.o \
		\
		str-format.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@


MODS += $(AFPFX)soxr.$(SO)
LIBS3 += $(ALIB3_BIN)/libsoxr-phi.$(SO)

$(ALIB3_BIN)/libsoxr-phi.$(SO): $(ALIB3)/soxr/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../soxr/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

soxr-conv.o: $(PHIOLA)/src/afilter/soxr-conv.c
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@

$(AFPFX)soxr.$(SO): soxr-conv.o | $(ALIB3_BIN)/libsoxr-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lsoxr-phi -o $@


MODS += $(AFPFX)sox.$(SO)
LIBS3 += $(ALIB3_BIN)/libsox-phi.$(SO)

$(ALIB3_BIN)/libsox-phi.$(SO): $(ALIB3)/sox/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../sox/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

sox.o: $(PHIOLA)/src/afilter/sox.c
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@

$(AFPFX)sox.$(SO): sox.o | $(ALIB3_BIN)/libsox-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lsox-phi -o $@


MODS += $(AFPFX)danorm.$(SO)
LIBS3 += $(ALIB3_BIN)/libDynamicAudioNormalizer-phi.$(SO)

$(ALIB3_BIN)/libDynamicAudioNormalizer-phi.$(SO): $(ALIB3)/DynamicAudioNormalizer/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../DynamicAudioNormalizer/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

dynanorm.o: $(PHIOLA)/src/afilter/dynanorm.c
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@

$(AFPFX)danorm.$(SO): dynanorm.o | $(ALIB3_BIN)/libDynamicAudioNormalizer-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lDynamicAudioNormalizer-phi -o $@


MODS += $(AFPFX)loudness.$(SO)
LIBS3 += $(ALIB3_BIN)/libebur128-phi.$(SO)

$(ALIB3_BIN)/libebur128-phi.$(SO): $(ALIB3)/EBUR128/Makefile | $(ALIB3_BIN)
	$(MAKE)  -C $(ALIB3_BIN)  -f ../EBUR128/Makefile  -I ..  ALIB3=.. $(ALIB3_FLAGS)

loudness.o: $(PHIOLA)/src/afilter/loudness.c
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@

$(AFPFX)loudness.$(SO): loudness.o | $(ALIB3_BIN)/libebur128-phi.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lebur128-phi -lm -o $@


$(ALIB3_BIN):
	$(MKDIR) $@

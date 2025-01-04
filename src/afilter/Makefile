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
# SO
# SYS

# Modify:
# LIBS3
# MODS

ALIB3 := $(PHIOLA)/alib3
ALIB3_BIN := $(ALIB3)/_$(SYS)-$(CPU)
AFPFX := af-

MODS += afilter.$(SO)
%.o: $(PHIOLA)/src/afilter/%.c
	$(C) $(CFLAGS) -DFFBASE_OPT_SIZE $< -o $@
afilter.$(SO): afilter.o \
		peaks.o \
		gain.o \
		rtpeak.o \
		conv.o \
		\
		str-format.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@

MODS += $(AFPFX)soxr.$(SO)
LIBS3 += $(ALIB3_BIN)/libsoxr-phi.$(SO)
soxr.o: $(PHIOLA)/src/afilter/soxr.c
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
$(AFPFX)soxr.$(SO): soxr.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lsoxr-phi -o $@

MODS += $(AFPFX)danorm.$(SO)
LIBS3 += $(ALIB3_BIN)/libDynamicAudioNormalizer-phi.$(SO)
dynanorm.o: $(PHIOLA)/src/afilter/dynanorm.c
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
$(AFPFX)danorm.$(SO): dynanorm.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lDynamicAudioNormalizer-phi -o $@

MODS += $(AFPFX)loudness.$(SO)
LIBS3 += $(ALIB3_BIN)/libebur128-phi.$(SO)
loudness.o: $(PHIOLA)/src/afilter/loudness.c
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
$(AFPFX)loudness.$(SO): loudness.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lebur128-phi -lm -o $@

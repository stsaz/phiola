# phiola file format reader/writer

# Inherit:
# C
# CFLAGS
# LINK
# LINKFLAGS
# PHIOLA
# ROOT_DIR
# SO

# Modify:
# MODS

AVPACK := $(ROOT_DIR)/avpack

MODS += format.$(SO)

CFLAGS_FORMAT := $(CFLAGS) -DFFBASE_OPT_SIZE -I$(AVPACK)
%.o: $(PHIOLA)/src/format/%.c
	$(C) $(CFLAGS_FORMAT) $< -o $@
%.o: $(PHIOLA)/src/list/%.c
	$(C) $(CFLAGS_FORMAT) $< -o $@

format.$(SO): mod-fmt.o \
		aac-adts.o \
		flac-fmt.o \
		ogg.o \
		ts-read.o \
		\
		tag.o \
		\
		cue-read.o \
		m3u.o \
		pls-read.o \
		\
		sort.o \
		str-format.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@

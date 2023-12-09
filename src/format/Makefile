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

%.o: $(PHIOLA)/src/format/%.c
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
%.o: $(PHIOLA)/src/list/%.c
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@

format.$(SO): mod-fmt.o \
		aac-adts.o \
		ape-read.o \
		avi.o \
		caf.o \
		flac-fmt.o flac-ogg.o \
		mkv.o \
		mp3.o \
		mp4.o \
		mpc-read.o \
		ogg.o \
		wav.o \
		wv.o \
		\
		tag.o \
		\
		cue-read.o \
		m3u.o \
		pls-read.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

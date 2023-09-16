# phiola audio I/O modules

# Inherit:
# C
# CFLAGS
# CFLAGS_BASE
# DEPS
# LINK
# LINKFLAGS
# OS
# PHIOLA
# ROOT_DIR
# SO
# SYS

# Modify:
# MODS

FFAUDIO := $(ROOT_DIR)/ffaudio

ifeq "$(OS)" "windows"
	MODS += wasapi.$(SO) direct-sound.$(SO)
else ifeq "$(OS)" "apple"
	MODS += coreaudio.$(SO)
else ifeq "$(OS)" "freebsd"
	MODS += oss.$(SO)
else ifeq "$(SYS)" "android"
	MODS += aaudio.$(SO)
else ifeq "$(OS)" "linux"
	MODS += alsa.$(SO) pulse.$(SO) jack.$(SO)
endif

%.o: $(PHIOLA)/src/adev/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/adev/*.h)
	$(C) $(CFLAGS) -I$(FFAUDIO) $< -o $@

ffaudio-wasapi.o: $(FFAUDIO)/ffaudio/wasapi.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@
wasapi.$(SO): wasapi.o ffaudio-wasapi.o
	$(LINK) -shared $+ $(LINKFLAGS) -lole32 -o $@

ffaudio-dsound.o: $(FFAUDIO)/ffaudio/dsound.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@
direct-sound.$(SO): directsound.o ffaudio-dsound.o
	$(LINK) -shared $+ $(LINKFLAGS) -ldsound -ldxguid -o $@

ffaudio-alsa.o: $(FFAUDIO)/ffaudio/alsa.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@
alsa.$(SO): alsa.o ffaudio-alsa.o
	$(LINK) -shared $+ $(LINKFLAGS) -lasound -o $@

ffaudio-pulse.o: $(FFAUDIO)/ffaudio/pulse.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@
pulse.$(SO): pulse.o ffaudio-pulse.o
	$(LINK) -shared $+ $(LINKFLAGS) -lpulse -o $@

ffaudio-jack.o: $(FFAUDIO)/ffaudio/jack.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@
jack.$(SO): jack.o ffaudio-jack.o
	$(LINK) -shared $+ $(LINKFLAGS) -L/usr/lib64/pipewire-0.3/jack -ljack -o $@

ffaudio-aaudio.o: $(FFAUDIO)/ffaudio/aaudio.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@
aaudio.$(SO): aaudio.o ffaudio-aaudio.o
	$(LINK) -shared $+ $(LINKFLAGS) -laaudio -o $@

ffaudio-coreaudio.o: $(FFAUDIO)/ffaudio/coreaudio.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@
coreaudio.$(SO): coreaudio.o ffaudio-coreaudio.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

ffaudio-oss.o: $(FFAUDIO)/ffaudio/oss.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@
oss.$(SO): oss.o ffaudio-oss.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

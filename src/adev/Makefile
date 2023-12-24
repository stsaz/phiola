# phiola audio I/O modules

# Inherit:
# C
# CFLAGS
# CFLAGS_BASE
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
ifneq "$(A_API)" "21"
	MODS += aaudio.$(SO)
endif
else ifeq "$(OS)" "linux"
	MODS += alsa.$(SO) pulse.$(SO) jack.$(SO)
endif

%.o: $(PHIOLA)/src/adev/%.c
	$(C) $(CFLAGS) -I$(FFAUDIO) $< -o $@
ffaudio-%.o: $(FFAUDIO)/ffaudio/%.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@

wasapi.$(SO): wasapi.o ffaudio-wasapi.o
	$(LINK) -shared $+ $(LINKFLAGS) -lole32 -o $@

direct-sound.$(SO): directsound.o ffaudio-dsound.o
	$(LINK) -shared $+ $(LINKFLAGS) -ldsound -ldxguid -o $@

alsa.$(SO): alsa.o ffaudio-alsa.o
	$(LINK) -shared $+ $(LINKFLAGS) -lasound -o $@

pulse.$(SO): pulse.o ffaudio-pulse.o
	$(LINK) -shared $+ $(LINKFLAGS) -lpulse -o $@

jack.$(SO): jack.o ffaudio-jack.o
	$(LINK) -shared $+ $(LINKFLAGS) -L/usr/lib64/pipewire-0.3/jack -ljack -o $@

aaudio.$(SO): aaudio.o ffaudio-aaudio.o
	$(LINK) -shared $+ $(LINKFLAGS) -laaudio -o $@

coreaudio.$(SO): coreaudio.o ffaudio-coreaudio.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

oss.$(SO): oss.o ffaudio-oss.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

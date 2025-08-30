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
	MODS += wasapi.$(SO)
else ifeq "$(OS)" "apple"
	MODS += coreaudio.$(SO)
else ifeq "$(OS)" "freebsd"
	MODS += oss.$(SO)
else ifeq "$(SYS)" "android"
ifneq "$(A_API)" "23"
	MODS += aaudio.$(SO)
endif
else ifeq "$(OS)" "linux"
	MODS += alsa.$(SO) pulse.$(SO) jack.$(SO)
endif

%.o: $(PHIOLA)/src/adev/%.c
	$(C) $(CFLAGS) -I$(FFAUDIO) $< -o $@

ffaudio-%.o: $(FFAUDIO)/ffaudio/%.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@

%.$(SO): %.o ffaudio-%.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

wasapi.$(SO): LINKFLAGS += -lole32

alsa.$(SO): LINKFLAGS += -lasound

pulse.$(SO): LINKFLAGS += -lpulse

jack.$(SO): LINKFLAGS += -L/usr/lib64/pipewire-0.3/jack -ljack

aaudio.$(SO): LINKFLAGS += -laaudio

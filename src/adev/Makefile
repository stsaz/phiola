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
ADPFX := ad-

ifeq "$(OS)" "windows"
	MODS += $(ADPFX)wasapi.$(SO)
else ifeq "$(OS)" "apple"
	MODS += $(ADPFX)coreaudio.$(SO)
else ifeq "$(OS)" "freebsd"
	MODS += $(ADPFX)oss.$(SO)
else ifeq "$(SYS)" "android"
	MODS += $(ADPFX)aaudio.$(SO)
else ifeq "$(OS)" "linux"
	MODS += $(ADPFX)alsa.$(SO) $(ADPFX)pulse.$(SO) $(ADPFX)jack.$(SO)
endif

%.o: $(PHIOLA)/src/adev/%.c
	$(C) $(CFLAGS) -I$(FFAUDIO) $< -o $@

ffaudio-%.o: $(FFAUDIO)/ffaudio/%.c
	$(C) -I$(FFAUDIO) $(CFLAGS_BASE) $< -o $@

$(ADPFX)%.$(SO): %.o ffaudio-%.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

$(ADPFX)wasapi.$(SO): LINKFLAGS += -lole32

$(ADPFX)alsa.$(SO): LINKFLAGS += -lasound

$(ADPFX)pulse.$(SO): LINKFLAGS += -lpulse

$(ADPFX)jack.$(SO): LINKFLAGS += -L/usr/lib64/pipewire-0.3/jack -ljack

$(ADPFX)aaudio.$(SO): LINKFLAGS += -laaudio

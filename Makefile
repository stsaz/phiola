# phiola Makefile

ROOT_DIR := ..
PHIOLA := $(ROOT_DIR)/phiola
FFAUDIO := $(ROOT_DIR)/ffaudio
AVPACK := $(ROOT_DIR)/avpack
FFPACK := $(ROOT_DIR)/ffpack
NETMILL := $(ROOT_DIR)/netmill
FFOS := $(ROOT_DIR)/ffos
FFBASE := $(ROOT_DIR)/ffbase

include $(FFBASE)/test/makeconf

SUBMAKE := $(MAKE) -f $(firstword $(MAKEFILE_LIST))
ALIB3 := $(PHIOLA)/alib3
ALIB3_BIN := $(ALIB3)/_$(OS)-$(CPU)
FFPACK_BIN := $(FFPACK)/_$(OS)-$(CPU)

# COMPILER

CFLAGS += -DFFBASE_MEM_ASSERT
CFLAGS += -I$(FFBASE)
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-for-loop-analysis -Wno-multichar
CFLAGS += -g
CFLAGS += -fPIC
ifeq "$(DEBUG)" "1"
	CFLAGS += -DFF_DEBUG -O0 -Werror -Wno-deprecated-declarations
else
	CFLAGS += -O3 -fno-strict-aliasing -fvisibility=hidden
endif
ifeq "$(ASAN)" "1"
	CFLAGS += -fsanitize=address
	LINKFLAGS += -fsanitize=address
endif
CFLAGS_BASE := $(CFLAGS)
CFLAGS += -I$(PHIOLA)/src -I$(FFOS)
CXXFLAGS := $(CFLAGS) -fno-exceptions -fno-rtti -Wno-c++11-narrowing
ifeq "$(OS)" "windows"
	LINKFLAGS += -lws2_32
endif
LINK_DL :=
ifeq "$(OS)" "linux"
	LINK_DL := -ldl
endif

# MODULES

ifneq "$(DEBUG)" "1"
default: strip-debug
	$(SUBMAKE) app
else
default: build
	$(SUBMAKE) app
endif

DEPS := $(PHIOLA)/src/phiola.h \
	$(PHIOLA)/src/track.h

%.o: $(PHIOLA)/src/%.c $(DEPS)
	$(C) $(CFLAGS) $< -o $@

# EXE
_:=
ifeq "$(OS)" "windows"
	EXE_OBJ := exe.coff
endif
%.o: $(PHIOLA)/src/exe/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/exe/*.h) \
		$(wildcard $(PHIOLA)/src/util/*.h)
	$(C) $(CFLAGS) $< -o $@
exe.coff: $(PHIOLA)/src/gui/res/exe.rc \
		$(PHIOLA)/src/gui/res/exe.manifest \
		$(wildcard $(PHIOLA)/src/gui/res/*.ico)
	$(WINDRES) $< $@
phiola$(DOTEXE): main.o \
		$(EXE_OBJ) \
		core.$(SO)
	$(LINK) $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) $(LINK_DL) -o $@

# CORE
%.o: $(PHIOLA)/src/core/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/core/*.h) \
		$(wildcard $(PHIOLA)/src/util/*.h)
	$(C) $(CFLAGS) $< -o $@
%.o: $(PHIOLA)/src/queue/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/queue/*.h)
	$(C) $(CFLAGS) $< -o $@
CORE_O := core.o \
		auto.o \
		dir-read.o \
		file.o\
		qu.o \
		track.o
ifeq "$(OS)" "windows"
	CORE_O += sys-sleep-win.o
endif
core.$(SO): $(CORE_O)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_PTHREAD) $(LINK_DL) -o $@

# AUDIO IO

_:=
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

# AFILTERS

MODS += afilter.$(SO)
%.o: $(PHIOLA)/src/afilter/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/afilter/*.h)
	$(C) $(CFLAGS) $< -o $@
crc.o: $(PHIOLA)/3pt/crc/crc.c
	$(C) $(CFLAGS) $< -o $@
afilter.$(SO): afilter.o \
		crc.o \
		peaks.o \
		gain.o \
		rtpeak.o \
		conv.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@

MODS += soxr.$(SO)
LIBS3 += $(ALIB3_BIN)/libsoxr-phi.$(SO)
soxr.o: $(PHIOLA)/src/afilter/soxr.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/afilter/soxr*.h)
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
soxr.$(SO): soxr.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lsoxr-phi -o $@

MODS += danorm.$(SO)
LIBS3 += $(ALIB3_BIN)/libDynamicAudioNormalizer-phi.$(SO)
dynanorm.o: $(PHIOLA)/src/afilter/dynanorm.c $(DEPS)
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
danorm.$(SO): dynanorm.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lDynamicAudioNormalizer-phi -o $@

# FORMAT
MODS += format.$(SO)
%.o: $(PHIOLA)/src/format/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/format/*.h)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
cue-read.o: $(PHIOLA)/src/list/cue-read.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/list/entry.h)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
m3u.o: $(PHIOLA)/src/list/m3u.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/list/m3u-*.h) \
		$(wildcard $(PHIOLA)/src/list/entry.h)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
pls-read.o: $(PHIOLA)/src/list/pls-read.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/list/entry.h)
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
		cue-read.o \
		m3u.o \
		pls-read.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

ifneq "$(PHI_CODECS)" "0"

# CODECS LOSSY

MODS += aac.$(SO)
LIBS3 += $(ALIB3_BIN)/libfdk-aac-phi.$(SO)
aac.o: $(PHIOLA)/src/acodec/aac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/aac-*.h) $(PHIOLA)/src/acodec/alib3-bridge/aac.h
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
		$(wildcard $(PHIOLA)/src/acodec/vorbis-*.h) $(PHIOLA)/src/acodec/alib3-bridge/vorbis.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
vorbis.$(SO): vorbis.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -logg-phi -lvorbis-phi -lvorbisenc-phi -o $@

MODS += opus.$(SO)
LIBS3 += $(ALIB3_BIN)/libopus-phi.$(SO)
opus.o: $(PHIOLA)/src/acodec/opus.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/opus-*.h) $(PHIOLA)/src/acodec/alib3-bridge/opus.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
opus.$(SO): opus.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lopus-phi -o $@

MODS += mpc.$(SO)
LIBS3 += $(ALIB3_BIN)/libmusepack-phi.$(SO)
mpc.o: $(PHIOLA)/src/acodec/mpc.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/mpc-*.h) $(PHIOLA)/src/acodec/alib3-bridge/musepack.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
mpc.$(SO): mpc.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lmusepack-phi -o $@

# CODECS LOSSLESS

MODS += alac.$(SO)
LIBS3 += $(ALIB3_BIN)/libALAC-phi.$(SO)
alac.o: $(PHIOLA)/src/acodec/alac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/alac-*.h) $(PHIOLA)/src/acodec/alib3-bridge/alac.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
alac.$(SO): alac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lALAC-phi -o $@

MODS += ape.$(SO)
LIBS3 += $(ALIB3_BIN)/libMAC-phi.$(SO)
ape.o: $(PHIOLA)/src/acodec/ape.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/ape-*.h) $(PHIOLA)/src/acodec/alib3-bridge/ape.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
ape.$(SO): ape.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lMAC-phi -o $@

MODS += flac.$(SO)
LIBS3 += $(ALIB3_BIN)/libFLAC-phi.$(SO)
flac.o: $(PHIOLA)/src/acodec/flac.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/flac-*.h) $(PHIOLA)/src/acodec/alib3-bridge/flac.h
	$(C) $(CFLAGS) -I$(ALIB3) -I$(AVPACK) $< -o $@
flac.$(SO): flac.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lFLAC-phi -o $@

MODS += wavpack.$(SO)
LIBS3 += $(ALIB3_BIN)/libwavpack-phi.$(SO)
wavpack.o: $(PHIOLA)/src/acodec/wavpack.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/acodec/wavpack-*.h) $(PHIOLA)/src/acodec/alib3-bridge/wavpack.h
	$(C) $(CFLAGS) -I$(ALIB3) $< -o $@
wavpack.$(SO): wavpack.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(ALIB3_BIN) -lwavpack-phi -o $@

endif # PHI_CODECS

# MISC

ifeq "$(OS)" "linux"
MODS += dbus.$(SO)
sys-sleep-dbus.o: $(PHIOLA)/src/sys-sleep-dbus.c $(DEPS)
	$(C) $(CFLAGS) `pkg-config --cflags dbus-1` $< -o $@
dbus.$(SO): sys-sleep-dbus.o
	$(LINK) -shared $+ $(LINKFLAGS) -ldbus-1 -o $@
endif

MODS += tui.$(SO)
%.o: $(PHIOLA)/src/tui/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/tui/*.h)
	$(C) $(CFLAGS) $< -o $@
tui.$(SO): tui.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@

MODS += gui.$(SO)
ifeq "$(OS)" "windows"
	FFGUI_HDR := $(wildcard $(PHIOLA)/src/util/gui-winapi/*.h)
	CFLAGS_GUI := -Wno-missing-field-initializers
	LINKFLAGS_GUI := -lshell32 -luxtheme -lcomctl32 -lcomdlg32 -lgdi32 -lole32 -luuid
	FFGUI_OBJ := ffgui-winapi.o ffgui-winapi-loader.o
else
	FFGUI_HDR := $(wildcard $(PHIOLA)/src/util/gui-gtk/*.h)
	CFLAGS_GUI := -Wno-free-nonheap-object -Wno-deprecated-declarations `pkg-config --cflags gtk+-3.0`
	ifeq "$(DEBUG)" "1"
		CFLAGS_GUI += -DFFGUI_DEBUG
	endif
	LINKFLAGS_GUI := `pkg-config --libs gtk+-3.0` $(LINK_PTHREAD) -lm
	FFGUI_OBJ := ffgui-gtk.o ffgui-gtk-loader.o
endif
CFLAGS_GUI := $(CFLAGS) $(CFLAGS_GUI)
CXXFLAGS_GUI := $(CXXFLAGS) $(CFLAGS_GUI)
LINKFLAGS_GUI := $(LINKFLAGS) $(LINKFLAGS_GUI)
gui-mod.o: $(PHIOLA)/src/gui/mod.c $(DEPS) \
		$(PHIOLA)/src/gui/mod.h \
		$(PHIOLA)/src/gui/track.h \
		$(PHIOLA)/src/gui/track-convert.h
	$(C) $(CFLAGS) $< -o $@
gui.o: $(PHIOLA)/src/gui/gui.c $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/gui/gui.h \
		$(PHIOLA)/src/gui/mod.h \
		$(PHIOLA)/src/gui/actions.h
	$(C) $(CFLAGS_GUI) $< -o $@
gui-main.o: $(PHIOLA)/src/gui/main.cpp $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/gui/gui.h \
		$(PHIOLA)/src/gui/mod.h \
		$(PHIOLA)/src/gui/actions.h
	$(CXX) $(CXXFLAGS_GUI) $< -o $@
gui-dialogs.o: $(PHIOLA)/src/gui/dialogs.cpp $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/gui/gui.h \
		$(PHIOLA)/src/gui/mod.h \
		$(wildcard $(PHIOLA)/src/gui/*.hpp)
	$(CXX) $(CXXFLAGS_GUI) $< -o $@
ffgui-gtk.o: $(PHIOLA)/src/util/gui-gtk/ffgui-gtk.c $(DEPS) $(FFGUI_HDR)
	$(C) $(CFLAGS_GUI) $< -o $@
ffgui-gtk-loader.o: $(PHIOLA)/src/util/gui-gtk/ffgui-gtk-loader.c $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/util/conf-scheme.h \
		$(wildcard $(PHIOLA)/src/util/ltconf*.h)
	$(C) $(CFLAGS_GUI) $< -o $@
ffgui-winapi.o: $(PHIOLA)/src/util/gui-winapi/ffgui-winapi.c $(DEPS) $(FFGUI_HDR)
	$(C) $(CFLAGS_GUI) $< -o $@
ffgui-winapi-loader.o: $(PHIOLA)/src/util/gui-winapi/ffgui-winapi-loader.c $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/util/conf-scheme.h \
		$(wildcard $(PHIOLA)/src/util/ltconf*.h)
	$(C) $(CFLAGS_GUI) $< -o $@
gui.$(SO): gui-mod.o \
		gui.o \
		gui-main.o \
		gui-dialogs.o \
		$(FFGUI_OBJ)
	$(LINKXX) -shared $+ $(LINKFLAGS_GUI) $(LINK_DL) -o $@

MODS += http.$(SO)
CFLAGS_NETMILL := $(CFLAGS_BASE) -I$(NETMILL)/src
ifeq "$(DEBUG)" "1"
	CFLAGS_NETMILL += -DNML_ENABLE_LOG_EXTRA
endif
%.o: $(PHIOLA)/src/net/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/net/*.h)
	$(C) $(CFLAGS) -I$(NETMILL)/src $< -o $@
icy.o: $(PHIOLA)/src/net/icy.c $(DEPS)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
netmill-http-filters.o: $(PHIOLA)/src/net/http-filters.c \
		$(PHIOLA)/src/net/http-bridge.h \
		$(wildcard $(NETMILL)/src/http-client/*.h)
	$(C) $(CFLAGS_NETMILL) -I$(PHIOLA)/src -I$(FFOS) $< -o $@
netmill-http-client.o: $(NETMILL)/src/http-client/oclient.c \
		$(wildcard $(NETMILL)/src/http-client/*.h)
	$(C) $(CFLAGS_NETMILL) -I$(FFOS) $< -o $@
http.$(SO): http.o \
		icy.o \
		netmill-http-client.o \
		netmill-http-filters.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_PTHREAD) -o $@

MODS += zstd.$(SO)
LIBS3 += $(FFPACK_BIN)/libzstd-ffpack.$(SO)
%.o: $(PHIOLA)/src/dfilter/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/dfilter/zstd-*.h)
	$(C) $(CFLAGS) -I$(FFPACK) $< -o $@
zstd.$(SO): zstd.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(FFPACK_BIN) -lzstd-ffpack -o $@


build: core.$(SO) \
		phiola$(DOTEXE) \
		$(MODS)

strip-debug: core.$(SO).debug \
		phiola$(DOTEXE).debug \
		$(MODS:.$(SO)=.$(SO).debug)
%.debug: %
	$(OBJCOPY) --only-keep-debug $< $@
	$(STRIP) $<
	$(OBJCOPY) --add-gnu-debuglink=$@ $<
	touch $@

APP_DIR := phiola-2
app:
	$(MKDIR) $(APP_DIR) $(APP_DIR)/mod
	$(CP) phiola$(DOTEXE) core.$(SO) $(APP_DIR)/
	chmod 644 $(APP_DIR)/core.$(SO)
	$(CP) $(PHIOLA)/LICENSE \
		$(PHIOLA)/README.md \
		$(APP_DIR)/

	$(CP) $(MODS) $(APP_DIR)/mod/
ifneq "$(LIBS3)" ""
	$(CP) $(LIBS3) $(APP_DIR)/mod/
endif
	$(CP) $(PHIOLA)/src/tui/help.txt $(APP_DIR)/mod/tui-help.txt
	chmod 644 $(APP_DIR)/mod/*.$(SO)

	$(MKDIR) $(APP_DIR)/mod/gui
ifeq "$(OS)" "windows"
	$(CP) $(PHIOLA)/src/gui/phiola-winapi.gui $(APP_DIR)/mod/gui/phiola.gui
	$(CP) $(PHIOLA)/src/gui/gui_lang*.txt \
		$(APP_DIR)/mod/gui/
	sed -i 's/_/\&/' $(APP_DIR)/mod/gui/gui_lang*.txt
	unix2dos $(APP_DIR)/mod/gui/phiola.gui $(APP_DIR)/mod/gui/*.txt
else
	$(CP) $(PHIOLA)/src/gui/phiola-gtk.gui $(APP_DIR)/mod/gui/phiola.gui
	$(CP) $(PHIOLA)/src/gui/gui_lang*.txt \
		$(PHIOLA)/src/gui/res/*.ico $(PHIOLA)/src/gui/res/phiola.desktop \
		$(APP_DIR)/mod/gui/
endif
	chmod 644 $(APP_DIR)/mod/gui/*

ifeq "$(OS)" "windows"
	mv $(APP_DIR)/README.md $(APP_DIR)/README.txt
	unix2dos $(APP_DIR)/README.txt
endif

PKG_VER := test
PKG_ARCH := $(CPU)
PKG_PACKER := tar -c --owner=0 --group=0 --numeric-owner -v --zstd -f
PKG_EXT := tar.zst
ifeq "$(OS)" "windows"
	PKG_PACKER := zip -r -v
	PKG_EXT := zip
endif
PKG_NAME := phiola-$(PKG_VER)-$(OS)-$(PKG_ARCH).$(PKG_EXT)
package: $(PKG_NAME)
$(PKG_NAME): $(APP_DIR)
	$(PKG_PACKER) $@ $<

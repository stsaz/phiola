# phiola Makefile

ROOT_DIR := ..
PHIOLA := $(ROOT_DIR)/phiola
FFOS := $(ROOT_DIR)/ffos
FFBASE := $(ROOT_DIR)/ffbase
APP_DIR := phiola-2

include $(FFBASE)/conf.mk

SUBMAKE := $(MAKE) -f $(firstword $(MAKEFILE_LIST))

# COMPILER

CFLAGS += -DFFBASE_MEM_ASSERT
CFLAGS += -MMD -MP
CFLAGS += -I$(FFBASE)
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-for-loop-analysis -Wno-multichar
CFLAGS += -fPIC
CFLAGS += -g
ifeq "$(DEBUG)" "1"
	CFLAGS += -DFF_DEBUG -O0 -Werror -Wno-deprecated-declarations
else
	CFLAGS += -O3 -fno-strict-aliasing -fvisibility=hidden
endif
ifeq "$(ASAN)" "1"
	CFLAGS += -fsanitize=address
	LINKFLAGS += -fsanitize=address
endif
CFLAGS += $(CFLAGS_USER)
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

-include $(wildcard *.d)

DEPS := $(PHIOLA)/src/phiola.h \
	$(PHIOLA)/src/track.h

%.o: $(PHIOLA)/src/%.c
	$(C) $(CFLAGS) $< -o $@

EXES :=
MODS :=

include $(PHIOLA)/src/core/Makefile
include $(PHIOLA)/src/exe/Makefile
include $(PHIOLA)/src/adev/Makefile
include $(PHIOLA)/src/afilter/Makefile
include $(PHIOLA)/src/format/Makefile

ifneq "$(PHI_CODECS)" "0"
include $(PHIOLA)/src/acodec/Makefile
endif

# MISC

ifeq "$(OS)" "linux"
MODS += dbus.$(SO)
sys-sleep-dbus.o: $(PHIOLA)/src/sys-sleep-dbus.c
	$(C) $(CFLAGS) `pkg-config --cflags dbus-1` $< -o $@
dbus.$(SO): sys-sleep-dbus.o
	$(LINK) -shared $+ $(LINKFLAGS) -ldbus-1 -o $@
endif

MODS += remote.$(SO)
remote.$(SO): remote-ctl.o
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

MODS += tui.$(SO)
%.o: $(PHIOLA)/src/tui/%.c
	$(C) $(CFLAGS) $< -o $@
tui.$(SO): tui.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@

include $(PHIOLA)/src/gui/Makefile
include $(PHIOLA)/src/net/Makefile
include $(PHIOLA)/src/dfilter/Makefile

build: libphiola.$(SO) \
		$(EXES) \
		$(MODS)

strip-debug: libphiola.$(SO).debug \
		phiola$(DOTEXE).debug \
		$(EXES:.exe=.exe.debug) \
		$(MODS:.$(SO)=.$(SO).debug)
%.debug: %
	$(OBJCOPY) --only-keep-debug $< $@
	$(STRIP) $<
	$(OBJCOPY) --add-gnu-debuglink=$@ $<
	touch $@

app:
	$(MKDIR) $(APP_DIR) $(APP_DIR)/mod
	$(CP) phiola$(DOTEXE) libphiola.$(SO) \
		$(APP_DIR)/
	chmod 644 $(APP_DIR)/libphiola.$(SO)
	$(CP) $(PHIOLA)/LICENSE \
		$(PHIOLA)/README.md \
		$(APP_DIR)/

	$(CP) $(MODS) $(APP_DIR)/mod/
ifneq "$(LIBS3)" ""
	$(CP) $(LIBS3) $(APP_DIR)/mod/
endif
	$(CP) $(PHIOLA)/src/tui/help.txt $(APP_DIR)/mod/tui-help.txt
	chmod 644 $(APP_DIR)/mod/*.$(SO)

	$(CP) $(PHIOLA)/src/net/client.pem $(APP_DIR)/mod/http-client.pem
	$(SUBMAKE) app-gui

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

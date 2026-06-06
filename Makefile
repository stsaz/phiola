# phiola Makefile

ROOT_DIR := ..
PHIOLA := $(ROOT_DIR)/phiola
FFSYS := $(ROOT_DIR)/ffsys
FFBASE := $(ROOT_DIR)/ffbase
APP_DIR := phiola-2

include $(FFBASE)/conf.mk
SYS := $(OS)

# COMPILER

CFLAGS += -DFFBASE_MEM_ASSERT \
	-MMD -MP \
	-I$(FFBASE) \
	-Wall -Wextra -Wno-unused-parameter -Wno-multichar \
	-fPIC -fvisibility=hidden \
	-g
ifeq "$(DEBUG)" "1"
	CFLAGS += -DFF_DEBUG -O0 -Werror -Wno-deprecated-declarations
else
	CFLAGS += -O3 -fno-strict-aliasing
endif
ifeq "$(ASAN)" "1"
	CFLAGS += -fsanitize=address
	LINKFLAGS += -fsanitize=address
endif
CFLAGS += $(CFLAGS_USER)
CFLAGS_BASE := $(CFLAGS)
CFLAGS += -I$(PHIOLA)/src -I$(FFSYS)
CXXFLAGS := -std=c++11 $(CFLAGS) -fno-exceptions -fno-rtti
CFLAGS := -std=c99 $(CFLAGS)
LINKFLAGS += $(LINKFLAGS_USER)
ifeq "$(OS)" "windows"
	LINKFLAGS += -lws2_32
endif
LINK_DL :=
ifeq "$(OS)" "linux"
	LINK_DL := -ldl
endif

# MODULES

default: build
ifneq "$(DEBUG)" "1"
	$(SUBMAKE) strip-debug
endif
	$(SUBMAKE) app

-include $(wildcard *.d)

%.o: $(PHIOLA)/src/%.c
	$(C) $(CFLAGS) $< -o $@
%.o: $(FFBASE)/ffbase/%.c
	$(C) $(CFLAGS) -DFFBASE_OPT_SIZE $< -o $@

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

include $(PHIOLA)/src/tui/Makefile
include $(PHIOLA)/src/gui/Makefile
include $(PHIOLA)/src/net/Makefile
include $(PHIOLA)/src/dfilter/Makefile

ifeq "$(TARGETS)" ""
override TARGETS := libphiola.$(SO) $(EXES) $(MODS)
endif
build: $(TARGETS)

strip-debug: $(addsuffix .debug,$(TARGETS))
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
		$(PHIOLA)/phiola-example.conf \
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

PKG_DEBUG_NAME := phiola-$(PKG_VER)-$(OS)-$(PKG_ARCH)-debug.$(PKG_EXT)
$(PKG_DEBUG_NAME):
	$(PKG_PACKER) $@ *.debug
package-debug: $(PKG_DEBUG_NAME)

INST_ROOT := install-root

PKG_DEB := phiola_$(PKG_VER)_$(CPU).deb
deb: $(PKG_DEB)
$(PKG_DEB): $(INST_ROOT) $(PHIOLA)/installer/deb/control
	mkdir -p $(INST_ROOT)/DEBIAN
	sed -e "s/__PHIOLA_VERSION__/$(PKG_VER)/" \
		-e "s/__ARCH__/$(CPU)/" \
		$(PHIOLA)/installer/deb/control > $(INST_ROOT)/DEBIAN/control
	dpkg-deb -v --build $(INST_ROOT) $@
	rm -rf $(INST_ROOT)/DEBIAN

rpm: $(INST_ROOT) $(PHIOLA)/installer/rpm/nfpm.yaml
	sed -e "s/__PHIOLA_VERSION__/$(PKG_VER)/" \
		-e "s/__ARCH__/$(PKG_ARCH)/" \
		$(PHIOLA)/installer/rpm/nfpm.yaml > $(INST_ROOT)/nfpm.yaml
	cd $(INST_ROOT) && nfpm pkg --config nfpm.yaml --packager rpm --target ..
	rm $(INST_ROOT)/nfpm.yaml

$(INST_ROOT): $(APP_DIR)
	mkdir -p \
		$@/opt \
		$@/usr/bin \
		$@/usr/share/applications \
		$@/usr/share/bash-completion/completions \
		$@/usr/share/icons/hicolor/scalable/apps
	cp -arv $(APP_DIR) $@/opt/
	ln -sf /opt/phiola-2/phiola $@/usr/bin/phiola
	rm $@/opt/phiola-2/mod/gui/phiola.desktop
	cp -av $(PHIOLA)/src/gui/res/phiola.desktop $@/usr/share/applications/
	cp -av $(PHIOLA)/res/phiola.svg $@/usr/share/icons/hicolor/scalable/apps/
ifeq "$(CPU)" "amd64"
	./phiola __bash_completion > $@/usr/share/bash-completion/completions/phiola
endif

setup_exe:
	$(MAKE) -f ../installer/exe/Makefile

release: default
	$(SUBMAKE) package
	$(SUBMAKE) package-debug
ifeq "$(OS)" "linux"
	$(SUBMAKE) deb
	$(SUBMAKE) rpm
else ifeq "$(WIN_INSTALLER)" "1"
	$(SUBMAKE) setup_exe
endif

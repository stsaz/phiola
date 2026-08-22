# Android project makefile

# Undefine built-in rules, suffixes and variables
MAKEFLAGS += -Rr

# Set utils
SUBMAKE := $(MAKE) -f $(firstword $(MAKEFILE_LIST))
MKDIR := mkdir -p
RM := rm -rf
CP := cp -au
LS := ls -l

ANDROID_HOME ?= /Android
# ANDROID_BT_VER :=
# ANDROID_NDK_ROOT :=
# ADB_FLAGS := -d

CPU := amd64
LIB_DIR := libs/x86_64
ifeq "$(CPU)" "arm64"
	LIB_DIR := libs/arm64-v8a
endif
APK_ID := com.github.stsaz.phiola
APK := gradle/outputs/apk/release/phiola-release-unsigned.apk
ifeq "$(DEBUG)" "1"
	APK_ID := com.github.stsaz.phiola.debug
	APK := gradle/outputs/apk/debug/phiola-debug.apk
endif

default: libs
	$(SUBMAKE) apk


# Build .apk

GRADLE := ./gradlew
GRADLE_BUILD := assembleRelease
ifeq "$(DEBUG)" "1"
	GRADLE_BUILD := assembleDebug
endif

apk:
#	cd $(PHIOLA)/android && $(GRADLE) wrapper --gradle-version 7.5.1 \
#		--gradle-distribution-sha256-sum f6b8596b10cce501591e92f229816aa4046424f3b24d771751b06779d58c8ec4
	cd $(PHIOLA)/android && $(GRADLE) $(GRADLE_BUILD) \
		-PbuildDir=$(CURDIR)/gradle \
		-PjniLibsDir=$(CURDIR)/libs

clean-java:
	cd $(PHIOLA)/android && $(GRADLE) clean

apk-list:
	unzip -l $(APK)


# Sign .apk

APK_KEY_STORE :=
APK_VER := test
APK_SIGNED := phiola-$(APK_VER)-android-$(CPU).apk

sign:
	$(ANDROID_HOME)/build-tools/$(ANDROID_BT_VER)/zipalign -v -f 4 $(APK) $(APK_SIGNED)
	$(ANDROID_HOME)/build-tools/$(ANDROID_BT_VER)/apksigner sign \
		--ks $(APK_KEY_STORE) \
		--ks-pass env:APK_KEY_PASS \
		$(APK_SIGNED)
	$(ANDROID_HOME)/build-tools/$(ANDROID_BT_VER)/apksigner verify $(APK_SIGNED)

install-signed:
	$(ANDROID_HOME)/platform-tools/adb $(ADB_FLAGS) install -r $(APK_SIGNED)


# Upload and install .apk on Android device

install:
	$(ANDROID_HOME)/platform-tools/adb $(ADB_FLAGS) install -r $(APK)

uninstall:
	$(ANDROID_HOME)/platform-tools/adb $(ADB_FLAGS) uninstall $(APK_ID)

build-install: build
	$(MAKE) install


# Show logs

log:
	$(ANDROID_HOME)/platform-tools/adb $(ADB_FLAGS) logcat -v time | grep --color=auto --line-buffered '/phiola'

log-error:
	$(ANDROID_HOME)/platform-tools/adb $(ADB_FLAGS) logcat -v time | grep -E --color=auto --line-buffered '/phiola|W/|E/'

log-clear:
	$(ANDROID_HOME)/platform-tools/adb $(ADB_FLAGS) logcat -c

log-bt:
	$(ANDROID_HOME)/platform-tools/adb $(ADB_FLAGS) logcat -v time | $(ANDROID_NDK_ROOT)/ndk-stack -sym $(LIB_DIR)


# Directories
ROOT_DIR := ../..
PHIOLA := $(ROOT_DIR)/phiola
FFBASE := $(ROOT_DIR)/ffbase
FFSYS := $(ROOT_DIR)/ffsys

COMPILER := clang
include $(FFBASE)/conf.mk
include andk.mk
SYS := android

CFLAGS := $(A_CFLAGS)
CFLAGS += -DFFBASE_MEM_ASSERT
CFLAGS += -MMD -MP
CFLAGS += -I$(FFBASE)
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-for-loop-analysis
CFLAGS += -g
CFLAGS += -fPIC
ifeq "$(DEBUG)" "1"
	CFLAGS += -DFF_DEBUG -O0 -Werror -Wno-deprecated-declarations
else
	CFLAGS += -O3 -fno-strict-aliasing -fvisibility=hidden
endif
CFLAGS += $(CFLAGS_USER)
CFLAGS_BASE := $(CFLAGS)
CFLAGS += -I$(PHIOLA)/src -I$(FFSYS)
LINKFLAGS := $(A_LINKFLAGS)

%.o: $(FFBASE)/ffbase/%.c
	$(C) $(CFLAGS) -DFFBASE_OPT_SIZE $< -o $@

-include $(wildcard *.d)

include $(PHIOLA)/src/core/Makefile
include $(PHIOLA)/src/adev/Makefile
include $(PHIOLA)/src/afilter/Makefile
include $(PHIOLA)/src/format/Makefile
include $(PHIOLA)/src/acodec/Makefile
include $(PHIOLA)/src/net/Makefile
include $(PHIOLA)/src/dfilter/Makefile

ifeq "$(TARGETS)" ""
override TARGETS := libphiola.so $(MODS)
endif
libs: $(TARGETS)
	$(MKDIR) $(LIB_DIR)
	$(SUBMAKE) strip-debug
	cp -u $(LIBS3) *.so *.debug \
		$(LIB_DIR)
	@for M in $(MODS); do mv "$(LIB_DIR)/$$M" "$(LIB_DIR)/lib$$M"; done

	$(MKDIR) $(PHIOLA)/android/phiola/src/main/assets
	$(CP) $(PHIOLA)/src/net/client.pem $(PHIOLA)/android/phiola/src/main/assets/http-client.pem


strip-debug: $(addsuffix .debug,$(TARGETS))
%.debug: %
	$(OBJCOPY) --only-keep-debug $< $@
	$(STRIP) $<
	$(OBJCOPY) --add-gnu-debuglink=$@ $<
	touch $@

PKG_ARCH := $(CPU)
PKG_PACKER := tar -c --owner=0 --group=0 --numeric-owner -v --zstd -f
PKG_EXT := tar.zst
PKG_DEBUG_NAME := phiola-$(APK_VER)-android-$(PKG_ARCH)-debug.$(PKG_EXT)
$(PKG_DEBUG_NAME): $(wildcard $(LIB_DIR)/*.debug)
	$(PKG_PACKER) $@ $+
package-debug: $(PKG_DEBUG_NAME)

release: default
	$(SUBMAKE) package-debug
	$(SUBMAKE) sign

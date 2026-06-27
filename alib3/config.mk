# Base settings for Makefile-s

include ../../ffbase/conf.mk

PHI_CF := -fpic -fvisibility=hidden -g
ifneq "$(DEBUG)" "1"
	PHI_CF += -O3
endif
CFLAGS += $(PHI_CF)
CXXFLAGS += $(PHI_CF)

PHI_LF := -fuse-ld=lld $(LINK_INSTALLNAME_LOADERPATH) -lm -static-libgcc
ifneq "$(DEBUG)" "1"
	PHI_LF += -s
endif
LINKFLAGS += $(PHI_LF)
LINKXXFLAGS += $(PHI_LF) -static-libstdc++

SYS := $(OS)
ifeq "$(SYS)" "android"
	include ../android/andk.mk
	CFLAGS := $(PHI_CF) $(A_CFLAGS)
	CXXFLAGS := $(PHI_CF) $(A_CFLAGS)
	LINKFLAGS := $(PHI_LF) $(A_LINKFLAGS)
	LINKXXFLAGS := $(PHI_LF) $(A_LINKFLAGS)
endif

CURL := curl -L
UNTAR_BZ2 := tar -x --no-same-owner -f
UNTAR_GZ := tar -x --no-same-owner -f
UNTAR_XZ := tar -x --no-same-owner -f
UNTAR_ZST := tar -x --zstd --no-same-owner -f
UNZIP := unzip

# Base settings for Makefile-s

include $(ALIB3)/../../ffbase/conf.mk

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
PHI_LFXX := $(PHI_LF) -static-libstdc++
LINKFLAGS += $(PHI_LF)
LINKXXFLAGS += $(PHI_LFXX)

SYS := $(OS)
ifeq "$(SYS)" "android"
	include ../android/andk.mk
	CFLAGS := $(PHI_CF) $(A_CFLAGS)
	CXXFLAGS := $(PHI_CF) $(A_CFLAGS)
	LINKFLAGS := $(PHI_LF) $(A_LINKFLAGS)
	LINKXXFLAGS := $(PHI_LFXX) $(A_LINKFLAGS)
endif

CURL := curl -L
UNTAR := tar --no-same-owner -xf
UNZIP := unzip

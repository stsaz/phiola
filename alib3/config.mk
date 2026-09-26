# Base settings for Makefile-s

include $(ALIB3)/../../ffbase/conf.mk

PHI_CF := -fpic -fvisibility=hidden -g
ifneq "$(DEBUG)" "1"
	PHI_CF += -O3
endif
CFLAGS += $(PHI_CF)
CXXFLAGS += $(PHI_CF)

PHI_LF =
PHI_LFXX =
ifneq "$(OS)" "apple"
	PHI_LF += -fuse-ld=lld -static-libgcc
	PHI_LFXX += -static-libstdc++
endif
PHI_LF += $(LINK_INSTALLNAME_LOADERPATH) -lm
ifneq "$(DEBUG)" "1"
	PHI_LF += -s
endif
PHI_LFXX += $(PHI_LF)
PHI_LF_TMP := $(LINKFLAGS)
PHI_LFXX_TMP := $(LINKXXFLAGS)
LINKFLAGS = $(PHI_LF_TMP) $(PHI_LF)
LINKXXFLAGS = $(PHI_LFXX_TMP) $(PHI_LFXX)

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

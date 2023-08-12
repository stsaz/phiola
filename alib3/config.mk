# Base settings for Makefile-s

include ../../ffbase/test/makeconf

CFLAGS := -fpic -fvisibility=hidden
CFLAGS += -O3
CXXFLAGS := $(CFLAGS)

LINKFLAGS = -fpic $(LINK_INSTALLNAME_LOADERPATH) -lm
LINKFLAGS += -s
ifeq "$(COMPILER)" "gcc"
	LINKFLAGS += -static-libgcc
endif
LINKXXFLAGS = $(LINKFLAGS)
ifeq "$(COMPILER)" "gcc"
	ifeq "$(OS)" "linux"
		LINKXXFLAGS += -static-libstdc++
	else
		LINKXXFLAGS += -static
	endif
endif

# Set compiler and append compiler & linker flags for Android
SYS := $(OS)
ifeq "$(SYS)" "android"
	include ../android/andk.mk
	CFLAGS += $(A_CFLAGS)
	CXXFLAGS += $(A_CFLAGS)
	LINKFLAGS += $(A_LINKFLAGS)
endif

CURL := curl -L
UNTAR_BZ2 := tar -x --no-same-owner -f
UNTAR_GZ := tar -x --no-same-owner -f
UNTAR_XZ := tar -x --no-same-owner -f
UNTAR_ZST := tar -x --zstd --no-same-owner -f
UNZIP := unzip

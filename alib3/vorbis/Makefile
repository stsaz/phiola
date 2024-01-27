# libvorbis, libvorbisenc

include ../config.mk

VER := 1.3.7
URL := http://downloads.xiph.org/releases/vorbis/libvorbis-$(VER).tar.xz
PKG := $(notdir $(URL))
DIR := libvorbis-$(VER)

default: $(DIR)
	$(MAKE) libvorbis-phi.$(SO) libvorbisenc-phi.$(SO)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	$(UNTAR_XZ) $(PKG)
	cd $(DIR) && cat ../*.patch | patch -p1
	touch $(DIR)

# build
CFLAGS += \
	-I$(DIR)/lib -I$(DIR)/include -I../ogg/libogg-1.3.3/include \
	-DUSE_MEMORY_H -DVORB_EXP
ifeq ($(OS),linux)
	CFLAGS += -DHAVE_ALLOCA_H=1 -ffast-math
else ifeq ($(OS),freebsd)
	CFLAGS += -ffast-math
endif

SRC := \
	$(DIR)/lib/mdct.c \
	$(DIR)/lib/smallft.c \
	$(DIR)/lib/block.c \
	$(DIR)/lib/envelope.c \
	$(DIR)/lib/window.c \
	$(DIR)/lib/lsp.c \
	$(DIR)/lib/lpc.c \
	$(DIR)/lib/analysis.c \
	$(DIR)/lib/synthesis.c \
	$(DIR)/lib/psy.c \
	$(DIR)/lib/info.c \
	$(DIR)/lib/floor1.c \
	$(DIR)/lib/floor0.c \
	$(DIR)/lib/res0.c \
	$(DIR)/lib/mapping0.c \
	$(DIR)/lib/registry.c \
	$(DIR)/lib/codebook.c \
	$(DIR)/lib/sharedbook.c \
	$(DIR)/lib/lookup.c \
	$(DIR)/lib/bitrate.c
OBJ := vorbis-phi.o $(SRC:.c=.o)

SRC_ENC := $(DIR)/lib/vorbisenc.c
OBJ_ENC := vorbisenc-phi.o $(SRC_ENC:.c=.o)

%.o: %.c
	$(C) $(CFLAGS) $< -o $@

_:=
ifeq "$(OS)" "linux"
	LINKFLAGS_VORBIS := -Wl,--exclude-libs,ALL
endif
libvorbis-phi.$(SO): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINKFLAGS_VORBIS) ../ogg/libogg.a -o $@

libvorbisenc-phi.$(SO): $(OBJ_ENC) libvorbis-phi.$(SO)
	$(LINK) -shared $(OBJ_ENC) $(LINKFLAGS) -L. -lvorbis-phi -o $@

clean:
	$(RM) $(OBJ) $(OBJ_ENC) $(DIR)

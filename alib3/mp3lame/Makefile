# libmp3lame

include ../config.mk

VER := 3.100
# https://sourceforge.net/projects/lame/files/lame/3.100/lame-3.100.tar.gz/download
URL := https://github.com/stsaz/phiola/raw/alib3/alib3/mp3lame/lame-$(VER).tar.gz
MD5SUM := 83e260acbe4389b54fe08e0bdbf7cddb
PKG := $(ALIB3)/mp3lame/$(notdir $(URL))
DIR := lame-$(VER)
LIB := libmp3lame-phi.$(SO)

default: $(DIR)/config.h
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	echo "$(MD5SUM) *$(PKG)" | md5sum -c -
	$(UNTAR_GZ) $(PKG)
	touch $@

_=
ifeq "$(CPU)" "arm64"
	CONFIGURE_FLAGS := --host=aarch64-linux-gnu
endif
ifeq "$(SYS)" "android"
	CONFIGURE_ENV := CC="$(C_DIR)/clang" CFLAGS="$(A_CFLAGS)"
endif
$(DIR)/config.h: | $(DIR)
	cd $(DIR) && $(CONFIGURE_ENV) ./configure --disable-decoder $(CONFIGURE_FLAGS)
	cat $@

# build
CFLAGS += \
	-DHAVE_CONFIG_H \
	-I$(DIR) -I$(DIR)/include -I$(DIR)/libmp3lame \
	-ffast-math -funroll-loops

SRC := \
	$(DIR)/libmp3lame/bitstream.c \
	$(DIR)/libmp3lame/encoder.c \
	$(DIR)/libmp3lame/fft.c \
	$(DIR)/libmp3lame/gain_analysis.c \
	$(DIR)/libmp3lame/lame.c \
	$(DIR)/libmp3lame/newmdct.c \
	$(DIR)/libmp3lame/presets.c \
	$(DIR)/libmp3lame/psymodel.c \
	$(DIR)/libmp3lame/quantize.c \
	$(DIR)/libmp3lame/quantize_pvt.c \
	$(DIR)/libmp3lame/reservoir.c \
	$(DIR)/libmp3lame/set_get.c \
	$(DIR)/libmp3lame/tables.c \
	$(DIR)/libmp3lame/takehiro.c \
	$(DIR)/libmp3lame/util.c \
	$(DIR)/libmp3lame/vbrquantize.c \
	$(DIR)/libmp3lame/VbrTag.c \
	$(DIR)/libmp3lame/version.c

ifeq "$(CPU)" "amd64"
	SRC += $(DIR)/libmp3lame/vector/xmm_quantize_sub.c
endif

OBJ := lame-phi.o $(SRC:.c=.o)

lame-phi.o: $(ALIB3)/mp3lame/lame-phi.c
	$(C) $(CFLAGS) $< -o $@

%.o: %.c
	$(C) $(CFLAGS) $< -o $@

$(DIR)/libmp3lame/vector/xmm_quantize_sub.o: $(DIR)/libmp3lame/vector/xmm_quantize_sub.c
	$(C) $(CFLAGS) -msse $< -o$@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

clean:
	$(RM) $(OBJ) $(DIR)

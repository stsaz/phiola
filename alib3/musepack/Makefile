# libmpc

include ../config.mk

VER := r475
URL := https://files.musepack.net/source/musepack_src_$(VER).tar.gz
PKG := $(notdir $(URL))
DIR := musepack_src_$(VER)
LIB := libmusepack-phi.$(SO)

default: $(DIR)
	$(MAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	$(UNTAR_GZ) $(PKG)
	cd $(DIR) && cat ../*.patch | patch -p1
	touch $(DIR)

# build
CFLAGS += \
	-DMPC_API="" \
	-I$(DIR) -I$(DIR)/include -I$(DIR)/libmpcenc -I$(DIR)/libmpcpsy -I$(DIR)/libwavformat

SRC_ENCLIB := \
	$(DIR)/common/crc32.c \
	$(DIR)/common/huffman-bcl.c \
	$(DIR)/lib/mpcenc/analy_filter.c \
	$(DIR)/lib/mpcenc/bitstream.c \
	$(DIR)/lib/mpcenc/encode_sv7.c \
	$(DIR)/lib/mpcenc/huffsv7.c \
	$(DIR)/lib/mpcenc/quant.c

SRC_DECLIB := \
	$(DIR)/libmpcdec/huffman.c \
	$(DIR)/libmpcdec/mpc_bits_reader.c \
	$(DIR)/libmpcdec/mpc_decoder.c \
	$(DIR)/libmpcdec/mpc_demux.c \
	$(DIR)/libmpcdec/requant.c \
	$(DIR)/libmpcdec/streaminfo.c \
	$(DIR)/libmpcdec/synth_filter.c

SRC_COMM := \
	$(DIR)/common/tags.c

SRC_PSY := \
	$(DIR)/libmpcpsy/ans.c \
	$(DIR)/libmpcpsy/cvd.c \
	$(DIR)/libmpcpsy/fft4g.c \
	$(DIR)/libmpcpsy/fft_routines.c \
	$(DIR)/libmpcpsy/profile.c \
	$(DIR)/libmpcpsy/psy.c \
	$(DIR)/libmpcpsy/psy_tab.c

SRC_WF := \
	$(DIR)/libwavformat/input.c \
	$(DIR)/libwavformat/output.c

SRC_DEC := \
	$(DIR)/mpcdec/mpcdec.c \
	$(DIR)/libmpcdec/mpc_reader.c

SRC_ENC := \
	$(DIR)/common/fastmath.c \
	$(DIR)/mpcenc/keyboard.c \
	$(DIR)/mpcenc/mpcenc.c \
	$(DIR)/mpcenc/pipeopen.c \
	$(DIR)/mpcenc/stderr.c \
	$(DIR)/mpcenc/wave_in.c \
	$(DIR)/mpcenc/winmsg.c

OBJ := mpc-phi.o $(SRC_DECLIB:.c=.o)
OBJ_DEC := $(SRC_DEC:.c=.o) $(SRC_COMM:.c=.o) $(SRC_DECLIB:.c=.o) $(SRC_WF:.c=.o)
OBJ_ENC := $(SRC_ENC:.c=.o) $(SRC_COMM:.c=.o) $(SRC_ENCLIB:.c=.o) $(SRC_PSY:.c=.o)

%.o: %.c
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

mpcdec-bin: $(OBJ_DEC)
	$(LINK) $+ $(LINKFLAGS) -o $@

mpcenc-bin: $(OBJ_ENC)
	$(LINK) $+ $(LINKFLAGS) -o $@

clean:
	$(RM) $(OBJ) $(OBJ_ENC) $(OBJ_DEC) $(DIR)

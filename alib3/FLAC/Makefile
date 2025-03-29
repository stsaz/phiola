# libFLAC

include ../config.mk

VER := 1.5.0
URL := https://github.com/xiph/flac/archive/refs/tags/$(VER).zip
MD5SUM := 49560e2be82d559c6ac0751fe51e43bf
PKG := $(ALIB3)/FLAC/$(notdir $(URL))
DIR := flac-$(VER)
LIB := libFLAC-phi.$(SO)

default: $(DIR)/config.h
	#cd $(DIR) && sed -n '/libFLAC_sources = /,/libFLAC_la_SOURCES = /p' src/libFLAC/Makefile.in >phi.mk
	#cd $(DIR) && make -j8 V=1 >LOG
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	echo "$(MD5SUM) *$(PKG)" | md5sum -c -
	$(UNZIP) $(PKG)
	touch $@

# config
CONFIGURE_FLAGS := --host=x86_64
ifeq "$(OS)" "windows"
	CONFIGURE_FLAGS := --host=x86_64-w64-mingw32
else ifeq "$(CPU)" "arm64"
	ifeq "$(SYS)" "android"
		CONFIGURE_FLAGS := --host=aarch64
	else
		CONFIGURE_FLAGS := --host=aarch64-linux-gnu
	endif
else ifeq "$(CPU)" "arm"
	CONFIGURE_FLAGS := --host=armv7
endif
ifeq "$(SYS)" "android"
	CONFIGURE_ENV := CC="$(C_DIR)/clang" CFLAGS="$(A_CFLAGS)"
endif
$(DIR)/config.h: | $(DIR)
	cd $(DIR) && bash autogen.sh
	cd $(DIR) && $(CONFIGURE_ENV) ./configure --disable-ogg --disable-multithreading $(CONFIGURE_FLAGS)
	# cd $(DIR) && git init && git add include src/libFLAC && git commit -m init && git am $(ALIB3)/FLAC/*.patch
	cat $(ALIB3)/FLAC/*.patch | patch -d $(DIR) -p1
	cat $@
	touch $@

# build
CFLAGS += \
	-I$(ALIB3)/FLAC -I$(DIR) -I$(DIR)/src/libFLAC/include -I$(DIR)/include \
	-DHAVE_CONFIG_H -D_FORTIFY_SOURCE=2 -DDLL_EXPORT \
	-Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes -Waggregate-return -Wcast-align -Wnested-externs -Wshadow -Wundef -Wmissing-declarations -Winline -Wdeclaration-after-statement \
	-fassociative-math -fno-signed-zeros -fno-trapping-math -freciprocal-math -funroll-loops
ifeq "$(OS)" "linux"
	CFLAGS += -fstack-protector-strong
endif

ifeq "$(OS)" "windows"
	LINKFLAGS += -Wl,-static -lssp
endif

SRC := \
	bitmath.c \
	bitreader.c \
	bitwriter.c \
	cpu.c \
	crc.c \
	fixed.c \
	fixed_intrin_sse2.c \
	fixed_intrin_ssse3.c \
	fixed_intrin_sse42.c \
	fixed_intrin_avx2.c \
	float.c \
	format.c \
	lpc.c \
	lpc_intrin_sse2.c \
	lpc_intrin_sse41.c \
	lpc_intrin_avx2.c \
	lpc_intrin_fma.c \
	lpc_intrin_neon.c \
	md5.c \
	memory.c \
	stream_decoder.c \
	stream_encoder.c \
	stream_encoder_intrin_sse2.c \
	stream_encoder_intrin_ssse3.c \
	stream_encoder_intrin_avx2.c \
	stream_encoder_framing.c \
	window.c
OBJ := FLAC-phi.o $(addprefix $(DIR)/src/libFLAC/,$(SRC:.c=.o))

%.o: $(ALIB3)/FLAC/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: %.c
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@
ifeq "$(SYS)" "android"
	$(C_DIR)/llvm-objdump -T $@
endif

clean:
	$(RM) $(OBJ) $(DIR)

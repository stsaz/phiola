# libmpg123

include ../config.mk

VER := 1.32.10
URL := http://mpg123.de/download/mpg123-$(VER).tar.bz2
MD5SUM := f6d1a69dbf340c8d889b64772e8e0a61
PKG := $(ALIB3)/mpg123/$(notdir $(URL))
DIR := mpg123-$(VER)
LIB := libmpg123-phi.$(SO)

default: $(DIR)/src/config.h
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	echo "$(MD5SUM) *$(PKG)" | md5sum -c -
	$(UNTAR_BZ2) $(PKG)

_:=
ifeq "$(CPU)" "amd64"
	ifeq "$(SYS)" "android"
		CONFIGURE_FLAGS := --host=x86_64
	else ifeq "$(OS)" "windows"
		CONFIGURE_FLAGS := --host=x86_64-w64-mingw32
	endif
else ifeq "$(CPU)" "x86"
	ifeq "$(OS)" "windows"
		CONFIGURE_FLAGS := --host=i686-w64-mingw32
	endif
else ifeq "$(CPU)" "arm64"
	ifeq "$(SYS)" "android"
		CONFIGURE_FLAGS := --host=aarch64
	else
		CONFIGURE_FLAGS := --host=aarch64-linux-gnu
	endif
	CONFIGURE_FLAGS += --with-cpu=aarch64
else ifeq "$(CPU)" "arm"
	CONFIGURE_FLAGS := --host=armv7 --with-cpu=neon
endif
ifeq "$(SYS)" "android"
	CONFIGURE_ENV := CC="$(C_DIR)/clang" CFLAGS="$(A_CFLAGS)"
endif
CONFIGURE_FLAGS += \
	--disable-modules    \
	--disable-id3v2      \
	--disable-string     \
	--disable-icy        \
	--disable-messages   \
	--disable-16bit      \
	--disable-8bit       \
	--disable-32bit      \
	--disable-equalizer  \
	--disable-layer1     \
	--disable-layer2     \
	--disable-largefile  \
	--disable-components \
	--enable-libmpg123

$(DIR)/src/config.h: | $(DIR)
	cd $(DIR) && $(CONFIGURE_ENV) ./configure $(CONFIGURE_FLAGS)
	cat $(ALIB3)/mpg123/*.patch | patch -d $(DIR) -p1
	cat $@
	cd $(DIR) && make phi

# build
CFLAGS += \
	-I. -I$(DIR) -I$(DIR)/src -I$(DIR)/src/include -I$(DIR)/src/libmpg123 -I$(DIR)/src/compat \
	-DMPG123_EXPORT="" -DHAVE_CONFIG_H \
	-DOPT_MULTI -DOPT_GENERIC -DOPT_GENERIC_DITHER -DREAL_IS_FLOAT \
	-fomit-frame-pointer -funroll-all-loops -finline-functions -ffast-math

SRC := \
	$(DIR)/src/compat/compat_str.c
SRC += \
	$(DIR)/src/libmpg123/parse.c \
	$(DIR)/src/libmpg123/frame.c \
	$(DIR)/src/libmpg123/format.c \
	$(DIR)/src/libmpg123/dct64.c \
	$(DIR)/src/libmpg123/id3.c \
	$(DIR)/src/libmpg123/optimize.c \
	$(DIR)/src/libmpg123/readers.c \
	$(DIR)/src/libmpg123/tabinit.c \
	$(DIR)/src/libmpg123/libmpg123.c \
	$(DIR)/src/libmpg123/index.c \
	$(DIR)/src/libmpg123/layer3.c \
	$(DIR)/src/libmpg123/dither.c \
	$(DIR)/src/libmpg123/synth_real.c \
	$(DIR)/src/libmpg123/lfs_wrap.c \
	$(DIR)/src/libmpg123/feature.c \
	$(DIR)/src/libmpg123/ntom.c

ifeq "$(CPU)" "x86"

	CFLAGS += -msse2 \
		-DOPT_I386 -DOPT_I586 -DOPT_I586_DITHER -DOPT_MMX \
		-DOPT_3DNOW -DOPT_3DNOW_VINTAGE -DOPT_3DNOWEXT -DOPT_3DNOWEXT_VINTAGE \
		-DOPT_SSE -DOPT_SSE_VINTAGE -DNOXFERMEM -DNEWOLD_WRITE_SAMPLE

	SRC_AS := \
		$(DIR)/src/libmpg123/dct64_i386.S \
		$(DIR)/src/libmpg123/synth_i586.S \
		$(DIR)/src/libmpg123/synth_i586_dither.S \
		$(DIR)/src/libmpg123/dct64_mmx.S \
		$(DIR)/src/libmpg123/tabinit_mmx.S \
		$(DIR)/src/libmpg123/synth_mmx.S \
		$(DIR)/src/libmpg123/synth_3dnow.S \
		$(DIR)/src/libmpg123/dct64_3dnow.S \
		$(DIR)/src/libmpg123/dct36_3dnow.S \
		$(DIR)/src/libmpg123/dct64_3dnowext.S \
		$(DIR)/src/libmpg123/synth_3dnowext.S \
		$(DIR)/src/libmpg123/dct36_3dnowext.S \
		$(DIR)/src/libmpg123/dct64_sse_float.S \
		$(DIR)/src/libmpg123/synth_sse_float.S \
		$(DIR)/src/libmpg123/synth_stereo_sse_float.S \
		$(DIR)/src/libmpg123/synth_sse_s32.S \
		$(DIR)/src/libmpg123/synth_stereo_sse_s32.S \
		$(DIR)/src/libmpg123/dct36_sse.S \
		$(DIR)/src/libmpg123/dct64_sse.S \
		$(DIR)/src/libmpg123/synth_sse.S \
		$(DIR)/src/libmpg123/getcpuflags.S

else ifeq "$(CPU)" "amd64"

	CFLAGS += -DOPT_X86_64 -DOPT_AVX

	SRC_AS := \
		$(DIR)/src/libmpg123/getcpuflags_x86_64.S \
		$(DIR)/src/libmpg123/dct36_x86_64.S \
		$(DIR)/src/libmpg123/synth_stereo_x86_64_float.S \
		$(DIR)/src/libmpg123/synth_stereo_x86_64_accurate.S \
		$(DIR)/src/libmpg123/dct64_x86_64_float.S \
		$(DIR)/src/libmpg123/synth_x86_64_float.S \
		$(DIR)/src/libmpg123/synth_x86_64_accurate.S \
		$(DIR)/src/libmpg123/dct36_avx.S \
		$(DIR)/src/libmpg123/dct64_avx_float.S \
		$(DIR)/src/libmpg123/synth_stereo_avx_float.S \
		$(DIR)/src/libmpg123/synth_stereo_avx_accurate.S

else ifeq "$(CPU)" "arm64"

	CFLAGS += -DOPT_NEON64

	SRC += $(DIR)/src/libmpg123/getcpuflags_arm.c

	SRC_AS := \
		$(DIR)/src/libmpg123/check_neon.S \
		$(DIR)/src/libmpg123/dct36_neon64.S \
		$(DIR)/src/libmpg123/dct64_neon64_float.S \
		$(DIR)/src/libmpg123/synth_neon64_float.S \
		$(DIR)/src/libmpg123/synth_stereo_neon64_float.S \
		$(DIR)/src/libmpg123/synth_neon64_accurate.S \
		$(DIR)/src/libmpg123/synth_stereo_neon64_accurate.S

else ifeq "$(CPU)" "arm"

	CFLAGS += -DOPT_NEON

	SRC += $(DIR)/src/libmpg123/getcpuflags_arm.c

	SRC_AS := \
		$(DIR)/src/libmpg123/check_neon.S \
		$(DIR)/src/libmpg123/dct36_neon.S \
		$(DIR)/src/libmpg123/dct64_neon_float.S \
		$(DIR)/src/libmpg123/synth_neon_float.S \
		$(DIR)/src/libmpg123/synth_stereo_neon_float.S \
		$(DIR)/src/libmpg123/synth_neon_accurate.S \
		$(DIR)/src/libmpg123/synth_stereo_neon_accurate.S

endif

OBJ := mpg123-phi.o \
	$(SRC:.c=.o) \
	$(SRC_AS:.S=.o)

%.o: $(ALIB3)/mpg123/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: %.c
	$(C) $(CFLAGS) $< -o $@

%.o: %.S
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@
ifeq "$(SYS)" "android"
	$(C_DIR)/llvm-objdump -T $@
else ifeq "$(CPU)" "amd64"
ifeq "$(OS)" "linux"
	ldd $@
	objdump -T $@
endif
endif
	ls -l $@

clean-obj:
	$(RM) $(OBJ) $(LIB)

clean:
	$(RM) $(OBJ) $(DIR)

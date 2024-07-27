# libopus

include ../config.mk

VER := 1.5.2
URL := https://github.com/xiph/opus/archive/refs/tags/v$(VER).zip
PKG := $(ALIB3)/opus/opus-$(VER).zip
DIR := opus-$(VER)
LIB := libopus-phi.$(SO)

default: $(DIR)/config.h
	cd $(DIR) && sed -n '/CELT_SOURCES = /,/src\/mlp_data.c/p' Makefile.in >phi.mk
	#cd $(DIR) && make -j8 V=1 >LOG
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	$(UNZIP) $(PKG)

# config
_:=
ifeq "$(CPU)" "arm64"
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
	cd $(DIR) && autoreconf -isf
	cd $(DIR) && $(CONFIGURE_ENV) ./configure $(CONFIGURE_FLAGS)
	$(SED) 's/PACKAGE_VERSION "unknown"/PACKAGE_VERSION "$(VER)"/' $(DIR)/config.h
	cat $@
	touch $@

# build
CFLAGS += \
	-DHAVE_CONFIG_H -DOPUS_EXPORT= \
	-I$(DIR) -I$(DIR)/include -I$(DIR)/celt -I$(DIR)/silk -I$(DIR)/silk/float -I$(DIR)/silk/fixed \
	-Wall -Wextra -Wcast-align -Wnested-externs -Wshadow -Wstrict-prototypes
ifeq "$(OS)" "linux"
	CFLAGS += -fstack-protector-strong
endif

-include $(DIR)/phi.mk

CPUFAM := x86
ifeq "$(CPU)" "arm64"
	CPUFAM := arm
else ifeq "$(CPU)" "arm"
	CPUFAM := arm
endif

ifeq "$(CPUFAM)" "x86"

	CELT_SOURCES += $(CELT_SOURCES_X86_RTCD) $(CELT_SOURCES_SSE) $(CELT_SOURCES_SSE2) $(CELT_SOURCES_SSE4_1) $(CELT_SOURCES_AVX2)
	SILK_SOURCES += $(SILK_SOURCES_X86_RTCD) $(SILK_SOURCES_SSE4_1) $(SILK_SOURCES_AVX2) $(SILK_SOURCES_FLOAT_AVX2)

	CFLAGS_SSE4_1 := -msse4.1
	SSE4_1_OBJ := $(CELT_SOURCES_SSE4_1:.c=.o) $(SILK_SOURCES_SSE4_1:.c=.o)
	SSE4_1_OBJ := $(addprefix $(DIR)/,$(SSE4_1_OBJ))
$(SSE4_1_OBJ): CFLAGS += $(CFLAGS_SSE4_1)

	CFLAGS_AVX2 := -mavx -mfma -mavx2
	AVX2_OBJ := $(CELT_SOURCES_AVX2:.c=.o) $(SILK_SOURCES_AVX2:.c=.o) $(SILK_SOURCES_FLOAT_AVX2:.c=.o)
	AVX2_OBJ := $(addprefix $(DIR)/,$(AVX2_OBJ))
$(AVX2_OBJ): CFLAGS += $(CFLAGS_AVX2)

else ifeq "$(CPUFAM)" "arm"

	CELT_SOURCES += $(CELT_SOURCES_ARM_RTCD) $(CELT_SOURCES_ARM_NEON_INTR)
	SILK_SOURCES += $(SILK_SOURCES_ARM_RTCD) $(SILK_SOURCES_ARM_NEON_INTR)

	# CELT_SOURCES_ARM_ASM
	# CELT_AM_SOURCES_ARM_ASM
	# CELT_SOURCES_ARM_NE10

endif

SILK_SOURCES += $(SILK_SOURCES_FLOAT)
OPUS_SOURCES += $(OPUS_SOURCES_FLOAT)

SRC := $(CELT_SOURCES) $(SILK_SOURCES) $(OPUS_SOURCES)
OBJ := opus-phi.o $(addprefix $(DIR)/,$(SRC:.c=.o))

%.o: $(ALIB3)/opus/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: %.c
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

clean:
	$(RM) $(OBJ) $(DIR)

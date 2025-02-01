# libsoxr

include ../config.mk

VER := edbdb4081db14eb21729b57c4f2c813514ad0259
URL := https://github.com/dofuuz/soxr/archive/soxr-$(VER).zip
MD5SUM := e5294f21a1d98860f4d94fe40ebe7f7c
PKG := $(ALIB3)/soxr/soxr-$(VER).zip
DIR := soxr-$(VER)
LIB := libsoxr-phi.$(SO)

default: $(DIR)/soxr-config.h
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	echo "$(MD5SUM) *$(PKG)" | md5sum -c -
	$(UNZIP) $(PKG)

$(DIR)/soxr-config.h: | $(DIR)
	cd $(DIR) && cmake -Wno-dev -DCMAKE_BUILD_TYPE="Release" $(CMAKE_FLAGS) .
	# cat $(ALIB3)/soxr/*.patch | patch -d $(DIR) -p1
	# cd $(DIR) && make VERBOSE=1
	$(SED) 's/AVUTIL_FOUND 1/AVUTIL_FOUND 0/' $(DIR)/soxr-config.h
ifeq "$(CPU)" "amd64"
else ifeq "$(CPU)" "x86"
else
	$(SED) 's/WITH_CR64S 1/WITH_CR64S 0/' $(DIR)/soxr-config.h
endif
	cat $(DIR)/soxr-config.h

# build
CFLAGS += \
	-I$(DIR) -I$(DIR)/src \
	-DNDEBUG -DSOXR_LIB -D_DEFAULT_SOURCE
ifeq "$(CPU)" "arm"
	# w/a for `cannot locate symbol "fetestexcept"`
	CFLAGS += -DSOXR_NO_FETESTEXCEPT
endif
ifeq "$(OS)" "windows"
	CFLAGS += -DSOXR_DLL -Dsoxr_EXPORTS -D_USE_MATH_DEFINES
else
	CFLAGS += -DSOXR_VISIBILITY
endif

SRC := \
	$(DIR)/src/cr.c \
	$(DIR)/src/cr32.c \
	$(DIR)/src/cr64.c \
	$(DIR)/src/data-io.c \
	$(DIR)/src/dbesi0.c \
	$(DIR)/src/fft4g32.c \
	$(DIR)/src/fft4g64.c \
	$(DIR)/src/filter.c \
	$(DIR)/src/soxr.c \
	$(DIR)/src/vr32.c

SRC_SIMD32 := \
	$(DIR)/src/cr32s.c \
	$(DIR)/src/pffft32s.c \
	$(DIR)/src/util32s.c

SRC_AVX := \
	$(DIR)/src/cr64s.c \
	$(DIR)/src/pffft64s.c \
	$(DIR)/src/util64s.c

SRC += $(SRC_SIMD32)

ifeq "$(CPU)" "amd64"
	SRC += $(SRC_AVX)
$(SRC_AVX:.c=.o): CFLAGS += -mavx
endif

OBJ := soxr-phi.o $(SRC:.c=.o)

%.o: $(ALIB3)/soxr/%.c $(wildcard $(DIR)/src/*.h)
	$(C) $(CFLAGS) $< -o $@

%.o: %.c $(wildcard $(DIR)/src/*.h)
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

clean:
	$(RM) $(OBJ) $(DIR)

# libsoxr

include ../config.mk

VER := 0.1.3
URL := https://github.com/stsaz/phiola/raw/alib3/alib3/soxr/soxr-0.1.3-Source.tar.xz
PKG := $(ALIB3)/soxr/$(notdir $(URL))
DIR := soxr-$(VER)-Source
LIB := libsoxr-phi.$(SO)

default: $(DIR)/soxr-config.h
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	$(UNTAR_XZ) $(PKG)

$(DIR)/soxr-config.h: | $(DIR)
	cd $(DIR) && cmake -Wno-dev -DCMAKE_BUILD_TYPE="Release" $(CMAKE_FLAGS) .
	cat $(ALIB3)/soxr/*.patch | patch -d $(DIR) -p1
	# cd $(DIR) && make VERBOSE=1
	$(SED) 's/AVUTIL_FOUND 1/AVUTIL_FOUND 0/' $(DIR)/soxr-config.h
ifeq "$(CPU)" "amd64"
else ifeq "$(CPU)" "x86"
else
	$(SED) 's/WITH_CR64 1/WITH_CR64 0/' $(DIR)/soxr-config.h
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
ifeq ($(OS),windows)
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

SRC += $(SRC_SIMD32)

ifeq "$(CPU)" "amd64"
	SRC += \
		$(DIR)/src/cr64s.c \
		$(DIR)/src/pffft64s.c \
		$(DIR)/src/util64s.c
endif

OBJ := $(SRC:.c=.o)

%.o: %.c $(wildcard $(DIR)/src/*.h)
	$(C) $(CFLAGS) $< -o $@

$(DIR)/src/cr64s.o: $(DIR)/src/cr64s.c $(wildcard $(DIR)/src/*.h)
	$(C) $(CFLAGS) -mavx $< -o $@

$(DIR)/src/pffft64s.o: $(DIR)/src/pffft64s.c $(wildcard $(DIR)/src/*.h)
	$(C) $(CFLAGS) -mavx $< -o $@

$(DIR)/src/util64s.o: $(DIR)/src/util64s.c $(wildcard $(DIR)/src/*.h)
	$(C) $(CFLAGS) -mavx $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

clean:
	$(RM) $(OBJ) $(DIR)

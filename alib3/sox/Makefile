# libsox

include ../config.mk

VER := 14.4.2
URL := https://github.com/chirlu/sox/archive/refs/tags/sox-$(VER).tar.gz
SHA256SUM := ee871d23d9eae9ea885f071cefe1f31dd0b31e75bd28903d380e090db8a68b70
PKG := $(ALIB3)/sox/sox-$(VER).tar.gz
DIR := sox-sox-$(VER)
LIB := libsox-phi.$(SO)

default: $(DIR)/src/soxconfig.h
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	echo "$(SHA256SUM) *$(PKG)" | sha256sum -c -
	$(UNTAR_GZ) $(PKG)

CONFIGURE_FLAGS := --without-libltdl
ifeq "$(OS)" "windows"
	CONFIGURE_FLAGS += --host=x86_64-w64-mingw32
endif

$(DIR)/src/soxconfig.h: | $(DIR)
	cd $(DIR) && autoreconf -i
	cd $(DIR) && ./configure $(CONFIGURE_FLAGS)
	cat $@
	cat $(ALIB3)/sox/*.patch | patch -d $(DIR) -p1

# build
CFLAGS += \
	-I$(DIR) -I$(DIR)/src

SRC := \
	$(DIR)/src/effects.c \
	$(DIR)/src/effects_i.c \
	$(DIR)/src/xmalloc.c \
	$(DIR)/src/biquad.c \
	$(DIR)/src/biquads.c

OBJ := sox-phi.o $(SRC:.c=.o)

%.o: $(ALIB3)/sox/%.c $(wildcard $(DIR)/src/*.h)
	$(C) $(CFLAGS) $< -o $@

%.o: %.c $(wildcard $(DIR)/src/*.h)
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

clean:
	$(RM) $(OBJ) $(DIR)

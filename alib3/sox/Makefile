# libsox

include ../config.mk

VER := 14.4.2
URL := /sox-$(VER).tar.bz2
MD5SUM := ba804bb1ce5c71dd484a102a5b27d0dd
PKG := $(ALIB3)/sox/sox-$(VER).tar.bz2
DIR := sox-$(VER)
LIB := libsox-phi.$(SO)

default: $(DIR)/src/soxconfig.h
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	echo "$(MD5SUM) *$(PKG)" | md5sum -c -
	$(UNTAR_BZ2) $(PKG)

CONFIGURE_FLAGS := --without-libltdl
ifeq "$(OS)" "windows"
	CONFIGURE_FLAGS += --host=x86_64-w64-mingw32
endif

$(DIR)/src/soxconfig.h: | $(DIR)
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

# libogg.  Used by libvorbis only.

include ../config.mk

VER := 1.3.3
URL := http://downloads.xiph.org/releases/ogg/libogg-$(VER).tar.xz
PKG := $(ALIB3)/ogg/$(notdir $(URL))
DIR := libogg-$(VER)
LIB := libogg.a

default: $(DIR)
	cd $(DIR) && ./configure
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	$(UNTAR_XZ) $(PKG)

# build
CFLAGS += -I$(DIR)/include \
	-fvisibility=default

SRC := $(DIR)/src/bitwise.c
OBJ := $(SRC:.c=.o)

%.o: %.c
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(AR) rcs $@ $+

clean:
	$(RM) $(OBJ) $(DIR)

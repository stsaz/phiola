# libEBUR128

include ../config.mk

VER := 1.2.6
URL := https://github.com/jiixyj/libebur128/archive/refs/tags/v$(VER).zip
SHA256SUM := edd60c1ed2ff45711ee1c372e723a1deda376ba9060c23fcc9262efddaee141b
PKG := $(ALIB3)/EBUR128/$(notdir $(URL))
DIR := libebur128-$(VER)
LIB := libebur128-phi.$(SO)

default: $(DIR)
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	echo "$(SHA256SUM) *$(PKG)" | sha256sum -c -
	$(UNZIP) $(PKG)

# build
CFLAGS += \
	-I$(DIR)/ebur128 -I$(DIR)/ebur128/queue

SRC := $(DIR)/ebur128/ebur128.c
OBJ := ebur128-phi.o $(SRC:.c=.o)

%.o: $(ALIB3)/EBUR128/%.c
	$(C) $(CFLAGS) $< -o $@

%.o: %.c
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

clean:
	$(RM) $(OBJ) $(DIR)

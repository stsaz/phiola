# libwavpack

include ../config.mk

VER := 4.75.0
URL := http://www.wavpack.com/wavpack-$(VER).tar.bz2
MD5SUM := 14e49c65efd0f3b0dea708e176a112d2
PKG := $(ALIB3)/wavpack/$(notdir $(URL))
DIR := wavpack-$(VER)
LIB := libwavpack-phi.$(SO)

default: $(DIR)
	$(SUBMAKE) $(LIB)

# download
$(PKG):
	$(CURL) -o $(PKG) $(URL)

# unpack
$(DIR): $(PKG)
	echo "$(MD5SUM) *$(PKG)" | md5sum -c -
	$(UNTAR_BZ2) $(PKG)
	cat $(ALIB3)/wavpack/*.patch | patch -d $(DIR) -p1

# build
CFLAGS += -I$(ALIB3)/wavpack \
	-DWVPK_EXP -DHAVE___BUILTIN_CLZ=1 -DHAVE___BUILTIN_CTZ=1 \
	-DVER4_ONLY -DNO_SEEKING -DNO_TAGS -DNO_USE_FSTREAMS

SRC := \
	$(DIR)/src/common_utils.c \
	$(DIR)/src/decorr_utils.c \
	$(DIR)/src/entropy_utils.c \
	$(DIR)/src/open_utils.c \
	$(DIR)/src/read_words.c \
	$(DIR)/src/unpack.c \
	$(DIR)/src/unpack_floats.c \
	$(DIR)/src/unpack_utils.c

ifeq "$(CPU)" "x86"
	CFLAGS += -DOPT_ASM_X86=1
	SRC_AS := $(DIR)/src/unpack_x86.s

else ifeq "$(CPU)" "amd64"
	CFLAGS += -DOPT_ASM_X64=1
	SRC_AS := $(DIR)/src/unpack_x64.s
endif

OBJ := $(SRC:.c=.o)
OBJ += $(SRC_AS:.s=.o)

%.o: %.c
	$(C) $(CFLAGS) $< -o $@

%.o: %.s
	$(C) $(CFLAGS) $< -o $@

$(LIB): $(OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) -o $@

clean:
	$(RM) $(OBJ) $(DIR)

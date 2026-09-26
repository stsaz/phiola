# phiola data filters

# Inherit:
# C
# CFLAGS
# CPU
# LINK
# LINK_RPATH_ORIGIN
# LINKFLAGS
# OS
# PHIOLA
# ROOT_DIR
# SO
# SYS

# Modify:
# LIBS3
# MODS

FFPACK := $(ROOT_DIR)/ffpack
FFPACK_BIN := ffpack
FFPACK_FLAGS := FFPACK=$(FFPACK) OS=$(OS) SYS=$(SYS) COMPILER=$(COMPILER) CPU=$(CPU)
ifeq "$(SYS)" "android"
else ifeq "$(OS)" "apple"
else
	FFPACK_FLAGS += LINKFLAGS_USER='-fuse-ld=lld'
endif

MODS += zstd.$(SO)
LIBS3 += $(FFPACK_BIN)/libzstd-ffpack.$(SO)

$(FFPACK_BIN)/libzstd-ffpack.$(SO): $(FFPACK)/zstd/Makefile | $(FFPACK_BIN)
	$(MAKE)  -C $(FFPACK_BIN)  -f $(FFPACK)/zstd/Makefile  -I $(FFPACK)/zstd  $(FFPACK_FLAGS)

%.o: $(PHIOLA)/src/dfilter/%.c
	$(C) $(CFLAGS) -I$(FFPACK) $< -o $@

zstd.$(SO): zstd.o | $(FFPACK_BIN)/libzstd-ffpack.$(SO)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(FFPACK_BIN) -lzstd-ffpack -o $@


$(FFPACK_BIN):
	$(MKDIR) $@

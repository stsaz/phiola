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
FFPACK_BIN := $(FFPACK)/_$(OS)-$(CPU)
ifeq "$(SYS)" "android"
	FFPACK_BIN := $(FFPACK)/_android-$(CPU)
endif

MODS += zstd.$(SO)
LIBS3 += $(FFPACK_BIN)/libzstd-ffpack.$(SO)

%.o: $(PHIOLA)/src/dfilter/%.c
	$(C) $(CFLAGS) -I$(FFPACK) $< -o $@

zstd.$(SO): zstd.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(FFPACK_BIN) -lzstd-ffpack -o $@

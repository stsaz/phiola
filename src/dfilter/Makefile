# phiola data filters

# Inherit:
# C
# CFLAGS
# CPU
# DEPS
# LINK
# LINK_RPATH_ORIGIN
# LINKFLAGS
# OS
# PHIOLA
# ROOT_DIR
# SO

# Modify:
# LIBS3
# MODS

FFPACK := $(ROOT_DIR)/ffpack
FFPACK_BIN := $(FFPACK)/_$(OS)-$(CPU)

MODS += zstd.$(SO)
LIBS3 += $(FFPACK_BIN)/libzstd-ffpack.$(SO)

%.o: $(PHIOLA)/src/dfilter/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/dfilter/zstd-*.h)
	$(C) $(CFLAGS) -I$(FFPACK) $< -o $@

zstd.$(SO): zstd.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) -L$(FFPACK_BIN) -lzstd-ffpack -o $@

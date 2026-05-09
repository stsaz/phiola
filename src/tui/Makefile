# phiola TUI

# Inherit:
# C
# CFLAGS
# LINK
# LINKFLAGS
# PHIOLA
# SO

# Modify:
# MODS

AVPACK := $(ROOT_DIR)/avpack

MODS += tui.$(SO)

%.o: $(PHIOLA)/src/tui/%.c
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@

tui.$(SO): tui.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@

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

MODS += tui.$(SO)

%.o: $(PHIOLA)/src/tui/%.c
	$(C) $(CFLAGS) $< -o $@

tui.$(SO): tui.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@

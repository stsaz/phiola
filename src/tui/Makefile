# phiola TUI

AVPACK := $(ROOT_DIR)/avpack

MODS += tui.$(SO)

%.o: $(PHIOLA)/src/tui/%.c
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@

tui.$(SO): tui.o
	$(LINK) -shared $+ $(LINKFLAGS) -lm -o $@

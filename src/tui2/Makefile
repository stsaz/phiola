# phiola: TUI-ncurses builder

ifeq "$(OS)" "linux"
	MODS += tui2.$(SO)
endif

tui2.o: $(PHIOLA)/src/tui2/tui2.c
	$(C) $(CFLAGS) $< -o $@

tui2.$(SO): tui2.o
	$(LINK) -shared $+ $(LINKFLAGS) -lncursesw -lm -o $@

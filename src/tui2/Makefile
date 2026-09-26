# phiola: TUI-ncurses builder

ifeq "$(OS)" "linux"
	MODS += tui2.$(SO)
	LF_CURSES := -lncursesw
else ifeq "$(OS)" "apple"
	MODS += tui2.$(SO)
	LF_CURSES := -lncurses
else ifeq "$(OS)" "windows"
	MODS += tui2.$(SO)
	CF_CURSES := -IPDCurses-3.9
	LIBCURSES := PDCurses-3.9/wincon/pdcurses.dll
	LF_CURSES := $(LIBCURSES)
	LIBS3 += $(LIBCURSES)
endif

tui2.o: $(PHIOLA)/src/tui2/tui2.c | $(LIBCURSES)
	$(C) $(CFLAGS) $(CF_CURSES) $< -o $@

tui2.$(SO): tui2.o $(LIBCURSES)
	$(LINK) -shared $+ $(LINKFLAGS) $(LF_CURSES) -lm -o $@

$(LIBCURSES): $(PHIOLA)/src/tui2/pdcurses.mk
	$(MAKE) -f $<

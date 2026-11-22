# phiola executor

AVPACK := $(ROOT_DIR)/avpack

EXES += phiola$(DOTEXE)

CFLAGS_EXE := $(CFLAGS) -I$(AVPACK)
%.o: $(PHIOLA)/src/exe/%.c
	$(C) $(CFLAGS_EXE) $< -o $@

_:=
ifeq "$(OS)" "windows"
	EXE_COFF := exe.coff
endif
exe.coff: $(PHIOLA)/res/exe.rc \
		$(PHIOLA)/res/exe.manifest \
		$(PHIOLA)/res/phiola.ico
	$(WINDRES) $< $@

phiola$(DOTEXE): main.o \
		$(EXE_COFF) \
		libphiola.$(SO)
	$(LINK) $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) $(LINK_DL) -o $@

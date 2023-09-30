# phiola executor

# Inherit:
# C
# CFLAGS
# DEPS
# DOTEXE
# LINK
# LINK_DL
# LINK_RPATH_ORIGIN
# LINKFLAGS
# OS
# PHIOLA
# SO
# WINDRES

# Modify:
# EXES

EXES += phiola$(DOTEXE)

%.o: $(PHIOLA)/src/exe/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/exe/*.h) \
		$(wildcard $(PHIOLA)/src/util/*.h)
	$(C) $(CFLAGS) $< -o $@

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
		core.$(SO)
	$(LINK) $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) $(LINK_DL) -o $@

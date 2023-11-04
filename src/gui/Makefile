# phiola GUI

# Inherit:
# APP_DIR
# C
# CFLAGS
# CP
# CXX
# CXXFLAGS
# DEBUG_GUI
# LINK_DL
# LINK_PTHREAD
# LINK_RPATH_ORIGIN
# LINKFLAGS
# LINKXX
# MKDIR
# OS
# PHIOLA
# ROOT_DIR
# SO
# WINDRES

# Modify:
# EXES
# MODS

FFGUI := $(ROOT_DIR)/ffgui

MODS += gui.$(SO)
ifeq "$(OS)" "windows"
	CFLAGS_GUI := -Wno-missing-field-initializers
	LINKFLAGS_GUI := -lshell32 -luxtheme -lcomctl32 -lcomdlg32 -lgdi32 -lole32 -luuid
	FFGUI_OBJ := \
		ffgui-winapi.o \
		ffgui-winapi-loader.o
else
	CFLAGS_GUI := -Wno-free-nonheap-object -Wno-deprecated-declarations \
		`pkg-config --cflags gtk+-3.0`
	LINKFLAGS_GUI := `pkg-config --libs gtk+-3.0` \
		$(LINK_PTHREAD) -lm
	FFGUI_OBJ := \
		ffgui-gtk.o \
		ffgui-gtk-loader.o
endif
ifeq "$(DEBUG_GUI)" "1"
	CFLAGS_GUI += -DFFGUI_DEBUG
endif
CFLAGS_GUI += -I$(FFGUI)
CFLAGS_GUI := $(CFLAGS) $(CFLAGS_GUI)
CXXFLAGS_GUI := $(CXXFLAGS) $(CFLAGS_GUI)
LINKFLAGS_GUI := $(LINKFLAGS) $(LINKFLAGS_GUI)

gui-mod.o: $(PHIOLA)/src/gui/mod.c
	$(C) $(CFLAGS) $< -o $@
gui-%.o: $(PHIOLA)/src/gui/%.c
	$(C) $(CFLAGS_GUI) $< -o $@
gui-%.o: $(PHIOLA)/src/gui/%.cpp
	$(CXX) $(CXXFLAGS_GUI) $< -o $@
%.o: $(FFGUI)/ffgui/gtk/%.c
	$(C) $(CFLAGS_GUI) $< -o $@
%.o: $(FFGUI)/ffgui/winapi/%.c
	$(C) $(CFLAGS_GUI) $< -o $@

GUI_RES :=
ifeq "$(OS)" "windows"
	GUI_RES := gui.coff
endif
gui.coff: $(PHIOLA)/src/gui/res/dll.rc \
		$(wildcard $(PHIOLA)/src/gui/res/*.ico)
	$(WINDRES) $< $@

gui.$(SO): gui-mod.o \
		gui-gui.o \
		gui-main.o \
		gui-dialogs.o \
		$(GUI_RES) \
		$(FFGUI_OBJ)
	$(LINKXX) -shared $+ $(LINKFLAGS_GUI) $(LINK_DL) -o $@

_:=
ifeq "$(OS)" "windows"
	GUIEXE_O := gui-exe.coff
	EXES += phiola-gui.exe
endif
gui-exe.coff: $(PHIOLA)/res/exe.rc \
		$(PHIOLA)/res/exe.manifest \
		$(PHIOLA)/res/phiola.ico
	$(WINDRES) $< $@

phiola-gui.exe: gui-windows.o \
		$(GUIEXE_O) \
		libphiola.$(SO)
	$(LINK) $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) $(LINK_DL) -mwindows -o $@

app-gui:
	$(MKDIR) $(APP_DIR)/mod/gui
ifeq "$(OS)" "windows"
	$(CP) phiola-gui.exe $(APP_DIR)/
	$(CP) $(PHIOLA)/src/gui/ui-winapi.conf $(APP_DIR)/mod/gui/ui.conf
	$(CP) $(PHIOLA)/src/gui/lang_*.conf \
		$(PHIOLA)/src/gui/res/theme-*.conf \
		$(APP_DIR)/mod/gui/
	sed -i 's/_/\&/' $(APP_DIR)/mod/gui/lang_*.conf
	unix2dos $(APP_DIR)/mod/gui/*.conf
else
	$(CP) $(PHIOLA)/src/gui/ui-gtk.conf $(APP_DIR)/mod/gui/ui.conf
	$(CP) $(PHIOLA)/src/gui/lang_*.conf \
		$(PHIOLA)/res/phiola.ico \
		$(PHIOLA)/src/gui/res/*.png \
		$(PHIOLA)/src/gui/res/phiola.desktop \
		$(APP_DIR)/mod/gui/
endif
	chmod 644 $(APP_DIR)/mod/gui/*

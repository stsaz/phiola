# phiola GUI

# Inherit:
# APP_DIR
# C
# CFLAGS
# CP
# CXX
# CXXFLAGS
# DEBUG_GUI
# DEPS
# LINK_DL
# LINK_PTHREAD
# LINKFLAGS
# LINKXX
# MKDIR
# OS
# PHIOLA
# SO

# Modify:
# MODS

MODS += gui.$(SO)
ifeq "$(OS)" "windows"
	FFGUI_HDR := $(wildcard $(PHIOLA)/src/util/gui-winapi/*.h)
	CFLAGS_GUI := -Wno-missing-field-initializers
	LINKFLAGS_GUI := -lshell32 -luxtheme -lcomctl32 -lcomdlg32 -lgdi32 -lole32 -luuid
	FFGUI_OBJ := ffgui-winapi.o ffgui-winapi-loader.o
else
	FFGUI_HDR := $(wildcard $(PHIOLA)/src/util/gui-gtk/*.h)
	CFLAGS_GUI := -Wno-free-nonheap-object -Wno-deprecated-declarations `pkg-config --cflags gtk+-3.0`
	LINKFLAGS_GUI := `pkg-config --libs gtk+-3.0` $(LINK_PTHREAD) -lm
	FFGUI_OBJ := ffgui-gtk.o ffgui-gtk-loader.o
endif
ifeq "$(DEBUG_GUI)" "1"
	CFLAGS_GUI += -DFFGUI_DEBUG
endif
CFLAGS_GUI := $(CFLAGS) $(CFLAGS_GUI)
CXXFLAGS_GUI := $(CXXFLAGS) $(CFLAGS_GUI)
LINKFLAGS_GUI := $(LINKFLAGS) $(LINKFLAGS_GUI)
gui-mod.o: $(PHIOLA)/src/gui/mod.c $(DEPS) \
		$(PHIOLA)/src/gui/mod.h \
		$(PHIOLA)/src/gui/track.h \
		$(PHIOLA)/src/gui/track-convert.h
	$(C) $(CFLAGS) $< -o $@
gui.o: $(PHIOLA)/src/gui/gui.c $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/gui/gui.h \
		$(PHIOLA)/src/gui/mod.h \
		$(PHIOLA)/src/gui/actions.h
	$(C) $(CFLAGS_GUI) $< -o $@
gui-main.o: $(PHIOLA)/src/gui/main.cpp $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/gui/gui.h \
		$(PHIOLA)/src/gui/mod.h \
		$(PHIOLA)/src/gui/actions.h
	$(CXX) $(CXXFLAGS_GUI) $< -o $@
gui-dialogs.o: $(PHIOLA)/src/gui/dialogs.cpp $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/gui/gui.h \
		$(PHIOLA)/src/gui/mod.h \
		$(wildcard $(PHIOLA)/src/gui/*.hpp)
	$(CXX) $(CXXFLAGS_GUI) $< -o $@
ffgui-gtk.o: $(PHIOLA)/src/util/gui-gtk/ffgui-gtk.c $(DEPS) $(FFGUI_HDR)
	$(C) $(CFLAGS_GUI) $< -o $@
ffgui-gtk-loader.o: $(PHIOLA)/src/util/gui-gtk/ffgui-gtk-loader.c $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/util/conf-*.h
	$(C) $(CFLAGS_GUI) $< -o $@
ffgui-winapi.o: $(PHIOLA)/src/util/gui-winapi/ffgui-winapi.c $(DEPS) $(FFGUI_HDR)
	$(C) $(CFLAGS_GUI) $< -o $@
ffgui-winapi-loader.o: $(PHIOLA)/src/util/gui-winapi/ffgui-winapi-loader.c $(DEPS) $(FFGUI_HDR) \
		$(PHIOLA)/src/util/conf-*.h
	$(C) $(CFLAGS_GUI) $< -o $@
gui.$(SO): gui-mod.o \
		gui.o \
		gui-main.o \
		gui-dialogs.o \
		$(FFGUI_OBJ)
	$(LINKXX) -shared $+ $(LINKFLAGS_GUI) $(LINK_DL) -o $@

app-gui:
	$(MKDIR) $(APP_DIR)/mod/gui
ifeq "$(OS)" "windows"
	$(CP) $(PHIOLA)/src/gui/ui-winapi.conf $(APP_DIR)/mod/gui/ui.conf
	$(CP) $(PHIOLA)/src/gui/lang_*.conf \
		$(APP_DIR)/mod/gui/
	sed -i 's/_/\&/' $(APP_DIR)/mod/gui/lang_*.conf
	unix2dos $(APP_DIR)/mod/gui/*.conf
else
	$(CP) $(PHIOLA)/src/gui/ui-gtk.conf $(APP_DIR)/mod/gui/ui.conf
	$(CP) $(PHIOLA)/src/gui/lang_*.conf \
		$(PHIOLA)/src/gui/res/*.ico $(PHIOLA)/src/gui/res/phiola.desktop \
		$(APP_DIR)/mod/gui/
endif
	chmod 644 $(APP_DIR)/mod/gui/*

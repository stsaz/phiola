# phiola Core

# Inherit:
# C
# CFLAGS
# LINK
# LINK_DL
# LINK_PTHREAD
# LINKFLAGS
# OS
# PHIOLA
# SO
# SYS

LINKFLAGS_CORE := $(LINKFLAGS)
CORE_O := \
	core.o \
	auto.o \
	dir-read.o \
	file.o\
	queue.o \
	track.o
ifeq "$(OS)" "windows"
	CORE_O += sys-sleep-win.o
endif
ifeq "$(SYS)" "android"
	CORE_O += phiola-jni.o
	LINKFLAGS_CORE += -llog -landroid
endif
CORE_O += \
	sort.o \
	str-format.o

libphiola.$(SO): $(CORE_O)
	$(LINK) -shared $+ $(LINKFLAGS_CORE) $(LINK_PTHREAD) $(LINK_DL) -o $@

CFLAGS_CORE := $(CFLAGS) $(CFLAGS_OPT) -DFFBASE_OPT_SIZE
ifdef PHI_VERSION_STR
	CFLAGS_CORE += -DPHI_VERSION_STR=\"$(PHI_VERSION_STR)\"
endif
%.o: $(PHIOLA)/src/core/%.c
	$(C) $(CFLAGS_CORE) $< -o $@
%.o: $(PHIOLA)/src/jni/%.c
	$(C) $(CFLAGS_CORE) $< -o $@

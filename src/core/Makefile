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

CORE_O := \
	core.o \
	auto.o \
	dir-read.o \
	file.o\
	qu.o \
	track.o
ifeq "$(OS)" "windows"
	CORE_O += sys-sleep-win.o
endif
ifeq "$(SYS)" "android"
	CORE_O += phiola-jni.o
endif
CORE_O += \
	sort.o \
	str-format.o

libphiola.$(SO): $(CORE_O)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_PTHREAD) $(LINK_DL) -o $@

CFLAGS_CORE := $(CFLAGS) -DFFBASE_OPT_SIZE
%.o: $(PHIOLA)/src/core/%.c
	$(C) $(CFLAGS_CORE) $< -o $@
%.o: $(PHIOLA)/src/queue/%.c
	$(C) $(CFLAGS_CORE) $< -o $@
%.o: $(PHIOLA)/src/jni/%.c
	$(C) $(CFLAGS_CORE) $< -o $@

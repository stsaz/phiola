# phiola network module

# Inherit:
# AVPACK
# C
# CFLAGS
# CFLAGS_BASE
# DEBUG
# DEPS
# FFOS
# LINK
# LINK_PTHREAD
# LINKFLAGS
# PHIOLA
# ROOT_DIR
# SO

# Modify:
# MODS

NETMILL := $(ROOT_DIR)/netmill

MODS += http.$(SO)
CFLAGS_NETMILL := $(CFLAGS_BASE) -I$(NETMILL)/src
ifeq "$(DEBUG)" "1"
	CFLAGS_NETMILL += -DNML_ENABLE_LOG_EXTRA
endif
%.o: $(PHIOLA)/src/net/%.c $(DEPS) \
		$(wildcard $(PHIOLA)/src/net/*.h)
	$(C) $(CFLAGS) -I$(NETMILL)/src $< -o $@
icy.o: $(PHIOLA)/src/net/icy.c $(DEPS)
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
netmill-http-filters.o: $(PHIOLA)/src/net/http-filters.c \
		$(PHIOLA)/src/net/http-bridge.h \
		$(wildcard $(NETMILL)/src/http-client/*.h)
	$(C) $(CFLAGS_NETMILL) -I$(PHIOLA)/src -I$(FFOS) $< -o $@
netmill-http-client.o: $(NETMILL)/src/http-client/oclient.c \
		$(wildcard $(NETMILL)/src/http-client/*.h)
	$(C) $(CFLAGS_NETMILL) -I$(FFOS) $< -o $@
http.$(SO): http.o \
		icy.o \
		netmill-http-client.o \
		netmill-http-filters.o
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_PTHREAD) -o $@

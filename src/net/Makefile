# phiola network module

# Inherit:
# C
# CFLAGS
# CFLAGS_BASE
# DEBUG
# FFOS
# LINK
# LINK_PTHREAD
# LINKFLAGS
# PHIOLA
# ROOT_DIR
# SO

# Modify:
# LIBS3
# MODS

AVPACK := $(ROOT_DIR)/avpack
NETMILL := $(ROOT_DIR)/netmill

MODS += http.$(SO)

HTTP_OBJ := http.o \
	icy.o \
	netmill-http-client.o \
	netmill-http-filters.o

CFLAGS_NETMILL := $(CFLAGS_BASE) -I$(NETMILL)/src
ifeq "$(DEBUG)" "1"
	CFLAGS_NETMILL += -DNML_ENABLE_LOG_EXTRA
endif

ifeq "$(PHI_HTTP_SSL)" "0"
	CFLAGS += -DPHI_HTTP_NO_SSL
	CFLAGS_NETMILL += -DPHI_HTTP_NO_SSL
else
	HTTP_OBJ += ffssl.o
	CFLAGS_NETMILL += -I$(NETMILL)/3pt/openssl/openssl-3.1.3/include
	LINKFLAGS_NETMILL := -L$(NETMILL)/3pt/_$(OS)-$(CPU)
ifeq "$(OS)" "windows"
	LINKFLAGS_NETMILL += -lssl-3-x64 -lcrypto-3-x64
	LIBS3 += \
		$(NETMILL)/3pt/_$(OS)-$(CPU)/libssl-3-x64.$(SO) \
		$(NETMILL)/3pt/_$(OS)-$(CPU)/libcrypto-3-x64.$(SO)
else
	LINKFLAGS_NETMILL += -lssl -lcrypto
	LIBS3 += \
		$(NETMILL)/3pt/_$(OS)-$(CPU)/libssl.$(SO).3 \
		$(NETMILL)/3pt/_$(OS)-$(CPU)/libcrypto.$(SO).3
endif
endif

http.$(SO): $(HTTP_OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) $(LINK_PTHREAD) $(LINKFLAGS_NETMILL) -o $@

icy.o: $(PHIOLA)/src/net/icy.c
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@
%.o: $(PHIOLA)/src/net/%.c
	$(C) $(CFLAGS) -I$(NETMILL)/src $< -o $@

netmill-http-filters.o: $(PHIOLA)/src/net/http-filters.c
	$(C) $(CFLAGS_NETMILL) -I$(PHIOLA)/src -I$(FFOS) $< -o $@
netmill-http-client.o: $(NETMILL)/src/http-client/client.c
	$(C) $(CFLAGS_NETMILL) -I$(FFOS) $< -o $@
ffssl.o: $(NETMILL)/src/util/ffssl.c
	$(C) $(CFLAGS_NETMILL) -I$(FFOS) $< -o $@

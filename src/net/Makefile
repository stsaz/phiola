# phiola network module

# Inherit:
# C
# CFLAGS
# CFLAGS_BASE
# DEBUG
# FFSYS
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
	netmill-core-cache.o \
	netmill-http-client.o \
	netmill-http-filters.o

HTTP_OBJ += server.o \
	netmill-tcp-listener.o \
	netmill-http-server.o \
	netmill-http-server-conn.o

CFLAGS_NETMILL := $(CFLAGS_BASE) -I$(NETMILL)/src
ifeq "$(DEBUG)" "1"
	CFLAGS_NETMILL += -DNML_ENABLE_LOG_EXTRA
endif

ifeq "$(PHI_HTTP_SSL)" "0"
	CFLAGS += -DPHI_HTTP_NO_SSL
	CFLAGS_NETMILL += -DPHI_HTTP_NO_SSL
else
	HTTP_OBJ += ffssl.o
	CFLAGS_NETMILL += -I$(NETMILL)/3pt/_$(SYS)-$(CPU)/openssl-3.1.3/include
	LINKFLAGS_NETMILL := -L$(NETMILL)/3pt/_$(SYS)-$(CPU)
	ifeq "$(OS)" "windows"
		LINKFLAGS_NETMILL += -lssl-3-x64 -lcrypto-3-x64
		LIBS3 += \
			$(NETMILL)/3pt/_$(OS)-$(CPU)/libssl-3-x64.$(SO) \
			$(NETMILL)/3pt/_$(OS)-$(CPU)/libcrypto-3-x64.$(SO)
	else ifeq "$(SYS)" "android"
		LINKFLAGS_NETMILL += -lssl -lcrypto
		LIBS3 += \
			$(NETMILL)/3pt/_$(SYS)-$(CPU)/libssl.$(SO) \
			$(NETMILL)/3pt/_$(SYS)-$(CPU)/libcrypto.$(SO)
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
	$(C) $(CFLAGS) -I$(NETMILL)/src -DNML_STATIC_LINKING $< -o $@

ifeq "$(DEBUG)" "1"
server.o: CFLAGS += -DNML_ENABLE_LOG_EXTRA
endif

netmill-http-filters.o: $(PHIOLA)/src/net/http-filters.c
	$(C) $(CFLAGS_NETMILL) -I$(PHIOLA)/src -I$(FFSYS) $< -o $@

netmill-http-client.o: $(NETMILL)/src/http-client/client.c
	$(C) $(CFLAGS_NETMILL) -I$(FFSYS) $< -o $@

netmill-http-server.o: $(NETMILL)/src/http-server/server.c
	$(C) $(CFLAGS_NETMILL) -I$(FFSYS) $< -o $@

netmill-http-server-conn.o: $(NETMILL)/src/http-server/conn.c
	$(C) $(CFLAGS_NETMILL) -I$(FFSYS) $< -o $@

netmill-tcp-listener.o: $(NETMILL)/src/core/tcp-listener.c
	$(C) $(CFLAGS_NETMILL) -I$(FFSYS) $< -o $@

netmill-core-cache.o: $(NETMILL)/src/core/cache.c
	$(C) $(CFLAGS_NETMILL) -I$(FFSYS) $< -o $@

ffssl.o: $(NETMILL)/src/util/ffssl.c
	$(C) $(CFLAGS_NETMILL) -I$(FFSYS) $< -o $@

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
NML_3PT := $(NETMILL)/3pt/_$(SYS)-$(CPU)
NML_3PT_FLAGS := OS=$(OS) SYS=$(SYS) COMPILER=$(COMPILER) CPU=$(CPU)
ifneq "$(SYS)" "android"
	NML_3PT_FLAGS += LINKFLAGS_USER='-fuse-ld=lld'
endif

MODS += http.$(SO)

HTTP_OBJ := http.o \
	icy.o \
	netmill-core-cache.o \
	netmill-http-client.o \
	netmill-http-filters.o

ifeq "$(PHI_HTTP_SRV)" "0"
	CFLAGS_HTTP += -DPHI_HTTP_NO_SRV
else
	HTTP_OBJ += server.o \
		netmill-tcp-listener.o \
		netmill-http-server.o \
		netmill-http-server-conn.o
endif

CFLAGS_NETMILL := $(CFLAGS_BASE) -I$(NETMILL)/src
ifeq "$(DEBUG)" "1"
	CFLAGS_NETMILL += -DNML_ENABLE_LOG_EXTRA
endif

ifeq "$(PHI_HTTP_SSL)" "0"
	CFLAGS_HTTP += -DPHI_HTTP_NO_SSL
	CFLAGS_NETMILL += -DPHI_HTTP_NO_SSL
else
	HTTP_OBJ += ffssl.o
	CFLAGS_NETMILL += -I$(NML_3PT)/openssl-3.5.6/include
	LINKFLAGS_NETMILL := -L$(NML_3PT)
	ifeq "$(OS)" "windows"
		LINKFLAGS_NETMILL += -lssl-3-x64 -lcrypto-3-x64
		LIBS3 += \
			$(NETMILL)/3pt/_$(OS)-$(CPU)/libssl-3-x64.$(SO) \
			$(NETMILL)/3pt/_$(OS)-$(CPU)/libcrypto-3-x64.$(SO)
		OPENSSL_SO := $(NML_3PT)/libssl-3-x64.$(SO)

	else ifeq "$(SYS)" "android"
		LINKFLAGS_NETMILL += -lssl -lcrypto
		LIBS3 += \
			$(NETMILL)/3pt/_$(SYS)-$(CPU)/libssl.$(SO) \
			$(NETMILL)/3pt/_$(SYS)-$(CPU)/libcrypto.$(SO)
		OPENSSL_SO := $(NML_3PT)/libssl.$(SO)

	else
		LINKFLAGS_NETMILL += -lssl -lcrypto
		LIBS3 += \
			$(NETMILL)/3pt/_$(OS)-$(CPU)/libssl.$(SO).3 \
			$(NETMILL)/3pt/_$(OS)-$(CPU)/libcrypto.$(SO).3
		OPENSSL_SO := $(NML_3PT)/libssl.$(SO).3
	endif

$(OPENSSL_SO): $(NETMILL)/3pt/openssl/Makefile | $(NML_3PT)
	$(MAKE)  -C $(NML_3PT)  -f ../openssl/Makefile  -I ..  NML_3PT=.. $(NML_3PT_FLAGS)

ffssl.o: $(NETMILL)/src/util/ffssl.c | $(OPENSSL_SO)
	$(C) $(CFLAGS_NETMILL) -I$(FFSYS) $< -o $@

$(NML_3PT):
	$(MKDIR) $@

endif

http.$(SO): $(HTTP_OBJ)
	$(LINK) -shared $+ $(LINKFLAGS) $(LINK_RPATH_ORIGIN) $(LINK_PTHREAD) $(LINKFLAGS_NETMILL) -o $@

icy.o: $(PHIOLA)/src/net/icy.c
	$(C) $(CFLAGS) -I$(AVPACK) $< -o $@

%.o: $(PHIOLA)/src/net/%.c
	$(C) $(CFLAGS) $(CFLAGS_HTTP) -I$(NETMILL)/src -DNML_STATIC_LINKING $< -o $@

server.o: CFLAGS += -I$(AVPACK)
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

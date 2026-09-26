# PDCurses builder

URL := https://github.com/wmcbrine/PDCurses/archive/refs/tags/3.9.zip
PKG := PDCurses-3.9.zip
PKG_SHA256 := 5c2ccab1d8fbf7e2c82eac954444a6760eb949d47d6dc57e09f339d34a50ba79
DIR := PDCurses-3.9
LIB := $(DIR)/wincon/pdcurses.dll

CURL := curl -L -o
CC := clang -target x86_64-w64-mingw32
WINDRES := llvm-windres
STRIP := llvm-strip
SHA256SUM := sha256sum

default: $(LIB)

$(PKG):
	$(CURL) $@ $(URL)

$(DIR): $(PKG)
	echo "$(PKG_SHA256) *$(PKG)" | $(SHA256SUM) -c -
	unzip $(PKG)

$(LIB): $(DIR)
	cd $(DIR)/wincon \
		&& $(MAKE) DLL=Y WIDE=Y UTF8=Y \
		CC="$(CC)" \
		LIBFLAGS="-fuse-ld=lld -shared -o" \
		WINDRES=$(WINDRES)
	$(STRIP) $@

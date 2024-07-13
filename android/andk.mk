# Android NDK environment for 'make'

# Set compiler
C_DIR := $(NDK_DIR)/toolchains/llvm/prebuilt/linux-x86_64/bin
C := $(C_DIR)/clang -c
CXX := $(C_DIR)/clang++ -c
LINK := $(C_DIR)/clang
LINKXX := $(C_DIR)/clang++

# Set target
A_API := 26
A_API32 := $(A_API)
ifeq "$(CPU)" "amd64"
	A_CFLAGS := -target x86_64-none-linux-android$(A_API)
	A_LINKFLAGS += -target x86_64-none-linux-android$(A_API)
else ifeq "$(CPU)" "arm64"
	A_CFLAGS := -target aarch64-none-linux-android$(A_API)
	A_LINKFLAGS += -target aarch64-none-linux-android$(A_API)
else ifeq "$(CPU)" "arm"
	A_CFLAGS := -target armv7-none-linux-androideabi$(A_API32) -mthumb
	A_LINKFLAGS += -target armv7-none-linux-androideabi$(A_API32)
endif

A_CFLAGS += \
	-fPIC -fdata-sections -ffunction-sections -fstack-protector-strong -funwind-tables \
	-no-canonical-prefixes \
	--sysroot $(NDK_DIR)/toolchains/llvm/prebuilt/linux-x86_64/sysroot \
	-D_FORTIFY_SOURCE=2 -DANDROID -DNDEBUG

A_LINKFLAGS += -no-canonical-prefixes \
	-Wl,-no-undefined -Wl,--gc-sections -Wl,--build-id=sha1 -Wl,--no-rosegment
